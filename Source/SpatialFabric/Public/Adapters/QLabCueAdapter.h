// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialProtocolAdapter.h"

/**
 * FQLabCueAdapter  [Phase 2 stub]
 *
 * Sends QLab cue control commands (start, stop, go-to).
 * Unlike FQLabObjectAdapter, this adapter is event-driven rather than
 * per-frame: it fires only when triggered by a Blueprint or game event.
 *
 * OSC examples:
 *   /cue/selected/start
 *   /cue/{id}/start
 *   /cue/{id}/stop
 *
 * TODO (Phase 2): Implement cue trigger, cue-number binding, and
 *   positional cue recall based on spatial zone entry/exit.
 */
class SPATIALFABRIC_API FQLabCueAdapter : public ISpatialProtocolAdapter
{
public:
	FQLabCueAdapter() = default;

	virtual FName GetName() const override { return TEXT("QLabCue"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::QLabCue; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override { Config = InConfig; }
	virtual void SetClientComponent(ULiveOSCClientComponent* InClient) override { Client = InClient; }
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot) override { /* Phase 2 */ }
	virtual bool IsEnabled() const override { return Config.bEnabled; }

	/** Trigger a QLab cue by ID string (e.g. "10" or "Scene1"). */
	void TriggerCue(const FString& CueID);

private:
	FSpatialAdapterConfig Config;
	ULiveOSCClientComponent* Client = nullptr;
};
