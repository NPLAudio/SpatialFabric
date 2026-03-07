// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "SpatialFabricManagerActor.h"
#include "SpatialFabricSettings.h"
#include "SpatialObjectRegistry.h"
#include "SpatialStageVolume.h"
#include "Components/BoxComponent.h"
#include "ProtocolRouter.h"
#include "SpatialOSCServerComponent.h"
#include "SpatialOSCClientComponent.h"
#include "EngineUtils.h"

// Adapters
#include "Adapters/ADMOSCAdapter.h"
#include "Adapters/CustomAdapter.h"
#include "Adapters/DS100Adapter.h"
#include "Adapters/SpaceMapGoAdapter.h"
#include "Adapters/TiMaxAdapter.h"

ASpatialFabricManagerActor::ASpatialFabricManagerActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// Must tick after the stage volume (PostPhysics), which itself runs after
	// character movement, so the listener position is always current.
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	ServerComponent    = CreateDefaultSubobject<USpatialOSCServerComponent>(TEXT("ServerComponent"));
	ClientComponent    = CreateDefaultSubobject<USpatialOSCClientComponent>(TEXT("ClientComponent"));
	RegistryComponent  = CreateDefaultSubobject<USpatialObjectRegistry>(TEXT("RegistryComponent"));

	PopulateDefaultAdapterConfigs();
}

// ── AActor overrides ──────────────────────────────────────────────────────

void ASpatialFabricManagerActor::BeginPlay()
{
	Super::BeginPlay();

	// Resolve stage volume and apply visibility early (before early return)
	ResolvedStageVolume = StageVolume.Get();
	if (IsValid(ResolvedStageVolume))
	{
		AddTickPrerequisiteActor(ResolvedStageVolume);
	}
	ApplyStageVisibility();

	const USpatialFabricSettings* Settings = GetDefault<USpatialFabricSettings>();
	const bool bIsPackaged = !GIsEditor;
	if (bIsPackaged && !Settings->bEnableInPackagedBuilds)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SpatialFabric: Networking disabled in packaged build. Enable in Project Settings > Spatial Fabric > Enable In Packaged Builds, or set bEnableInPackagedBuilds=true in DefaultEngine.ini."));
		return;
	}

	// Wire incoming OSC to router
	if (ServerComponent)
	{
		ServerComponent->OnMessageReceived.AddDynamic(this,
			&ASpatialFabricManagerActor::OnOSCMessageReceived);
	}

	// Start server
	StartServer();

	// Build router with all adapters
	Router = MakeUnique<FProtocolRouter>();
	InitializeAdapters();
}

void ASpatialFabricManagerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopServer();
	DisconnectClient();
	if (Router) { Router->ClearAdapters(); }
	Router.Reset();
	Super::EndPlay(EndPlayReason);
}

void ASpatialFabricManagerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Re-resolve stage volume each tick (handles late binding in PIE)
	if (!IsValid(ResolvedStageVolume) && !StageVolume.IsNull())
	{
		ResolvedStageVolume = StageVolume.Get();
		if (IsValid(ResolvedStageVolume))
		{
			AddTickPrerequisiteActor(ResolvedStageVolume);
		}
	}

	ProcessFrame(DeltaTime);
}

// ── Frame processing ──────────────────────────────────────────────────────

void ASpatialFabricManagerActor::ProcessFrame(float DeltaTime)
{
	if (!Router) { return; }

	LastSnapshot =
		RegistryComponent->BuildSnapshot(ObjectBindings, ResolvedStageVolume.Get());

	Router->ProcessFrame(LastSnapshot, ObjectBindings, DeltaTime);
}

// ── Server/Client controls ────────────────────────────────────────────────

void ASpatialFabricManagerActor::StartServer()
{
	if (ServerComponent)
	{
		const USpatialFabricSettings* S = GetDefault<USpatialFabricSettings>();
		ServerComponent->StartListening(TEXT("0.0.0.0"), S->DefaultOSCListenPort);
	}
}

void ASpatialFabricManagerActor::StopServer()
{
	if (ServerComponent) { ServerComponent->StopListening(); }
}

void ASpatialFabricManagerActor::ConnectClient()
{
	if (ClientComponent)
	{
		// Connect to the first enabled OSC adapter's endpoint
		const uint8 ADMKey = (uint8)ESpatialAdapterType::ADMOSC;
		if (const FSpatialAdapterConfig* Config = AdapterConfigs.Find(ADMKey))
		{
			ClientComponent->Connect(Config->TargetIP, Config->TargetPort);
		}
	}
}

void ASpatialFabricManagerActor::DisconnectClient()
{
	if (ClientComponent) { ClientComponent->Disconnect(); }
}

// ── Log ───────────────────────────────────────────────────────────────────

void ASpatialFabricManagerActor::AppendLog(const FSpatialFabricLogEntry& Entry)
{
	MessageLog.Add(Entry);
	while (MessageLog.Num() > MaxLogEntries)
	{
		MessageLog.RemoveAt(0);
	}
}

TArray<FSpatialFabricLogEntry> ASpatialFabricManagerActor::GetRecentLog(int32 Count) const
{
	const int32 Start = FMath::Max(0, MessageLog.Num() - Count);
	return TArray<FSpatialFabricLogEntry>(MessageLog.GetData() + Start,
	                                     MessageLog.Num() - Start);
}

// ── Static factory ────────────────────────────────────────────────────────

ASpatialFabricManagerActor* ASpatialFabricManagerActor::GetOrSpawnManager(
	UObject* WorldContextObject)
{
	if (!WorldContextObject) { return nullptr; }
	UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) { return nullptr; }

	for (TActorIterator<ASpatialFabricManagerActor> It(World); It; ++It)
	{
		return *It;
	}

	return World->SpawnActor<ASpatialFabricManagerActor>();
}

// ── Private helpers ───────────────────────────────────────────────────────

void ASpatialFabricManagerActor::InitializeAdapters()
{
	if (!Router) { return; }
	Router->ClearAdapters();

	// Helper lambda: configure an OSC adapter and register it
	auto RegisterOSC = [&](TSharedPtr<ISpatialProtocolAdapter> Adapter, ESpatialAdapterType Type)
	{
		const uint8 Key = (uint8)Type;
		if (const FSpatialAdapterConfig* Config = AdapterConfigs.Find(Key))
		{
			Adapter->Configure(*Config);
			Adapter->SetClientComponent(ClientComponent);
		}
		// Wire log callback
		Adapter->OnLog = [this](const FSpatialFabricLogEntry& Entry)
		{
			AppendLog(Entry);
		};
		Router->RegisterAdapter(Adapter);
	};

	RegisterOSC(MakeShared<FADMOSCAdapter>(),     ESpatialAdapterType::ADMOSC);
	RegisterOSC(MakeShared<FCustomAdapter>(),     ESpatialAdapterType::Custom);
	RegisterOSC(MakeShared<FDS100Adapter>(),      ESpatialAdapterType::DS100);
	RegisterOSC(MakeShared<FSpaceMapGoAdapter>(), ESpatialAdapterType::SpaceMapGo);
	RegisterOSC(MakeShared<FTiMaxAdapter>(),      ESpatialAdapterType::TiMax);

	// Connect the shared OSC client to the first OSC adapter (user can override)
	ConnectClient();
}

void ASpatialFabricManagerActor::PopulateDefaultAdapterConfigs()
{
	const USpatialFabricSettings* S = GetDefault<USpatialFabricSettings>();

	auto MakeConfig = [&](FString IP, int32 Port, bool bEnabled = false) -> FSpatialAdapterConfig
	{
		FSpatialAdapterConfig C;
		C.TargetIP   = IP;
		C.TargetPort = Port;
		C.SendRateHz = S->DefaultSendRateHz;
		C.bEnabled   = bEnabled;
		return C;
	};

	AdapterConfigs.Add((uint8)ESpatialAdapterType::ADMOSC,
		MakeConfig(TEXT("127.0.0.1"), S->DefaultADMOSCPort, /*bEnabled=*/true));

	AdapterConfigs.Add((uint8)ESpatialAdapterType::DS100,
		MakeConfig(TEXT("127.0.0.1"), S->DefaultDS100Port));

	AdapterConfigs.Add((uint8)ESpatialAdapterType::SpaceMapGo,
		MakeConfig(TEXT("127.0.0.1"), S->DefaultSpaceMapGoPort));

	AdapterConfigs.Add((uint8)ESpatialAdapterType::Custom,
		MakeConfig(TEXT("127.0.0.1"), 9000));

	AdapterConfigs.Add((uint8)ESpatialAdapterType::TiMax,
		MakeConfig(TEXT("127.0.0.1"), 7000));
}

void ASpatialFabricManagerActor::OnOSCMessageReceived(const FString& Address, float Value)
{
	if (Router)
	{
		Router->DispatchIncomingOSC(Address, Value);
	}
}

void ASpatialFabricManagerActor::SendCustomOSC(const FString& Address,
	const TArray<float>& FloatArgs, const TArray<int32>& IntArgs, const TArray<FString>& StringArgs)
{
	if (!ClientComponent) { return; }

	FString PrevIP = ClientComponent->GetTargetIP();
	int32 PrevPort = ClientComponent->GetTargetPort();

	ClientComponent->Connect(CustomTargetIP, CustomTargetPort);
	ClientComponent->SendMessage(Address, FloatArgs, IntArgs, StringArgs);

	// Restore previous connection
	if (!PrevIP.IsEmpty() && PrevPort > 0)
	{
		ClientComponent->Connect(PrevIP, PrevPort);
	}
	else
	{
		// Reconnect to first enabled adapter
		const uint8 ADMKey = (uint8)ESpatialAdapterType::ADMOSC;
		if (const FSpatialAdapterConfig* Config = AdapterConfigs.Find(ADMKey))
		{
			ClientComponent->Connect(Config->TargetIP, Config->TargetPort);
		}
		else
		{
			ClientComponent->Disconnect();
		}
	}

	// Log the send
	FSpatialFabricLogEntry Entry;
	Entry.Adapter = TEXT("Custom");
	Entry.Direction = TEXT("OUT");
	Entry.Address = Address;
	FString ValStr;
	for (float V : FloatArgs) { ValStr += FString::Printf(TEXT("%.3f "), V); }
	for (int32 V : IntArgs) { ValStr += FString::Printf(TEXT("%d "), V); }
	for (const FString& V : StringArgs) { ValStr += V + TEXT(" "); }
	Entry.ValueStr = ValStr.TrimEnd();
	FDateTime Now = FDateTime::Now();
	Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"), Now.GetHour(), Now.GetMinute(), Now.GetSecond());
	AppendLog(Entry);
}

void ASpatialFabricManagerActor::ApplyStageVisibility()
{
	ASpatialStageVolume* SV = ResolvedStageVolume.Get();
	if (!SV) { SV = StageVolume.Get(); }
	if (!IsValid(SV)) { return; }

	// Only apply in game worlds (PIE or packaged); editor viewport is unaffected
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		SV->SetActorHiddenInGame(bHideStageInPIE);
		SV->SetHideDebugDraw(bHideStageInPIE);
		// Hide the box component's wireframe outline (UBoxComponent draws bounds as lines)
		if (SV->StageBox)
		{
			SV->StageBox->SetHiddenInGame(bHideStageInPIE);
			SV->StageBox->bDrawOnlyIfSelected = bHideStageInPIE;
			SV->StageBox->MarkRenderStateDirty();
		}
	}
}
