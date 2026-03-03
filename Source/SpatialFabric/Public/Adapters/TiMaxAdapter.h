// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialProtocolAdapter.h"
#include "Adapters/ADMOSCAdapter.h"

/**
 * FTiMaxAdapter
 *
 * Routes spatial data to a TiMax2 SoundHub processor via ADM-OSC v1.0.
 * TiMax SoundHub implements the identical ADM-OSC v1.0 dictionary as
 * FADMOSCAdapter; this adapter is a thin delegation wrapper that
 * forwards all work to an internal FADMOSCAdapter instance.
 *
 * The only differences from FADMOSCAdapter are:
 *   - Default target port 7000 (configured by SpatialFabricManagerActor)
 *   - GetAdapterType() returns ESpatialAdapterType::TiMax
 *   - GetName() returns "TiMax"
 *   - Log entries tag the Adapter field as "TiMax"
 *
 * OSC messages sent per object (identical to FADMOSCAdapter):
 *   /adm/obj/{n}/xyz   x y z   (Cartesian, normalized [-1,1])
 *   /adm/obj/{n}/aed   azim elev dist   (Polar, if configured)
 *   /adm/obj/{n}/gain  float
 *   /adm/obj/{n}/w     float
 *   /adm/obj/{n}/mute  int32
 *   /adm/obj/{n}/name  string
 *   /adm/obj/{n}/dref  float   (when non-default)
 *   /adm/obj/{n}/dmax  float   (when set)
 * Listener (when present):
 *   /adm/lis/xyz   x y z
 *   /adm/lis/ypr   yaw pitch roll
 *
 * Default target port: 7000.
 */
class SPATIALFABRIC_API FTiMaxAdapter : public ISpatialProtocolAdapter
{
public:
	FTiMaxAdapter() = default;

	virtual FName GetName() const override { return TEXT("TiMax"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::TiMax; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override;
	virtual void SetClientComponent(USpatialOSCClientComponent* InClient) override;
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime) override;
	virtual void HandleIncomingOSC(const FString& Address, float Value) override;
	virtual bool IsEnabled() const override { return Config.bEnabled; }

	/** Returns true if a TiMax ADM-OSC reply has been received. */
	bool IsConnectionConfirmed() const { return InnerADM.IsConnectionConfirmed(); }
	/** Seconds since last inbound /adm/... message. -1 = never. */
	double GetSecondsSinceLastReply() const { return InnerADM.GetSecondsSinceLastReply(); }

private:
	FSpatialAdapterConfig Config;
	FADMOSCAdapter InnerADM;
};
