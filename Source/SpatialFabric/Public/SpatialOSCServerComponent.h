// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OSCServer.h"
#include "OSCMessage.h"
#include "SpatialOSCServerComponent.generated.h"

/** Fired when a valid single-float OSC message is received. Address is the OSC path; Value is the first float argument. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpatialOSCMessageReceived, const FString&, Address, float, Value);

/**
 * USpatialOSCServerComponent
 *
 * OnMessageReceived only forwards the *first float* (or int cast to float) from each
 * message — enough for simple feedback; extend HandleOSCMessage if you need full
 * multi-arg parsing.
 *
 * Wraps the Unreal Engine OSC plugin's UOSCServer to receive incoming UDP
 * packets and expose them as a Blueprint-assignable delegate.
 *
 * Only the first float argument of each message is forwarded; multi-argument
 * or non-float messages are silently ignored.
 *
 * Owned by ASpatialFabricManagerActor.  Do not tick — event-driven via OSC callbacks.
 */
UCLASS(BlueprintType, ClassGroup = "SpatialFabric",
	meta = (BlueprintSpawnableComponent, DisplayName = "Spatial OSC Server Component"))
class SPATIALFABRIC_API USpatialOSCServerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USpatialOSCServerComponent();

	/** Broadcast when a single-float OSC message arrives. Bind here to process incoming data. */
	UPROPERTY(BlueprintAssignable, Category = "SpatialFabric|OSC")
	FOnSpatialOSCMessageReceived OnMessageReceived;

	/** Open a UDP socket and begin listening on the given IP and port. Closes any previously open socket first. */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|OSC")
	void StartListening(const FString& IP, int32 Port);

	/** Close the UDP socket and stop receiving messages. */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|OSC")
	void StopListening();

	/** Returns true if the server socket is currently open and listening. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpatialFabric|OSC")
	bool IsListening() const;

	/** Returns the UDP port currently being listened on (0 if not listening). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpatialFabric|OSC")
	int32 GetListenPort() const { return CurrentPort; }

	/** Returns the IP address currently being listened on. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpatialFabric|OSC")
	FString GetListenIP() const { return CurrentIP; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Underlying UE OSC server object. */
	UPROPERTY()
	UOSCServer* OSCServer;

	/** IP address the server is bound to. */
	FString CurrentIP;
	/** Port the server is listening on. */
	int32 CurrentPort = 0;

	/** Callback from UOSCServer; extracts the first float and fires OnMessageReceived. */
	void HandleOSCMessage(const FOSCMessage& Message, const FString& IPAddress, uint16 Port);
};
