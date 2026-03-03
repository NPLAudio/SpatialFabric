// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialProtocolAdapter.h"
#include "SpatialFabricTypes.h"

/**
 * FSpaceMapGoAdapter
 *
 * Routes spatial data to a Meyer Sound SpaceMap Go processor via OSC.
 * SpaceMap Go listens for OSC on port 38033.
 *
 * Per-source messages sent each frame:
 *   /source/{n}/xpan        float  [-1, 1]   left/right pan  (+Y = right, matching our convention)
 *   /source/{n}/ypan        float  [-1, 1]   front/back pan  (+X = front)
 *   /source/{n}/crossfade   float  [ 0, 1]   Spacemap crossfade (mapped from depth Z, optional)
 *   /source/{n}/spread      float  [ 0, 1]   source spread
 *
 * Snapshot recall (call RecallSnapshot manually):
 *   /Console/recall/system  int32  (ID: 900–3000)
 *
 * Default target port: 38033.
 */
class SPATIALFABRIC_API FSpaceMapGoAdapter : public ISpatialProtocolAdapter
{
public:
	FSpaceMapGoAdapter() = default;

	virtual FName GetName() const override { return TEXT("SpaceMapGo"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::SpaceMapGo; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override;
	virtual void SetClientComponent(USpatialOSCClientComponent* InClient) override;
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime) override;
	virtual bool IsEnabled() const override { return Config.bEnabled; }

	/** Recall a SpaceMap Go system snapshot by ID (900–3000). */
	void RecallSnapshot(int32 SnapshotID);

private:
	FSpatialAdapterConfig Config;
	USpatialOSCClientComponent* Client = nullptr;

	void SendSource(const FSpatialNormalizedState& State);
};
