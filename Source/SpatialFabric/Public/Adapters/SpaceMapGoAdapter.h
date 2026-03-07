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
 *   /channel/{n}/position   float float   X Y position, range [-1000, 1000]
 *     X: left (-1000) to right (+1000) — maps from our +Y stage axis
 *     Y: back (-1000) to front (+1000) — maps from our +X stage axis
 *   /channel/{n}/spread     float         spread, range [0, 100] (logarithmic)
 *     maps from Width01 * 100; 14 = optimal default per SpaceMapGo spec
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

private:
	FSpatialAdapterConfig Config;
	USpatialOSCClientComponent* Client = nullptr;

	struct FSpaceMapGoCachedState
	{
		FVector PosNorm = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
		float Spread = -999.f;
		bool bEverSent = false;
	};
	TMap<int32, FSpaceMapGoCachedState> LastSentByID;

	void SendSource(const FSpatialNormalizedState& State);
};
