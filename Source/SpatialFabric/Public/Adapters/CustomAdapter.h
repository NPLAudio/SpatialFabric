// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialProtocolAdapter.h"
#include "SpatialFabricTypes.h"

/**
 * FCustomAdapter
 *
 * Address and argument templates use simple string replacement — {id}, {x}, etc.
 * Parsed arg names are lowercased in ParseArgTemplate for consistent matching.
 *
 * Template-based OSC adapter for arbitrary protocols.
 * Uses CustomAddressTemplate and CustomArgTemplate from FSpatialAdapterConfig.
 *
 * Address placeholders: {id}, {x}, {y}, {z}, {gain}, {width}, {label},
 * plus any CustomFields keys as {key}.
 *
 * Arg template: space-separated names (x, y, z, gain, width, id) to send as floats.
 * Example: "x y z" sends StageNormalized.X, Y, Z.
 */
class SPATIALFABRIC_API FCustomAdapter : public ISpatialProtocolAdapter
{
public:
	FCustomAdapter() = default;

	virtual FName GetName() const override { return TEXT("Custom"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::Custom; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override;
	virtual void SetClientComponent(USpatialOSCClientComponent* InClient) override;
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime) override;
	virtual bool IsEnabled() const override { return Config.bEnabled; }

private:
	FSpatialAdapterConfig Config;
	USpatialOSCClientComponent* Client = nullptr;

	/** Parsed arg names from CustomArgTemplate (e.g. x, y, z). */
	TArray<FString> CachedArgNames;

	struct FCustomCachedState
	{
		FVector PosNorm = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
		float GainLinear = -999.f;
		float Width01 = -999.f;
		bool bEverSent = false;
	};
	TMap<int32, FCustomCachedState> LastSentByID;

	void ParseArgTemplate();
	FString ResolveAddress(const FSpatialNormalizedState& State, const FString* AxisOverride = nullptr) const;
	TArray<float> BuildArgs(const FSpatialNormalizedState& State) const;
	void SendObject(const FSpatialNormalizedState& State);
};
