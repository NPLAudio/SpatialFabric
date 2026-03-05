// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpatialStageVolume.generated.h"

class UBoxComponent;

/**
 * How the stage volume tracks a listener (player or assigned actor).
 *
 *  Off                          — Stage is fixed at its design-time location.
 *                                 All coordinates are relative to the box center.
 *
 *  FollowPositionAndOrientation — Stage origin AND axes follow the listener.
 *                                 "Forward" always equals the listener's facing direction.
 *                                 Use for fully listener-relative coordinate output.
 */
UENUM(BlueprintType)
enum class EListenerMode : uint8
{
	Off                          UMETA(DisplayName = "Off (stage-fixed)"),
	FollowPositionAndOrientation UMETA(DisplayName = "Follow Position & Orientation"),
};

/**
 * ASpatialStageVolume
 *
 * A box-shaped level actor that defines the physical audio coordinate space.
 * All SpatialFabric object positions are normalized relative to this volume:
 * an object at the volume center maps to (0,0,0); an object at the +X face
 * maps to (1,0,0), etc.  Objects outside the volume are clamped to ±1.
 *
 * The box component's extent (half-size) in UE units determines the coordinate
 * space.  PhysicalWidth/Depth/HeightMeters tell adapters the real-world scale
 * for absolute-position output (e.g. DS100 absolute mode).
 *
 * Set ListenerMode to follow the player camera or an assigned actor, enabling
 * listener-relative coordinate output.  ADM-OSC listener messages
 * (/adm/lis/xyz, /adm/lis/ypr) are also emitted when a listener is active.
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

	// ── Physical dimensions (meters) ────────────────────────────────────────

	/**
	 * Physical width of the audio space in meters (maps to the box's X axis).
	 * Used by DS100 absolute mode to output real-world coordinates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Physical Dimensions",
		meta = (ClampMin = "0.1"))
	float PhysicalWidthMeters = 20.f;

	/** Physical depth of the audio space in meters (maps to the box's Y axis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Physical Dimensions",
		meta = (ClampMin = "0.1"))
	float PhysicalDepthMeters = 15.f;

	/** Physical height of the audio space in meters (maps to the box's Z axis). */
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

	// ── Listener / player mode ──────────────────────────────────────────────

	/**
	 * Controls whether the stage tracks a listener (player or actor).
	 *
	 *  Off                          — stage-fixed; coordinates relative to box center.
	 *  Follow Position & Orientation — origin AND axes follow the listener.
	 *
	 * When not Off, the listener is resolved in priority order:
	 *   1. Local player camera/view-target.
	 *   2. ListenerActor property (if set).
	 *   3. First World Outliner child of this actor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Listener")
	EListenerMode ListenerMode = EListenerMode::Off;

	/**
	 * Which local player to follow (0 = first player).
	 * Only used when ListenerMode is not Off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Listener",
		meta = (ClampMin = "0"))
	int32 ListenerPlayerIndex = 0;

	/**
	 * Optional explicit actor to use as the listener origin.
	 * Only consulted when the player controller cannot be resolved.
	 * Ignored when ListenerMode is Off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Listener")
	TSoftObjectPtr<AActor> ListenerActor;

	/**
	 * Assign the player pawn (character/vehicle) as the listener actor.
	 * Call at BeginPlay or from a Blueprint event to follow the player pawn.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage|Listener",
		meta = (DisplayName = "Assign Listener to Player Pawn"))
	void AssignListenerToPlayerPawn(int32 PlayerIndex = 0);

	/**
	 * Assign the player camera manager as the listener actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage|Listener",
		meta = (DisplayName = "Assign Listener to Player Camera"))
	void AssignListenerToPlayerCamera(int32 PlayerIndex = 0);

	/**
	 * Clear the listener actor assignment and set ListenerMode to Off.
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
	 * Transform a world-space point to physical meters relative to stage origin.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stage")
	FVector WorldToMeters(FVector WorldPos) const;

	/**
	 * Returns the half-extent in physical meters per axis: (W/2, D/2, H/2).
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

	/** Returns the cached listener world position (valid after ResolveListenerThisFrame). */
	FVector GetListenerWorldPosition() const { return CachedListenerPos; }

	/** Returns the cached listener rotation (valid after ResolveListenerThisFrame). */
	FRotator GetListenerRotation() const { return CachedListenerRot; }

	/** Returns true if a listener actor is currently assigned and resolved. */
	bool HasListener() const { return bListenerResolved; }

	/** Returns the listener's YPR (yaw/pitch/roll) as a rotator for ADM-OSC. */
	FRotator GetListenerYPR() const { return CachedListenerRot; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	FVector  CachedListenerPos  = FVector::ZeroVector;
	FRotator CachedListenerRot  = FRotator::ZeroRotator;
	bool     bListenerResolved  = false;

	/** World location/rotation of the stage box at BeginPlay — restored when ListenerMode is Off. */
	FVector  DesignLocation = FVector::ZeroVector;
	FRotator DesignRotation = FRotator::ZeroRotator;

	FVector ComputeLocalVector(FVector WorldPos) const;
};
