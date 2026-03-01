// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialProtocolAdapter.h"

/**
 * FADMOSCAdapter
 *
 * Implements the ADM-OSC object metadata standard (immersive-audio-live.github.io/ADM-OSC).
 * Sends normalized xyz, gain, width (w), and mute state for each spatial object.
 * When a listener is present in the snapshot, also emits /adm/lis/xyz and /adm/lis/ypr.
 *
 * Default target port: 9000 (L-ISA Controller default).
 * Recommended send rate: ≤50 Hz (per ADM-OSC spec).
 *
 * OSC address examples:
 *   /adm/obj/4/xyz   -0.9  0.15  0.70
 *   /adm/obj/4/gain   0.707
 *   /adm/obj/4/w      0.20
 *   /adm/obj/4/mute   0
 *   /adm/lis/xyz      0.0   0.5  -0.2
 *   /adm/lis/ypr    -45.0  30.0   5.0
 *
 * Coordinates: x/y/z are the FSpatialNormalizedState.StageNormalized values,
 * which are already in [-1,1] per ADM-OSC convention.
 */
class SPATIALFABRIC_API FADMOSCAdapter : public ISpatialProtocolAdapter
{
public:
	FADMOSCAdapter() = default;

	virtual FName GetName() const override { return TEXT("ADMOSC"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::ADMOSC; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override;
	virtual void SetClientComponent(ULiveOSCClientComponent* InClient) override;
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot) override;
	virtual bool IsEnabled() const override { return Config.bEnabled; }

private:
	FSpatialAdapterConfig Config;
	ULiveOSCClientComponent* Client = nullptr;

	float LastSendTime = 0.f;

	/** Send all OSC messages for one object. */
	void SendObject(const FSpatialNormalizedState& State);
	/** Send listener position and orientation. */
	void SendListener(const FSpatialFrameSnapshot& Snapshot);
};
