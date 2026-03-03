// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "SpatialStageVolume.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

ASpatialStageVolume::ASpatialStageVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	// Run after character movement (PrePhysics) so CachedListenerPos is
	// up-to-date for the current frame when BuildSnapshot reads it.
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	StageBox = CreateDefaultSubobject<UBoxComponent>(TEXT("StageBox"));
	StageBox->SetBoxExtent(FVector(1000.f, 750.f, 400.f)); // default ~20m×15m×8m in cm
	StageBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StageBox->SetLineThickness(2.f);
	RootComponent = StageBox;
}

// ─── Coordinate helpers ────────────────────────────────────────────────────

FVector ASpatialStageVolume::ComputeLocalVector(FVector WorldPos) const
{
	const FVector Origin = CachedListenerPos;
	FVector Local = WorldPos - Origin;

	if (bListenerResolved && bListenerRelativeOrientation)
	{
		// Listener-relative orientation: axes follow the listener's facing direction.
		if (!CachedListenerRot.IsNearlyZero())
			Local = CachedListenerRot.UnrotateVector(Local);
	}
	else
	{
		// Stage-relative: transform into the box's local coordinate frame so that
		// dividing by GetScaledBoxExtent() (which is in box-local space) gives
		// correct [-1, 1] results regardless of the box's orientation in the level.
		// For an axis-aligned box this is a no-op.
		Local = StageBox->GetComponentQuat().UnrotateVector(Local);
	}

	return Local;
}

FVector ASpatialStageVolume::WorldToNormalized(FVector WorldPos) const
{
	const FVector Local = ComputeLocalVector(WorldPos);
	const FVector HalfExtentUU = StageBox->GetScaledBoxExtent(); // UE units

	FVector Norm;
	Norm.X = (HalfExtentUU.X > 0.f) ? (Local.X / HalfExtentUU.X) : 0.f;
	Norm.Y = (HalfExtentUU.Y > 0.f) ? (Local.Y / HalfExtentUU.Y) : 0.f;
	Norm.Z = (HalfExtentUU.Z > 0.f) ? (Local.Z / HalfExtentUU.Z) : 0.f;

	// Apply axis flips
	if (bFlipX) { Norm.X = -Norm.X; }
	if (bFlipY) { Norm.Y = -Norm.Y; }
	if (bFlipZ) { Norm.Z = -Norm.Z; }

	// Clamp to [-1, 1]
	return FVector(
		FMath::Clamp(Norm.X, -1.f, 1.f),
		FMath::Clamp(Norm.Y, -1.f, 1.f),
		FMath::Clamp(Norm.Z, -1.f, 1.f));
}

FVector ASpatialStageVolume::WorldToMeters(FVector WorldPos) const
{
	const FVector Norm = WorldToNormalized(WorldPos);
	return Norm * GetHalfExtentMeters();
}

void ASpatialStageVolume::ResolveListenerThisFrame()
{
	bListenerResolved = false;
	CachedListenerRot = FRotator::ZeroRotator;

	AActor* ResolvedActor = nullptr;

	// ── Priority 1: Auto-follow player (checkbox in Details, no code) ──────
	// Tick "Auto-Follow Player" in the Details panel — nothing else needed.
	if (bAutoFollowPlayer && GetWorld())
	{
		if (APlayerController* PC =
			UGameplayStatics::GetPlayerController(this, AutoFollowPlayerIndex))
		{
			ResolvedActor = PC->GetViewTarget(); // pawn in gameplay, cine cam in cutscenes
			// GetViewTarget() returns the PlayerController itself before a pawn is
			// possessed (early PIE frames).  Fall back to GetPawn() so that we always
			// resolve to an actor with a meaningful world location.
			if (!ResolvedActor || ResolvedActor == PC)
				ResolvedActor = PC->GetPawn();
		}
	}

	// ── Priority 2: explicit ListenerActor property ─────────────────────────
	if (!ResolvedActor && !ListenerActor.IsNull())
	{
		ResolvedActor = ListenerActor.Get();

		// Soft pointer may not be loaded yet — try a label-based scan
		if (!ResolvedActor && GetWorld())
		{
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				if ((*It)->GetActorLabel() == ListenerActor.ToSoftObjectPath().GetAssetName())
				{
					ResolvedActor = *It;
					break;
				}
			}
		}
	}

	// ── Priority 3: World Outliner child (drag-and-drop, no code needed) ───
	// Drag a camera actor onto the Stage Volume row in the World Outliner
	// so it appears indented as a child.
	if (!ResolvedActor)
	{
		TArray<AActor*> AttachedChildren;
		GetAttachedActors(AttachedChildren, /*bResetArray=*/true,
		                  /*bRecursivelyIncludeAttachedActors=*/false);
		if (AttachedChildren.Num() > 0)
		{
			ResolvedActor = AttachedChildren[0];
		}
	}

	// ── Apply resolved actor ───────────────────────────────────────────────
	if (ResolvedActor)
	{
		CachedListenerPos = ResolvedActor->GetActorLocation();
		if (bListenerRelativeOrientation)
		{
			CachedListenerRot = ResolvedActor->GetActorRotation();
		}
		bListenerResolved = true;

		// When auto-following the player, teleport the stage volume so its
		// centre tracks the listener each frame.  Box dimensions stay constant —
		// the audio bubble moves with the listener.
		if (bAutoFollowPlayer)
		{
			SetActorLocation(CachedListenerPos);
			if (bListenerRelativeOrientation)
				SetActorRotation(CachedListenerRot);
		}

		return;
	}

	// No listener: use the stage box world origin
	CachedListenerPos = StageBox->GetComponentLocation();
}

// ─── Listener assignment helpers ───────────────────────────────────────────

void ASpatialStageVolume::AssignListenerToPlayerPawn(int32 PlayerIndex)
{
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, PlayerIndex))
	{
		ListenerActor = Pawn;
	}
}

void ASpatialStageVolume::AssignListenerToPlayerCamera(int32 PlayerIndex)
{
	// GetViewTarget() returns the actor the camera is currently viewing —
	// the player pawn during normal gameplay, a cinematic camera during
	// cutscenes.  It is declared on APlayerController so no extra include
	// beyond PlayerController.h is needed.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, PlayerIndex))
	{
		if (AActor* ViewTarget = PC->GetViewTarget())
		{
			ListenerActor = ViewTarget;
		}
	}
}

void ASpatialStageVolume::ClearListener()
{
	ListenerActor = nullptr;
}

// ─── AActor overrides ──────────────────────────────────────────────────────

void ASpatialStageVolume::BeginPlay()
{
	Super::BeginPlay();
	ResolveListenerThisFrame();
}

void ASpatialStageVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ResolveListenerThisFrame();

#if ENABLE_DRAW_DEBUG
	// Draw labelled debug box in editor and PIE
	const FVector BoxCenter = StageBox->GetComponentLocation();
	const FVector HalfExtent = StageBox->GetScaledBoxExtent();
	const FQuat BoxRot = StageBox->GetComponentQuat();

	DrawDebugBox(GetWorld(), BoxCenter, HalfExtent, BoxRot,
		FColor(0, 200, 255), false, -1.f, 0, 2.f);

	// Stage label
	DrawDebugString(GetWorld(), BoxCenter + FVector(0, 0, HalfExtent.Z + 30.f),
		FString::Printf(TEXT("Stage Volume\n%.1fm x %.1fm x %.1fm"),
			PhysicalWidthMeters, PhysicalDepthMeters, PhysicalHeightMeters),
		nullptr, FColor::White, 0.f);

	// Listener indicator
	if (bListenerResolved)
	{
		DrawDebugSphere(GetWorld(), CachedListenerPos, 20.f, 8, FColor::Yellow, false, -1.f, 0, 2.f);
		DrawDebugString(GetWorld(), CachedListenerPos + FVector(0, 0, 40.f),
			TEXT("Listener"), nullptr, FColor::Yellow, 0.f);
	}
#endif
}

#if WITH_EDITOR
void ASpatialStageVolume::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
}

void ASpatialStageVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
