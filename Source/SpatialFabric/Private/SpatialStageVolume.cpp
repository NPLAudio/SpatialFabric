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
	FVector Local = WorldPos - CachedListenerPos;

	if (bListenerResolved && ListenerMode == EListenerMode::FollowPositionAndOrientation)
	{
		if (!CachedListenerRot.IsNearlyZero())
			Local = CachedListenerRot.UnrotateVector(Local);
	}
	else
	{
		Local = StageBox->GetComponentQuat().UnrotateVector(Local);
	}

	return Local;
}

FVector ASpatialStageVolume::WorldToNormalized(FVector WorldPos) const
{
	const FVector Local = ComputeLocalVector(WorldPos);
	const FVector HalfExtentUU = StageBox->GetScaledBoxExtent();

	FVector Norm;
	Norm.X = (HalfExtentUU.X > 0.f) ? (Local.X / HalfExtentUU.X) : 0.f;
	Norm.Y = (HalfExtentUU.Y > 0.f) ? (Local.Y / HalfExtentUU.Y) : 0.f;
	Norm.Z = (HalfExtentUU.Z > 0.f) ? (Local.Z / HalfExtentUU.Z) : 0.f;

	if (bFlipX) { Norm.X = -Norm.X; }
	if (bFlipY) { Norm.Y = -Norm.Y; }
	if (bFlipZ) { Norm.Z = -Norm.Z; }

	return FVector(
		FMath::Clamp(Norm.X, -1.f, 1.f),
		FMath::Clamp(Norm.Y, -1.f, 1.f),
		FMath::Clamp(Norm.Z, -1.f, 1.f));
}

FVector ASpatialStageVolume::WorldToMeters(FVector WorldPos) const
{
	return WorldToNormalized(WorldPos) * GetHalfExtentMeters();
}

void ASpatialStageVolume::ResolveListenerThisFrame()
{
	bListenerResolved = false;
	CachedListenerRot = FRotator::ZeroRotator;

	// ── Off: restore design location and use box origin ────────────────────
	if (ListenerMode == EListenerMode::Off)
	{
		if (!GetActorLocation().Equals(DesignLocation, 0.1f))
		{
			SetActorLocation(DesignLocation);
			SetActorRotation(DesignRotation);
		}
		CachedListenerPos = StageBox->GetComponentLocation();
		return;
	}

	// ── Resolve the listener actor ─────────────────────────────────────────
	AActor* ResolvedActor = nullptr;

	// Priority 1: local player view target
	if (GetWorld())
	{
		if (APlayerController* PC =
			UGameplayStatics::GetPlayerController(this, ListenerPlayerIndex))
		{
			ResolvedActor = PC->GetViewTarget();
			if (!ResolvedActor || ResolvedActor == PC)
				ResolvedActor = PC->GetPawn();
		}
	}

	// Priority 2: explicit ListenerActor property
	if (!ResolvedActor && !ListenerActor.IsNull())
	{
		ResolvedActor = ListenerActor.Get();
		if (!ResolvedActor && GetWorld())
		{
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				if ((*It)->GetActorNameOrLabel() == ListenerActor.ToSoftObjectPath().GetAssetName())
				{
					ResolvedActor = *It;
					break;
				}
			}
		}
	}

	// Priority 3: first World Outliner child
	if (!ResolvedActor)
	{
		TArray<AActor*> AttachedChildren;
		GetAttachedActors(AttachedChildren, true, false);
		if (AttachedChildren.Num() > 0)
			ResolvedActor = AttachedChildren[0];
	}

	// ── Apply resolved actor ───────────────────────────────────────────────
	if (ResolvedActor)
	{
		CachedListenerPos = ResolvedActor->GetActorLocation();
		if (ListenerMode == EListenerMode::FollowPositionAndOrientation)
			CachedListenerRot = ResolvedActor->GetActorRotation();
		bListenerResolved = true;

		// Move the stage box so it stays centered on the listener.
		SetActorLocation(CachedListenerPos);
		if (ListenerMode == EListenerMode::FollowPositionAndOrientation)
			SetActorRotation(CachedListenerRot);
		return;
	}

	// Listener mode is on but nothing resolved — fall back to design location.
	CachedListenerPos = StageBox->GetComponentLocation();
}

// ─── Listener assignment helpers ───────────────────────────────────────────

void ASpatialStageVolume::AssignListenerToPlayerPawn(int32 PlayerIndex)
{
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, PlayerIndex))
		ListenerActor = Pawn;
}

void ASpatialStageVolume::AssignListenerToPlayerCamera(int32 PlayerIndex)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, PlayerIndex))
		if (AActor* ViewTarget = PC->GetViewTarget())
			ListenerActor = ViewTarget;
}

void ASpatialStageVolume::ClearListener()
{
	ListenerActor = nullptr;
	ListenerMode  = EListenerMode::Off;
}

// ─── AActor overrides ──────────────────────────────────────────────────────

void ASpatialStageVolume::BeginPlay()
{
	Super::BeginPlay();
	DesignLocation = GetActorLocation();
	DesignRotation = GetActorRotation();
	ResolveListenerThisFrame();
}

void ASpatialStageVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ResolveListenerThisFrame();

#if ENABLE_DRAW_DEBUG
	// Skip debug draw when hidden (via "Hide stage in PIE" checkbox)
	if (!bHideDebugDraw && (!GetRootComponent() || !GetRootComponent()->bHiddenInGame))
	{
		const FVector BoxCenter  = StageBox->GetComponentLocation();
		const FVector HalfExtent = StageBox->GetScaledBoxExtent();
		const FQuat   BoxRot     = StageBox->GetComponentQuat();

		DrawDebugBox(GetWorld(), BoxCenter, HalfExtent, BoxRot,
			FColor(0, 200, 255), false, -1.f, 0, 2.f);

		DrawDebugString(GetWorld(), BoxCenter + FVector(0, 0, HalfExtent.Z + 30.f),
			FString::Printf(TEXT("Stage Volume\n%.1fm x %.1fm x %.1fm"),
				PhysicalWidthMeters, PhysicalDepthMeters, PhysicalHeightMeters),
			nullptr, FColor::White, 0.f);

		if (bListenerResolved)
		{
			DrawDebugSphere(GetWorld(), CachedListenerPos, 20.f, 8, FColor::Yellow, false, -1.f, 0, 2.f);
			DrawDebugString(GetWorld(), CachedListenerPos + FVector(0, 0, 40.f),
				TEXT("Listener"), nullptr, FColor::Yellow, 0.f);
		}
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
