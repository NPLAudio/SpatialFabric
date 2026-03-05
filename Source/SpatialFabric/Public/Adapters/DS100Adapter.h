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
 *     Coordinates in meters relative to the DS100 room model origin.
 *     SpatialFabric supplies StageMeters (physical meters from stage volume).
 *
 *   Coordinate-mapping mode (bDS100AbsoluteMode = false):
 *     /dbaudio1/coordinatemapping/source_position_xy/{area}/{id}  x  y
 *     x/y in [0..1] mapped within a DS100 coordinate-mapping area.
 *     SpatialFabric supplies NormalizedToDS100Mapped() output.
 *
 * Spread:
 *   /dbaudio1/positioning/source_spread/{id}  s   (s in [0..1])
 *   Two modes per binding (EDS100SpreadMode):
 *     Fixed     — s = binding.Width01 (constant)
 *     Proximity — s = inverse-square interpolation between DS100SpreadMin and
 *                 DS100SpreadMax based on listener distance in stage-normalized space.
 *
 * Delay mode:
 *   /dbaudio1/positioning/source_delaymode/{id}  n  (n: 0=off, 1=tight, 2=full)
 *   Sent per-frame when binding.DS100DelayMode >= 0; suppressed when -1.
 */
class SPATIALFABRIC_API FDS100Adapter : public ISpatialProtocolAdapter
{
public:
	FDS100Adapter() = default;

	virtual FName GetName() const override { return TEXT("DS100"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::DS100; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override;
	virtual void SetClientComponent(USpatialOSCClientComponent* InClient) override;
	virtual void SetBindings(const TArray<FSpatialObjectBinding>& Bindings) override;
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime) override;
	virtual bool IsEnabled() const override { return Config.bEnabled; }

private:
	FSpatialAdapterConfig Config;
	USpatialOSCClientComponent* Client = nullptr;

	/**
	 * DS100 coordinate mapping area index (1-based).
	 * Used in coordinate-mapping mode: /coordinatemapping/source_position_xy/{Area}/{id}.
	 */
	int32 CoordMappingArea = 1;

	/** Bindings cached by resolved ObjectID — refreshed each frame by SetBindings(). */
	TMap<int32, FSpatialObjectBinding> CachedBindingsByObjectID;

	/** Per-object values from the last send; sentinel values mean "never sent yet". */
	struct FDS100LastSent
	{
		FVector PosNorm   = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
		float   Spread    = -999.f;
		int32   DelayMode = MIN_int32;
	};
	TMap<int32, FDS100LastSent> LastSentByID;

	/**
	 * Compute spread for one object.
	 * Fixed mode: returns State.Width01 unchanged.
	 * Proximity mode: inverse-square curve between DS100SpreadMin/DS100SpreadMax
	 *   using distance from ListenerNorm to State.StageNormalized.
	 */
	float ComputeSpread(const FSpatialNormalizedState& State,
	                    const FVector& ListenerNorm) const;

	void SendObject(const FSpatialNormalizedState& State,
	                const FSpatialFrameSnapshot& Snapshot);
};
