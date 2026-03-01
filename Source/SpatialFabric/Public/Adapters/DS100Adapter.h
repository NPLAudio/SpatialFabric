// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialProtocolAdapter.h"

/**
 * FDS100Adapter
 *
 * Implements d&b audiotechnik DS100 Soundscape OSC protocol.
 * Source: d&b DS100 OSC Protocol manual v1.3.10.
 *
 * Default target port: 50010 (DS100 incoming UDP port).
 *
 * Two position modes (configured via FSpatialAdapterConfig::bDS100AbsoluteMode):
 *
 *   Absolute mode (default):
 *     /dbaudio1/positioning/source_position/{id}  x  y  z
 *     Coordinates in metres relative to the DS100 room model origin.
 *     SpatialFabric supplies StageMeters (physical metres from stage volume).
 *
 *   Coordinate-mapping mode (bDS100AbsoluteMode = false):
 *     /dbaudio1/coordinatemapping/source_position_xy/{area}/{id}  x  y
 *     x/y in [0..1] mapped within a DS100 coordinate-mapping area.
 *     SpatialFabric supplies NormalizedToDS100Mapped() output.
 *
 * Spread:
 *   /dbaudio1/positioning/source_spread/{id}  s   (s in [0..1])
 */
class SPATIALFABRIC_API FDS100Adapter : public ISpatialProtocolAdapter
{
public:
	FDS100Adapter() = default;

	virtual FName GetName() const override { return TEXT("DS100"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::DS100; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override;
	virtual void SetClientComponent(ULiveOSCClientComponent* InClient) override;
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot) override;
	virtual bool IsEnabled() const override { return Config.bEnabled; }

private:
	FSpatialAdapterConfig Config;
	ULiveOSCClientComponent* Client = nullptr;

	/**
	 * DS100 coordinate mapping area index (1-based).
	 * Used in coordinate-mapping mode: /coordinatemapping/source_position_xy/{Area}/{id}.
	 */
	int32 CoordMappingArea = 1;

	void SendObject(const FSpatialNormalizedState& State);
};
