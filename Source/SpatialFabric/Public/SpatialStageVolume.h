// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpatialStageVolume.generated.h"

class UBoxComponent;

/**
 * ASpatialStageVolume
 *
 * A box-shaped level actor that defines the physical audio coordinate space.
 * All SpatialFabric object positions are normalized relative to this volume:
 * an object at the volume centre maps to (0,0,0); an object at the +X face
 * maps to (1,0,0), etc.  Objects outside the volume are clamped to ±1.
 *
 * The box component's extent (half-size) in UE units determines the coordinate
 * space.  PhysicalWidth/Depth/HeightMeters tell adapters the real-world scale
 * for absolute-position output (e.g. RTTrPM, DS100 absolute mode).
 *
 * When ListenerActor is assigned, the stage origin and axes follow the
 * listener's world position and rotation each frame, enabling listener-relative
 * ("binaural") coordinate output.  ADM-OSC listener messages (/adm/lis/xyz,
 * /adm/lis/ypr) are also emitted when a listener actor is present.
 *
 * Place one ASpatialStageVolume in the level and reference it from
 * ASpatialFabricManagerActor::StageVolume.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Spatial Stage Volume"))
class SPATIALFABRIC_API ASpatialStageVolume : public AActor
{
	GENERATED_BODY()

public:
	ASpatialStageVolume();

	// ── Stage box ───────────────────────────────────────────────────────────

	/** Box component whose extent defines the coordinate space in UE units. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage")
	TObjectPtr<UBoxComponent> StageBox;

	// ── Physical dimensions (metres) ────────────────────────────────────────

	/**
	 * Physical width of the audio space in metres (maps to the box's X axis).
	 * Used by DS100 absolute mode to output real-world coordinates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Physical Dimensions",
		meta = (ClampMin = "0.1"))
	float PhysicalWidthMeters = 20.f;

	/** Physical depth of the audio space in metres (maps to the box's Y axis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Physical Dimensions",
		meta = (ClampMin = "0.1"))
	float PhysicalDepthMeters = 15.f;

	/** Physical height of the audio space in metres (maps to the box's Z axis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Physical Dimensions",
		meta = (ClampMin = "0.1"))
	float PhysicalHeightMeters = 8.f;

	// ── Axis remapping ──────────────────────────────────────────────────────

	/**
	 * Flip the X axis output.  Use when the audio system's front-positive
	 * convention is opposite to the stage box's local X direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Axis Remap")
	bool bFlipX = false;

	/** Flip the Y axis output (left/right mirror). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Axis Remap")
	bool bFlipY = false;

	/** Flip the Z axis output (vertical mirror). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Axis Remap")
	bool bFlipZ = false;

	// ── Listener reference ──────────────────────────────────────────────────

	/**
	 * Automatically follow the local player's camera/pawn each frame.
	 * Tick this box in the Details panel — no code, no dragging required.
	 * Uses AutoFollowPlayerIndex to select which player (0 = first player).
	 * The active view target is tracked, so cinematic cameras work too.
	 * Takes priority over ListenerActor when enabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Listener",
		meta = (DisplayName = "Auto-Follow Player"))
	bool bAutoFollowPlayer = false;

	/**
	 * Which local player to follow when bAutoFollowPlayer is enabled.
	 * 0 = first player (the default for single-player projects).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Listener",
		meta = (EditCondition = "bAutoFollowPlayer", ClampMin = "0"))
	int32 AutoFollowPlayerIndex = 0;

	/**
	 * Optional actor whose position is used as the coordinate origin each frame
	 * instead of the stage box centre.  Set via the Details panel property
	 * picker, or call AssignListenerToPlayerPawn / AssignListenerToPlayerCamera
	 * from a Blueprint BeginPlay event.
	 *
	 * Ignored when bAutoFollowPlayer is enabled.
	 * Overridden by any actor attached as a World Outliner child when empty.
	 *
	 * When set, the listener's world location replaces the box origin, and
	 * (if bListenerRelativeOrientation is true) the listener's rotation is
	 * factored in so that "forward" always aligns with the listener's facing
	 * direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Listener",
		meta = (EditCondition = "!bAutoFollowPlayer"))
	TSoftObjectPtr<AActor> ListenerActor;

	/**
	 * When true and a listener is active, object positions are expressed
	 * in the listener's local frame (forward = listener facing direction).
	 * When false, only the listener's position shifts the origin; axes remain
	 * world-aligned (useful for head-tracked binaural where the renderer
	 * handles yaw itself).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Listener")
	bool bListenerRelativeOrientation = false;

	/**
	 * Assign the player pawn (character/vehicle) as the listener actor.
	 * Call at BeginPlay or from a Blueprint event to follow the player pawn.
	 * PlayerIndex is the local player index (0 for the first player).
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage|Listener",
		meta = (DisplayName = "Assign Listener to Player Pawn"))
	void AssignListenerToPlayerPawn(int32 PlayerIndex = 0);

	/**
	 * Assign the player camera manager as the listener actor.
	 * The camera manager follows the active view target (including cinematic
	 * cameras), making it ideal for first-person or binaural audio tracking.
	 * PlayerIndex is the local player index (0 for the first player).
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage|Listener",
		meta = (DisplayName = "Assign Listener to Player Camera"))
	void AssignListenerToPlayerCamera(int32 PlayerIndex = 0);

	/**
	 * Clear the listener actor assignment so the stage box origin is used
	 * as the coordinate origin (standard stage-fixed mode).
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage|Listener",
		meta = (DisplayName = "Clear Listener"))
	void ClearListener();

	// ── Coordinate helpers (called each frame by USpatialObjectRegistry) ────

	/**
	 * Transform a world-space point to stage-normalized coordinates [-1..1].
	 * Applies listener offset (if set), axis flips, and clamps to ±1.
	 * Must be called after ResolveListenerThisFrame().
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage")
	FVector WorldToNormalized(FVector WorldPos) const;

	/**
	 * Transform a world-space point to physical metres relative to stage origin.
	 * StageMeters = WorldToNormalized(WorldPos) * HalfExtentMeters.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage")
	FVector WorldToMeters(FVector WorldPos) const;

	/**
	 * Returns the half-extent in physical metres per axis: (W/2, D/2, H/2).
	 * Used by adapters to scale normalized coordinates to physical distances.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage")
	FVector GetHalfExtentMeters() const
	{
		return FVector(PhysicalWidthMeters * 0.5f,
		               PhysicalDepthMeters * 0.5f,
		               PhysicalHeightMeters * 0.5f);
	}

	/**
	 * Cache the listener actor's current position and rotation for this frame.
	 * Call once per tick before any WorldToNormalized() calls.
	 */
	void ResolveListenerThisFrame();

	/**
	 * Returns the cached listener world position (valid after ResolveListenerThisFrame).
	 * If no listener actor is assigned, returns the stage box world origin.
	 */
	FVector GetListenerWorldPosition() const { return CachedListenerPos; }

	/**
	 * Returns the cached listener rotation (valid after ResolveListenerThisFrame).
	 * Returns FRotator::ZeroRotator when no listener actor is set or
	 * bListenerRelativeOrientation is false.
	 */
	FRotator GetListenerRotation() const { return CachedListenerRot; }

	/** Returns true if a listener actor is currently assigned and resolved. */
	bool HasListener() const { return bListenerResolved; }

	/** Returns the listener's YPR (yaw/pitch/roll) as a rotator for ADM-OSC. */
	FRotator GetListenerYPR() const { return CachedListenerRot; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	/** Draw a labelled debug box in the editor viewport. */
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Cached listener world position (updated by ResolveListenerThisFrame). */
	FVector CachedListenerPos = FVector::ZeroVector;
	/** Cached listener rotation (updated by ResolveListenerThisFrame). */
	FRotator CachedListenerRot = FRotator::ZeroRotator;
	/** True if a listener actor was successfully resolved this frame. */
	bool bListenerResolved = false;

	/** Compute the local-space vector from the current origin to WorldPos. */
	FVector ComputeLocalVector(FVector WorldPos) const;
};
