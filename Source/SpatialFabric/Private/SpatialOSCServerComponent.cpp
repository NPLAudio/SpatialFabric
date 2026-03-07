// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "SpatialOSCServerComponent.h"
#include "SpatialFabricSettings.h"
#include "OSCManager.h"
#include "OSCAddress.h"
#include "OSCTypes.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpatialOSCServer, Log, All);

#define SF_LOG_DEBUG(Fmt, ...) \
	do { if (GetDefault<USpatialFabricSettings>()->bEnableDebugMessages) { UE_LOG(LogSpatialOSCServer, Log, Fmt, ##__VA_ARGS__); } } while(0)

USpatialOSCServerComponent::USpatialOSCServerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USpatialOSCServerComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USpatialOSCServerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopListening();
	Super::EndPlay(EndPlayReason);
}

void USpatialOSCServerComponent::StartListening(const FString& IP, int32 Port)
{
	StopListening();

	OSCServer = UOSCManager::CreateOSCServer(IP, Port, false, true, GetName(), this);

	if (!OSCServer)
	{
		UE_LOG(LogSpatialOSCServer, Error, TEXT("Failed to create OSC Server on %s:%d"), *IP, Port);
		return;
	}

	OSCServer->OnOscMessageReceivedNative.AddUObject(this, &USpatialOSCServerComponent::HandleOSCMessage);

	CurrentIP = IP;
	CurrentPort = Port;
	SF_LOG_DEBUG(TEXT("OSC Server listening on %s:%d"), *IP, Port);
}

void USpatialOSCServerComponent::StopListening()
{
	if (OSCServer)
	{
		OSCServer->Stop();
		OSCServer->OnOscMessageReceivedNative.RemoveAll(this);
		OSCServer = nullptr;
		CurrentPort = 0;
		CurrentIP.Empty();
		SF_LOG_DEBUG(TEXT("OSC Server stopped"));
	}
}

bool USpatialOSCServerComponent::IsListening() const
{
	return OSCServer != nullptr;
}

void USpatialOSCServerComponent::HandleOSCMessage(const FOSCMessage& Message, const FString& IPAddress, uint16 Port)
{
	const FString AddressStr = UOSCManager::GetOSCAddressFullPath(Message.GetAddress());

	float Value = 0.f;
	bool bFoundFloat = false;

	TArray<float> Floats;
	UOSCManager::GetAllFloats(Message, Floats);
	if (Floats.Num() > 0)
	{
		Value = Floats[0];
		bFoundFloat = true;
	}
	if (!bFoundFloat)
	{
		TArray<int32> Ints;
		UOSCManager::GetAllInt32s(Message, Ints);
		if (Ints.Num() > 0)
		{
			Value = static_cast<float>(Ints[0]);
			bFoundFloat = true;
		}
	}

	if (!bFoundFloat) return;

	OnMessageReceived.Broadcast(AddressStr, Value);
}
