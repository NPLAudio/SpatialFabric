// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "SpatialOSCServerComponent.h"
#include "OSCManager.h"
#include "OSCAddress.h"
#include "OSCTypes.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpatialOSCServer, Log, All);

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
	UE_LOG(LogSpatialOSCServer, Log, TEXT("OSC Server listening on %s:%d"), *IP, Port);
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
		UE_LOG(LogSpatialOSCServer, Log, TEXT("OSC Server stopped"));
	}
}

bool USpatialOSCServerComponent::IsListening() const
{
	return OSCServer != nullptr;
}

void USpatialOSCServerComponent::HandleOSCMessage(const FOSCMessage& Message, const FString& IPAddress, uint16 Port)
{
	const FString AddressStr = Message.GetAddress().GetFullPath();

	float Value = 0.f;
	bool bFoundFloat = false;

	const TArray<UE::OSC::FOSCData>& Args = Message.GetArgumentsChecked();

	for (int32 i = 0; i < Args.Num(); ++i)
	{
		const UE::OSC::FOSCData& Arg = Args[i];
		if (Arg.IsFloat())
		{
			Value = Arg.GetFloat();
			bFoundFloat = true;
			break;
		}
		if (Arg.IsInt32())
		{
			Value = static_cast<float>(Arg.GetInt32());
			bFoundFloat = true;
			break;
		}
	}

	if (!bFoundFloat) return;

	OnMessageReceived.Broadcast(AddressStr, Value);
}
