// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialProtocolAdapter.h"

/**
 * FSpaceMapGoAdapter  [Phase 2 stub]
 *
 * Routes spatial data to a Meyer Sound SpaceMap Go processor.
 * SpaceMap Go accepts OSC on port 38033 and RTTrPM for tracking.
 *
 * Default target port: 38033.
 *
 * Supported operations:
 *   - Snapshot recall:  /Console/recall/system  <ID>   (ID: 900–3000)
 *   - Tracking:         delegate to FRTTrPMAdapter
 *
 * TODO (Phase 2): Implement snapshot recall on zone events and
 *   RTTrPM passthrough configuration.
 */
class SPATIALFABRIC_API FSpaceMapGoAdapter : public ISpatialProtocolAdapter
{
public:
	FSpaceMapGoAdapter() = default;

	virtual FName GetName() const override { return TEXT("SpaceMapGo"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::SpaceMapGo; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override { Config = InConfig; }
	virtual void SetClientComponent(ULiveOSCClientComponent* InClient) override { Client = InClient; }
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime) override { /* Phase 2 */ }
	virtual bool IsEnabled() const override { return Config.bEnabled; }

	/** Recall a SpaceMap Go system snapshot by ID (900–3000). */
	void RecallSnapshot(int32 SnapshotID);

private:
	FSpatialAdapterConfig Config;
	ULiveOSCClientComponent* Client = nullptr;
};
