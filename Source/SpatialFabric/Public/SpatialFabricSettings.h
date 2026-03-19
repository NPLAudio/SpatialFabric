// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SpatialFabricSettings.generated.h"

/**
 * USpatialFabricSettings
 *
 * Project-wide defaults for SpatialFabric.  Accessible via
 * Project Settings → Plugins → Spatial Fabric.
 *
 * Networking is disabled in packaged builds by default (mirrors
 * Remote Control's -RCWebInterfaceEnable pattern).  Users must opt-in
 * via config or at runtime.
 */
UCLASS(config = SpatialFabric, defaultconfig, meta = (DisplayName = "Spatial Fabric"))
class SPATIALFABRIC_API USpatialFabricSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USpatialFabricSettings();

	/** When true, SpatialFabric networking is active in the editor. */
	UPROPERTY(EditAnywhere, config, Category = "SpatialFabric")
	bool bEnableInEditor = true;

	/**
	 * When true, SpatialFabric networking is active in packaged builds.
	 * Off by default — set via config or command-line to avoid unintended
	 * network traffic in shipped games.
	 */
	UPROPERTY(EditAnywhere, config, Category = "SpatialFabric")
	bool bEnableInPackagedBuilds = false;

	// ── Default listen port for incoming OSC (tracking-in / feedback) ──────
	UPROPERTY(EditAnywhere, config, Category = "SpatialFabric|Ports",
		meta = (ClampMin = "1024", ClampMax = "65535"))
	int32 DefaultOSCListenPort = 8100;

	// ── Default outgoing IP and ports per adapter ──────────────────────────
	/** Saved when user changes IP in the panel; used when spawning new managers. */
	UPROPERTY(EditAnywhere, config, Category = "SpatialFabric|Ports")
	FString DefaultADMOSTargetIP = TEXT("127.0.0.1");

	UPROPERTY(EditAnywhere, config, Category = "SpatialFabric|Ports")
	FString DefaultCustomTargetIP = TEXT("127.0.0.1");

	UPROPERTY(EditAnywhere, config, Category = "SpatialFabric|Ports",
		meta = (ClampMin = "1024", ClampMax = "65535"))
	int32 DefaultADMOSCPort = 50018;

	// ── Default send rate ───────────────────────────────────────────────────
	/** Default maximum packets-per-second for all adapters (overridable per-adapter). */
	UPROPERTY(EditAnywhere, config, Category = "SpatialFabric",
		meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float DefaultSendRateHz = 50.f;

	// ── Diagnostics ─────────────────────────────────────────────────────────
	/**
	 * When true, SpatialFabric emits verbose debug messages to the Output Log
	 * (OSC connect/disconnect events, missing-actor warnings, etc.).
	 * Off by default to keep the log clean during normal use.
	 */
	UPROPERTY(EditAnywhere, config, Category = "SpatialFabric")
	bool bEnableDebugMessages = false;

	// UDeveloperSettings overrides
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
};
