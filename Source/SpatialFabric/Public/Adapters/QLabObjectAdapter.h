// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialProtocolAdapter.h"

/**
 * FQLabObjectAdapter
 *
 * Implements QLab 5 Audio Object real-time position control.
 * Source: QLab 5 OSC Dictionary — https://qlab.app/docs/v5/scripting/osc-dictionary-v5/
 *
 * Default target port: 53000 (QLab OSC receive port).
 * Recommended send rate: 30–60 Hz for smooth motion.
 *
 * For each spatial object, the binding's FSpatialAdapterTargetEntry encodes
 * the QLab cue ID and object name packed into the label field as:
 *   "<label>|<QLabCueID>|<QLabObjectName>"
 *
 * OSC messages sent each frame (using /live suffix for real-time control):
 *   /cue/{cueID}/object/{objectName}/position/live   x  y
 *   /cue/{cueID}/object/{objectName}/spread/live     s
 *
 * Coordinate mapping (computed by FSpatialMath::NormalizedToQLab2D):
 *   QLab X = left(-1)..right(+1)  ← mapped from stage -Y..+Y (left-positive)
 *   QLab Y = back(-1)..front(+1)  ← mapped from stage -X..+X (front-positive)
 *   Spread = elevation magnitude  (or binding Width01 if non-zero)
 *
 * QLab auto-computes per-output levels from the object position; no manual
 * matrix crosspoints are needed for standard speaker configurations.
 */
class SPATIALFABRIC_API FQLabObjectAdapter : public ISpatialProtocolAdapter
{
public:
	FQLabObjectAdapter() = default;

	virtual FName GetName() const override { return TEXT("QLabObject"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::QLabObject; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override;
	virtual void SetClientComponent(ULiveOSCClientComponent* InClient) override;
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime) override;
	virtual bool IsEnabled() const override { return Config.bEnabled; }

private:
	FSpatialAdapterConfig Config;
	ULiveOSCClientComponent* Client = nullptr;

	void SendObject(const FSpatialNormalizedState& State);

	/**
	 * Parse the packed label "<label>|<cueID>|<objectName>" produced by
	 * FProtocolRouter::FilterSnapshotForAdapter and return cueID/objectName.
	 */
	static bool ParsePackedLabel(const FString& PackedLabel,
	                              FString& OutCueID, FString& OutObjectName);
};
