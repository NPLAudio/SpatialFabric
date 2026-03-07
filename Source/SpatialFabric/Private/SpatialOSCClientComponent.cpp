// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "SpatialOSCClientComponent.h"
#include "SpatialFabricSettings.h"
#include "OSCManager.h"
#include "OSCMessage.h"
#include "OSCAddress.h"
#include "OSCBundle.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpatialOSCClient, Log, All);

#define SF_LOG_DEBUG(Fmt, ...) \
	do { if (GetDefault<USpatialFabricSettings>()->bEnableDebugMessages) { UE_LOG(LogSpatialOSCClient, Log, Fmt, ##__VA_ARGS__); } } while(0)

USpatialOSCClientComponent::USpatialOSCClientComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USpatialOSCClientComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Disconnect();
	Super::EndPlay(EndPlayReason);
}

void USpatialOSCClientComponent::Connect(const FString& IP, int32 Port)
{
	Disconnect();

	OSCClient = UOSCManager::CreateOSCClient(IP, Port, GetName(), this);

	if (!OSCClient)
	{
		UE_LOG(LogSpatialOSCClient, Error, TEXT("Failed to create OSC Client targeting %s:%d"), *IP, Port);
		return;
	}

	if (!OSCClient->IsActive())
	{
		UE_LOG(LogSpatialOSCClient, Warning,
			TEXT("OSC Client created for %s:%d but IsActive() is false — socket may have failed to bind. "
			     "Verify the port is not already in use."),
			*IP, Port);
	}

	CurrentIP = IP;
	CurrentPort = Port;
	SF_LOG_DEBUG(TEXT("OSC Client connected to %s:%d"), *IP, Port);
}

void USpatialOSCClientComponent::Disconnect()
{
	if (OSCClient)
	{
		OSCClient = nullptr;
		CurrentIP.Empty();
		CurrentPort = 0;
		SF_LOG_DEBUG(TEXT("OSC Client disconnected"));
	}
}

bool USpatialOSCClientComponent::IsConnected() const
{
	return OSCClient != nullptr && OSCClient->IsActive();
}

void USpatialOSCClientComponent::SendFloat(const FString& Address, float Value)
{
	if (!OSCClient || !OSCClient->IsActive())
	{
		return;
	}

	FOSCMessage Message;
	Message.SetAddress(UOSCManager::ConvertStringToOSCAddress(Address));
	UOSCManager::AddFloat(Message, Value);
	OSCClient->SendOSCMessage(Message);
}

void USpatialOSCClientComponent::SendInt(const FString& Address, int32 Value)
{
	if (!OSCClient || !OSCClient->IsActive()) { return; }

	FOSCMessage Message;
	Message.SetAddress(UOSCManager::ConvertStringToOSCAddress(Address));
	UOSCManager::AddInt32(Message, Value);
	OSCClient->SendOSCMessage(Message);
}

void USpatialOSCClientComponent::SendLinearColor(const FString& Address, FLinearColor Color)
{
	if (!OSCClient || !OSCClient->IsActive()) return;

	FOSCMessage Message;
	Message.SetAddress(UOSCManager::ConvertStringToOSCAddress(Address));
	UOSCManager::AddFloat(Message, Color.R);
	UOSCManager::AddFloat(Message, Color.G);
	UOSCManager::AddFloat(Message, Color.B);
	UOSCManager::AddFloat(Message, Color.A);
	OSCClient->SendOSCMessage(Message);
}

void USpatialOSCClientComponent::SendString(const FString& Address, const FString& Value)
{
	if (!OSCClient || !OSCClient->IsActive()) return;

	FOSCMessage Message;
	Message.SetAddress(UOSCManager::ConvertStringToOSCAddress(Address));
	UOSCManager::AddString(Message, Value);
	OSCClient->SendOSCMessage(Message);
}

void USpatialOSCClientComponent::SendBundle(FOSCBundle& Bundle)
{
	if (!OSCClient || !OSCClient->IsActive()) return;
	OSCClient->SendOSCBundle(Bundle);
}

void USpatialOSCClientComponent::SendMultiArg(const FString& Address, const TArray<float>& Values)
{
	if (!OSCClient || !OSCClient->IsActive() || Values.Num() == 0) return;

	FOSCMessage Message;
	Message.SetAddress(UOSCManager::ConvertStringToOSCAddress(Address));
	for (float V : Values)
	{
		UOSCManager::AddFloat(Message, V);
	}
	OSCClient->SendOSCMessage(Message);
}

void USpatialOSCClientComponent::SendMessage(const FString& Address,
	const TArray<float>& FloatArgs,
	const TArray<int32>& IntArgs,
	const TArray<FString>& StringArgs)
{
	if (!OSCClient || !OSCClient->IsActive()) return;

	FOSCMessage Message;
	Message.SetAddress(UOSCManager::ConvertStringToOSCAddress(Address));
	for (float V : FloatArgs)
	{
		UOSCManager::AddFloat(Message, V);
	}
	for (int32 V : IntArgs)
	{
		UOSCManager::AddInt32(Message, V);
	}
	for (const FString& V : StringArgs)
	{
		UOSCManager::AddString(Message, V);
	}
	OSCClient->SendOSCMessage(Message);
}
