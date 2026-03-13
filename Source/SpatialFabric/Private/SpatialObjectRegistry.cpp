// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "SpatialObjectRegistry.h"
#include "SpatialFabricSettings.h"
#include "SpatialStageVolume.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

USpatialObjectRegistry::USpatialObjectRegistry()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FSpatialFrameSnapshot USpatialObjectRegistry::BuildSnapshot(
	const TArray<FSpatialObjectBinding>& Bindings,
	ASpatialStageVolume* Stage) const
{
	FSpatialFrameSnapshot Snapshot;
	Snapshot.Timestamp = FPlatformTime::Seconds();

	// Populate listener data if a stage volume with a listener is available
	if (Stage && Stage->HasListener())
	{
		const FVector ListenerWorld = Stage->GetListenerWorldPosition();
		Snapshot.ListenerNormalized = Stage->WorldToNormalized(ListenerWorld);
		Snapshot.ListenerYPR        = Stage->GetListenerYPR();
		Snapshot.bHasListener       = true;
	}

	for (int32 i = 0; i < Bindings.Num(); ++i)
	{
		const FSpatialObjectBinding& Binding = Bindings[i];
		if (!Binding.bEnabled) { continue; }

		AActor* Actor = ResolveActor(Binding.TargetActor, Binding.CachedActorLabel);
		if (!Actor)
		{
			if (!WarnedMissingActors.Contains(i))
			{
				if (GetDefault<USpatialFabricSettings>()->bEnableDebugMessages)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("SpatialFabric: Binding[%d] '%s' — target actor not found."),
						i, *Binding.Label);
				}
				WarnedMissingActors.Add(i);
			}
			continue;
		}

		WarnedMissingActors.Remove(i); // clear warning once resolved

		const FVector WorldPos = Actor->GetActorLocation();
		const FQuat   WorldRot = Actor->GetActorQuat();

		FSpatialNormalizedState State;
		State.Label      = Binding.Label.IsEmpty() ? Actor->GetActorNameOrLabel() : Binding.Label;
		State.ObjectID   = Binding.DefaultObjectID;
		State.GainLinear = Binding.GainLinear;
		State.Width01    = Binding.Width01;
		State.bMuted     = Binding.bMuted;
		State.Dref         = Binding.ADMDref;
		State.Dmax         = Binding.ADMDmax;
		State.bADMSendGain = Binding.bADMSendGain;
		State.bADMSendMute = Binding.bADMSendMute;
		State.bADMSendName = Binding.bADMSendName;
		State.Orientation = WorldRot;

		if (Stage)
		{
			State.StageNormalized = Stage->WorldToNormalized(WorldPos);
			State.StageMeters     = Stage->WorldToMeters(WorldPos);
		}
		else
		{
			// Fallback when no Stage Volume: normalize using manager location as
			// origin and default stage size (20×15×8 m). Produces ADM-OSC–spec
			// normalized coords [-1,1] and polar ranges (azim, elev, dist).
			const AActor* Owner = GetOwner();
			const FVector Origin = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
			const FVector HalfExtentUU(1000.f, 750.f, 400.f); // 10m×7.5m×4m half (20×15×8 full)
			const FVector Local = WorldPos - Origin;
			State.StageNormalized = FVector(
				FMath::Clamp((HalfExtentUU.X > 0.f) ? (Local.X / HalfExtentUU.X) : 0.f, -1.f, 1.f),
				FMath::Clamp((HalfExtentUU.Y > 0.f) ? (Local.Y / HalfExtentUU.Y) : 0.f, -1.f, 1.f),
				FMath::Clamp((HalfExtentUU.Z > 0.f) ? (Local.Z / HalfExtentUU.Z) : 0.f, -1.f, 1.f));
			State.StageMeters = State.StageNormalized * FVector(10.f, 7.5f, 4.f); // half-extent in m
		}

		Snapshot.Objects.Add(State);
	}

	return Snapshot;
}

AActor* USpatialObjectRegistry::ResolveActor(
	const TSoftObjectPtr<AActor>& SoftPtr,
	const FString& CachedLabel) const
{
	// Fast path: soft pointer already loaded
	if (!SoftPtr.IsNull())
	{
		AActor* Actor = SoftPtr.Get();
		if (Actor) { return Actor; }
	}

	// Fallback: scan by cached label
	if (!CachedLabel.IsEmpty() && GetWorld())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if ((*It)->GetActorNameOrLabel() == CachedLabel)
			{
				return *It;
			}
		}
	}

	return nullptr;
}
