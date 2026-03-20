// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

/**
 * Manager actor: BeginPlay wires OSC → router, creates adapters, starts server.
 * Each Tick: BuildSnapshot → Router::ProcessFrame. Packaged builds can disable
 * networking via USpatialFabricSettings::bEnableInPackagedBuilds.
 */

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

ASpatialFabricManagerActor::ASpatialFabricManagerActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// Must tick after the stage volume (PostPhysics), which itself runs after
	// character movement, so the listener position is always current.
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	ServerComponent   = CreateDefaultSubobject<USpatialOSCServerComponent>(TEXT("ServerComponent"));
	ClientComponent   = CreateDefaultSubobject<USpatialOSCClientComponent>(TEXT("ClientComponent"));
	RegistryComponent = CreateDefaultSubobject<USpatialObjectRegistry>(TEXT("RegistryComponent"));
	// Router is created in BeginPlay (needs valid world / settings path for packaged checks)

	PopulateDefaultAdapterConfigs();
}

// ── AActor overrides ──────────────────────────────────────────────────────

void ASpatialFabricManagerActor::BeginPlay()
{
	Super::BeginPlay();

	// Resolve stage volume and apply visibility early (before early return)
	ResolvedStageVolume = ResolveStageVolumeForThisWorld();
	if (IsValid(ResolvedStageVolume))
	{
		AddTickPrerequisiteActor(ResolvedStageVolume);
	}
	ApplyStageVisibility();

	const USpatialFabricSettings* Settings = GetDefault<USpatialFabricSettings>();
	const bool bIsPackaged = !GIsEditor;
	if (bIsPackaged && !Settings->bEnableInPackagedBuilds)
	{
		// Skip OSC entirely in shipping unless explicitly opted in (avoids surprise UDP)
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

	// Re-resolve stage volume each tick (handles late binding + PIE world mismatch)
	if (!IsValid(ResolvedStageVolume) && !StageVolume.IsNull())
	{
		ResolvedStageVolume = ResolveStageVolumeForThisWorld();
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

	// Single pipeline step: registry produces geometry; router + adapters produce OSC
	LastSnapshot =
		RegistryComponent->BuildSnapshot(ObjectBindings, ResolvedStageVolume);

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
		// Default outbound target: ADM-OSC config (other adapters reconnect in ProcessFrame)
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

void ASpatialFabricManagerActor::ClearMessageLog()
{
	MessageLog.Empty();
}

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
		return *It; // reuse existing manager
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
		// Each adapter pushes FSpatialFabricLogEntry; manager owns the ring buffer
		Adapter->OnLog = [this](const FSpatialFabricLogEntry& Entry)
		{
			AppendLog(Entry);
		};
		Router->RegisterAdapter(Adapter);
	};

	RegisterOSC(MakeShared<FADMOSCAdapter>(),     ESpatialAdapterType::ADMOSC);
	RegisterOSC(MakeShared<FCustomAdapter>(),     ESpatialAdapterType::Custom);

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
		C.SendRateHz = S->DefaultSendRateHz; // project default; user can override on the actor
		C.bEnabled   = bEnabled;
		return C;
	};

	// uint8 key = enum value so TMap serializes cleanly on the manager actor
	AdapterConfigs.Add((uint8)ESpatialAdapterType::ADMOSC,
		MakeConfig(S->DefaultADMOSTargetIP.IsEmpty() ? TEXT("127.0.0.1") : S->DefaultADMOSTargetIP,
		          S->DefaultADMOSCPort, /*bEnabled=*/true));

	AdapterConfigs.Add((uint8)ESpatialAdapterType::Custom,
		MakeConfig(S->DefaultCustomTargetIP.IsEmpty() ? TEXT("127.0.0.1") : S->DefaultCustomTargetIP,
		          S->DefaultADMOSCPort));
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

	// One UDP client is shared: temporarily point it at CustomTarget*, send, then restore
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

ASpatialStageVolume* ASpatialFabricManagerActor::ResolveStageVolumeForThisWorld()
{
	ASpatialStageVolume* SV = StageVolume.Get();
	// TSoftObjectPtr can resolve to the Editor world's actor during PIE instead of
	// the PIE-world duplicate, causing wrong-world coordinates. Validate same world.
	if (IsValid(SV) && SV->GetWorld() != GetWorld())
	{
		SV = nullptr; // Wrong world — don't use
	}
	if (!SV && GetWorld())
	{
		// Fallback: find an ASpatialStageVolume in this world (handles PIE duplication)
		for (TActorIterator<ASpatialStageVolume> It(GetWorld()); It; ++It)
		{
			SV = *It;
			break;
		}
	}
	return SV;
}

void ASpatialFabricManagerActor::ApplyStageVisibility()
{
	ASpatialStageVolume* SV = ResolvedStageVolume;
	if (!SV) { SV = ResolveStageVolumeForThisWorld(); }
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
