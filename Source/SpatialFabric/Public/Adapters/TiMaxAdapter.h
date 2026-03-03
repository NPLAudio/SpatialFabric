// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialProtocolAdapter.h"

/**
 * FTiMaxAdapter  [Phase 2 stub]
 *
 * Routes spatial data to a TiMax SoundHub or panLab processor.
 * TiMax SoundHub v6+ supports ADM-OSC directly; earlier versions
 * accept user-defined OSC address templates.
 *
 * Default target port: 7000.
 *
 * Modes:
 *   - ADM-OSC mode:  forward normalized xyz/gain/w (same as FADMOSCAdapter)
 *   - Template mode: user defines custom OSC addresses (e.g. /timax/obj{id}/xy)
 *
 * TODO (Phase 2): Implement configurable address templates and
 *   ADM-OSC passthrough delegating to FADMOSCAdapter.
 */
class SPATIALFABRIC_API FTiMaxAdapter : public ISpatialProtocolAdapter
{
public:
	FTiMaxAdapter() = default;

	virtual FName GetName() const override { return TEXT("TiMax"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::TiMax; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override { Config = InConfig; }
	virtual void SetClientComponent(USpatialOSCClientComponent* InClient) override { Client = InClient; }
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime) override { /* Phase 2 */ }
	virtual bool IsEnabled() const override { return Config.bEnabled; }

	/**
	 * OSC address template for x/y position.
	 * Use {id} as placeholder for the object ID.
	 * Example: "/timax/object{id}/position"
	 */
	FString AddressTemplateXY = TEXT("/timax/object{id}/position");

private:
	FSpatialAdapterConfig Config;
	USpatialOSCClientComponent* Client = nullptr;
};
