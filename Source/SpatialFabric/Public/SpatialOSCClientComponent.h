// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OSCClient.h"
#include "SpatialOSCClientComponent.generated.h"

struct FOSCBundle;

/**
 * USpatialOSCClientComponent
 *
 * Wraps the Unreal Engine OSC plugin's UOSCClient to send outgoing UDP
 * packets.  Provides typed helpers for the message formats used by
 * SpatialFabric adapters (single float, int, string, color, bundle,
 * and multi-arg).
 *
 * Owned by ASpatialFabricManagerActor.  Does not tick.
 */
UCLASS(BlueprintType, ClassGroup = "SpatialFabric",
	meta = (BlueprintSpawnableComponent, DisplayName = "Spatial OSC Client Component"))
class SPATIALFABRIC_API USpatialOSCClientComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USpatialOSCClientComponent();

	/** Open a UDP socket targeting IP:Port. Disconnects any existing connection first. */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|OSC")
	void Connect(const FString& IP, int32 Port);

	/** Close the UDP socket. Safe to call when not connected. */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|OSC")
	void Disconnect();

	/** Returns true if the client socket is open and active. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpatialFabric|OSC")
	bool IsConnected() const;

	/** Returns the IP address this client is targeting. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpatialFabric|OSC")
	FString GetTargetIP() const { return CurrentIP; }

	/** Returns the port this client is targeting. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpatialFabric|OSC")
	int32 GetTargetPort() const { return CurrentPort; }

	/** Send a single-float OSC message to Address. No-op if not connected. */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|OSC")
	void SendFloat(const FString& Address, float Value);

	/** Send a single int32 OSC message to Address (OSC type tag 'i'). No-op if not connected. */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|OSC")
	void SendInt(const FString& Address, int32 Value);

	/** Send a linear color as four floats (R, G, B, A) on Address. No-op if not connected. */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|OSC")
	void SendLinearColor(const FString& Address, FLinearColor Color);

	/** Send a string argument on Address. No-op if not connected. */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|OSC")
	void SendString(const FString& Address, const FString& Value);

	/** Send a pre-built OSC bundle. No-op if not connected. */
	void SendBundle(FOSCBundle& Bundle);

	/** Send all Values as multiple float arguments on a single Address. No-op if not connected or Values is empty. */
	void SendMultiArg(const FString& Address, const TArray<float>& Values);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Underlying UE OSC client object. Null when disconnected. */
	UPROPERTY()
	TObjectPtr<UOSCClient> OSCClient;

	/** IP address this client targets. */
	FString CurrentIP;
	/** Port this client targets. */
	int32 CurrentPort = 0;
};
