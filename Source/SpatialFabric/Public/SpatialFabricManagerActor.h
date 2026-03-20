// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpatialFabricTypes.h"
#include "ProtocolRouter.h"
#include "SpatialFabricManagerActor.generated.h"

class USpatialOSCServerComponent;
class USpatialOSCClientComponent;
class USpatialObjectRegistry;
class ASpatialStageVolume;

/**
 * ASpatialFabricManagerActor
 *
 * This is the main *actor* you place in the level (like a light or camera).
 * It owns subobjects: OSC server, OSC client, registry component, and a non-UObject
 * router (see TUniquePtr in the .cpp). Tick order is PostPhysics so movement is final.
 *
 * Central level actor for SpatialFabric.  Place one in the level, assign a
 * ASpatialStageVolume, configure adapter endpoints, then populate
 * ObjectBindings to start routing spatial positions to connected hardware.
 *
 * Architecture:
 *   - USpatialOSCServerComponent — receives incoming OSC
 *   - USpatialOSCClientComponent — sends OSC packets
 *   - USpatialObjectRegistry     — builds FSpatialFrameSnapshot each tick
 *   - FProtocolRouter            — fans snapshot out to all enabled adapters
 *
 * Each adapter is created and configured during BeginPlay from AdapterConfigs.
 * In the editor (no PIE), FSpatialFabricEditorTickable drives ProcessFrame so
 * the editor panel shows live data without requiring Play.
 *
 * Networking is disabled in packaged builds unless USpatialFabricSettings::
 * bEnableInPackagedBuilds is true.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Spatial Fabric Manager"))
class SPATIALFABRIC_API ASpatialFabricManagerActor : public AActor
{
	GENERATED_BODY()

public:
	ASpatialFabricManagerActor();

	// ── Stage ────────────────────────────────────────────────────────────────

	/**
	 * Stage volume that defines the coordinate space.
	 * All actor positions are normalized relative to this volume's bounds.
	 * Must be set before play for correct output; if null, raw UE cm are used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Stage")
	TSoftObjectPtr<ASpatialStageVolume> StageVolume;

	/**
	 * When true, hides the stage volume actor in the game view during PIE and
	 * packaged builds. Useful to avoid the stage box cluttering the viewport
	 * while testing. Toggle via the Spatial Fabric Manager panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Stage")
	bool bHideStageInPIE = false;

	/**
	 * Apply bHideStageInPIE to the stage volume's visibility.
	 * Called automatically on BeginPlay; also call when toggling from the editor panel.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|Stage")
	void ApplyStageVisibility();

	// ── Bindings ─────────────────────────────────────────────────────────────

	/**
	 * The single protocol format all tracked objects currently route to.
	 * Changed via the Spatial Fabric editor panel format selector.
	 * Changing this re-enables the matching adapter and disables all others.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Bindings")
	ESpatialAdapterType ActiveAdapterType = ESpatialAdapterType::ADMOSC;

	/** All active actor → adapter target mappings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Bindings",
		meta = (TitleProperty = "Label"))
	TArray<FSpatialObjectBinding> ObjectBindings;

	// ── Custom OSC (on-demand send) ───────────────────────────────────────────

	/** Target IP for Custom OSC Send. Used when SendCustomOSC is called. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Custom")
	FString CustomTargetIP = TEXT("127.0.0.1");

	/** Target port for Custom OSC Send. Used when SendCustomOSC is called. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Custom",
		meta = (ClampMin = "1024", ClampMax = "65535"))
	int32 CustomTargetPort = 9000;

	/**
	 * Send an arbitrary OSC message to CustomTargetIP:CustomTargetPort.
	 * Connect to the custom endpoint, send the message, then restore the previous
	 * client connection. Safe to call from Blueprint or C++.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|Custom")
	void SendCustomOSC(const FString& Address,
		const TArray<float>& FloatArgs,
		const TArray<int32>& IntArgs,
		const TArray<FString>& StringArgs);

	// ── Adapter endpoint configuration ───────────────────────────────────────

	/**
	 * Per-adapter network and rate configuration.
	 * Key: ESpatialAdapterType cast to uint8.
	 * Populated with defaults from USpatialFabricSettings on construction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapters")
	TMap<uint8, FSpatialAdapterConfig> AdapterConfigs;

	// ── OSC server (receive) ─────────────────────────────────────────────────

	/** Receives incoming OSC via UE5's OSC plugin. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpatialFabric|Components")
	USpatialOSCServerComponent* ServerComponent;

	/**
	 * Shared OSC client — passed to all OSC-based adapters for sending.
	 * NOTE: All OSC adapters share one client; each adapter sends to its own
	 * configured IP:port by reconnecting the client per-send.  For independent
	 * concurrent targets, create multiple manager actors.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpatialFabric|Components")
	USpatialOSCClientComponent* ClientComponent;

	/** Spatial object registry component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpatialFabric|Components")
	USpatialObjectRegistry* RegistryComponent;

	// ── Log ──────────────────────────────────────────────────────────────────

	/** Ring buffer of recent protocol log entries. */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric|Log")
	TArray<FSpatialFabricLogEntry> MessageLog;

	// ── Runtime controls ─────────────────────────────────────────────────────

	/** Start the OSC receive server (called automatically on BeginPlay if settings allow). */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric")
	void StartServer();

	/** Stop the OSC receive server. */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric")
	void StopServer();

	/** Connect the shared OSC client (reconnects based on current first-adapter config). */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric")
	void ConnectClient();

	/** Disconnect the shared OSC client. */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric")
	void DisconnectClient();

	/**
	 * Drive one spatial frame: build snapshot and route to all enabled adapters.
	 * Called by Tick (PIE/game) and by FSpatialFabricEditorTickable (editor).
	 */
	void ProcessFrame(float DeltaTime);

	/** Get the most recent N log entries (newest last). */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|Log")
	TArray<FSpatialFabricLogEntry> GetRecentLog(int32 Count = 10) const;

	/** Returns the most recent per-frame snapshot (updated every Tick during PIE/editor tick). */
	const FSpatialFrameSnapshot& GetLastSnapshot() const { return LastSnapshot; }

	/** Append a log entry; trims to MaxLogEntries. */
	void AppendLog(const FSpatialFabricLogEntry& Entry);

	/** Clear all log entries. Called by the editor panel Clear button. */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric|Log")
	void ClearMessageLog();

	/**
	 * Find the first ASpatialFabricManagerActor in WorldContextObject's world.
	 * Spawns a new one if none exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialFabric",
		meta = (WorldContext = "WorldContextObject"))
	static ASpatialFabricManagerActor* GetOrSpawnManager(UObject* WorldContextObject);

	/** Direct access to the protocol router (used by the editor module). */
	FProtocolRouter* GetRouter() const { return Router.Get(); }

	/**
	 * Re-create and re-configure all adapters from the current AdapterConfigs.
	 * Safe to call during PIE when you change adapter settings at runtime
	 * (e.g. via the editor panel format selector).  No-op if Router is null.
	 */
	void InitializeAdapters();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

private:
	static constexpr int32 MaxLogEntries = 500;

	/** Protocol router — plain C++ object, not a UPROPERTY. */
	TUniquePtr<FProtocolRouter> Router;

	/** Cached snapshot from the most recent ProcessFrame call. */
	FSpatialFrameSnapshot LastSnapshot;

	/** Cached resolved pointer to the stage volume (updated each tick). */
	UPROPERTY()
	ASpatialStageVolume* ResolvedStageVolume;

	/** Bound to ServerComponent::OnMessageReceived — forwards to router. */
	UFUNCTION()
	void OnOSCMessageReceived(const FString& Address, float Value);

	/** Apply default adapter configs from USpatialFabricSettings. */
	void PopulateDefaultAdapterConfigs();

	/**
	 * Resolve the Stage Volume for this actor's world. Handles PIE where
	 * TSoftObjectPtr may resolve to the Editor world's actor instead of the
	 * PIE-world duplicate. Falls back to first ASpatialStageVolume in world.
	 */
	ASpatialStageVolume* ResolveStageVolumeForThisWorld();
};
