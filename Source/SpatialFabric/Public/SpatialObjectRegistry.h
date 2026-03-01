// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpatialFabricTypes.h"
#include "SpatialObjectRegistry.generated.h"

class ASpatialStageVolume;

/**
 * USpatialObjectRegistry
 *
 * Actor component owned by ASpatialFabricManagerActor.
 * Each tick it iterates the manager's FSpatialObjectBinding array, resolves
 * each TargetActor soft pointer, reads its world transform, and packs the
 * results (after stage-volume normalization) into an FSpatialFrameSnapshot
 * that the ProtocolRouter consumes.
 *
 * Soft-pointer resolution uses the same CachedActorLabel fallback pattern
 * as LiveOSC's dispatcher and transmitter.
 */
UCLASS(ClassGroup = "SpatialFabric",
	meta = (BlueprintSpawnableComponent, DisplayName = "Spatial Object Registry"))
class SPATIALFABRIC_API USpatialObjectRegistry : public UActorComponent
{
	GENERATED_BODY()

public:
	USpatialObjectRegistry();

	/**
	 * Build one immutable FSpatialFrameSnapshot from the current bindings.
	 * Called by ASpatialFabricManagerActor::Tick.
	 *
	 * @param Bindings   The manager's object bindings array (read-only).
	 * @param Stage      The stage volume used for coordinate normalization.
	 *                   May be null — positions will be in raw UE cm if so.
	 */
	FSpatialFrameSnapshot BuildSnapshot(
		const TArray<FSpatialObjectBinding>& Bindings,
		ASpatialStageVolume* Stage) const;

private:
	/** Attempt to resolve a soft actor pointer by label in the owning world. */
	AActor* ResolveActor(const TSoftObjectPtr<AActor>& SoftPtr,
	                     const FString& CachedLabel) const;

	/** Set of binding indices for which a missing-actor warning was already logged. */
	mutable TSet<int32> WarnedMissingActors;
};
