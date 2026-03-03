// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialProtocolAdapter.h"
#include "SpatialFabricTypes.h"

/**
 * FADMOSCAdapter
 *
 * Implements the ADM-OSC v1.0 object metadata standard
 * (immersive-audio-live.github.io/ADM-OSC).
 *
 * Full message set per object:
 *   Position (configurable):
 *     Cartesian:  /adm/obj/{n}/xyz   x y z   (normalized, [-1..1])
 *     Polar:      /adm/obj/{n}/aed   azim elev dist
 *     Both:       sends xyz and aed
 *   Properties:
 *     /adm/obj/{n}/gain   float   linear amplitude
 *     /adm/obj/{n}/w      float   horizontal extent [0..1]
 *     /adm/obj/{n}/mute   int32   0 or 1
 *     /adm/obj/{n}/name   string  object label
 *     /adm/obj/{n}/dref   float   distance-ref [0..1] (if non-default)
 *     /adm/obj/{n}/dmax   float   max distance in meters (if set)
 *   Listener (when present):
 *     /adm/lis/xyz   x y z
 *     /adm/lis/ypr   yaw pitch roll
 *
 * Default target port: 9000 (L-ISA Controller default).
 * Recommended send rate: ≤50 Hz (per ADM-OSC spec).
 */
class SPATIALFABRIC_API FADMOSCAdapter : public ISpatialProtocolAdapter
{
public:
	FADMOSCAdapter() = default;

	virtual FName GetName() const override { return TEXT("ADMOSC"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::ADMOSC; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override;
	virtual void SetClientComponent(USpatialOSCClientComponent* InClient) override;
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime) override;
	virtual void HandleIncomingOSC(const FString& Address, float Value) override;
	virtual bool IsEnabled() const override { return Config.bEnabled; }

	/** Returns true if an ADM-OSC receiver has sent any message back to us. */
	bool IsConnectionConfirmed() const { return bConnectionConfirmed; }
	/** Seconds since last inbound ADM-OSC message was received. -1 = never. */
	double GetSecondsSinceLastReply() const { return SecondsSinceLastReply; }

private:
	FSpatialAdapterConfig Config;
	EADMCoordinateMode CoordMode = EADMCoordinateMode::Cartesian;
	USpatialOSCClientComponent* Client = nullptr;

	/** Set to true when any inbound /adm/... message is received. */
	bool bConnectionConfirmed = false;
	/** Wall-clock time of last received /adm/... message (FPlatformTime::Seconds()). -1 = never. */
	double LastReplyTime = -1.0;
	/** Seconds elapsed since LastReplyTime (updated in ProcessFrame). */
	double SecondsSinceLastReply = -1.0;

	/** Send all ADM-OSC v1.0 messages for one object. */
	void SendObject(const FSpatialNormalizedState& State);
	/** Send Cartesian position (/xyz). */
	void SendCartesian(int32 ID, const FVector& Norm);
	/** Send Polar position (/aed) computed from Cartesian. */
	void SendPolar(int32 ID, const FVector& Norm);
	/** Send listener position and orientation. */
	void SendListener(const FSpatialFrameSnapshot& Snapshot);
};
