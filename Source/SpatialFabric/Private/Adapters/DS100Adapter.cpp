// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "Adapters/DS100Adapter.h"
#include "SpatialMath.h"
#include "LiveOSCClientComponent.h"

void FDS100Adapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config           = InConfig;
	CachedSendRateHz = InConfig.SendRateHz;
	LastSentByID.Reset(); // invalidate cache when mode / settings change
}

void FDS100Adapter::SetClientComponent(ULiveOSCClientComponent* InClient)
{
	Client = InClient;
}

void FDS100Adapter::SetBindings(const TArray<FSpatialObjectBinding>& Bindings)
{
	CachedBindingsByObjectID.Reset();
	for (const FSpatialObjectBinding& B : Bindings)
	{
		if (!B.bEnabled) { continue; }
		const int32 Key = B.DefaultObjectID + Config.DS100ChannelOffset;
		CachedBindingsByObjectID.Add(Key, B);
	}
}

void FDS100Adapter::ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime)
{
	if (!Config.bEnabled || !Client) { return; }
	if (!ShouldSendThisFrame(DeltaTime)) { return; }

	if (!Client->IsConnected())
	{
		Client->Connect(Config.TargetIP, Config.TargetPort);
	}

	for (const FSpatialNormalizedState& State : Snapshot.Objects)
	{
		SendObject(State, Snapshot);
	}
}

// ── Private helpers ────────────────────────────────────────────────────────────

float FDS100Adapter::ComputeSpread(const FSpatialNormalizedState& State,
                                    const FVector& ListenerNorm) const
{
	const FSpatialObjectBinding* Binding = CachedBindingsByObjectID.Find(State.ObjectID);
	if (!Binding || Binding->DS100SpreadMode == EDS100SpreadMode::Fixed)
	{
		return State.Width01;
	}

	// Proximity mode: inverse-square curve in stage-normalized space.
	// t=0 (listener at source) → MaxSpread; t>=1 (at or past MaxDistance) → MinSpread.
	const float Dist      = FVector::Dist(State.StageNormalized, ListenerNorm);
	const float MaxDist   = FMath::Max(Binding->DS100ProximityMaxDistance, 0.01f);
	const float t         = FMath::Clamp(Dist / MaxDist, 0.f, 1.f);
	const float Proximity = 1.f - t * t; // 1 at source, 0 at MaxDist
	return FMath::Lerp(Binding->DS100SpreadMin, Binding->DS100SpreadMax, Proximity);
}

void FDS100Adapter::SendObject(const FSpatialNormalizedState& State,
                                const FSpatialFrameSnapshot& Snapshot)
{
	if (!Client) { return; }

	const int32 ID = State.ObjectID + Config.DS100ChannelOffset;

	FDS100LastSent& Cache = LastSentByID.FindOrAdd(State.ObjectID);

	FDateTime Now = FDateTime::Now();
	const FString Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"),
		Now.GetHour(), Now.GetMinute(), Now.GetSecond());

	// ── Position (only if moved) ──────────────────────────────────────────────

	if (!State.StageNormalized.Equals(Cache.PosNorm, UE_KINDA_SMALL_NUMBER))
	{
		Cache.PosNorm = State.StageNormalized;

		if (Config.bDS100AbsoluteMode)
		{
			// /dbaudio1/positioning/source_position/{id}  x  y  z
			const FVector& M   = State.StageMeters;
			const FString  Addr = FString::Printf(
				TEXT("/dbaudio1/positioning/source_position/%d"), ID);
			Client->SendMultiArg(Addr, { (float)M.X, (float)M.Y, (float)M.Z });

			if (OnLog)
			{
				FSpatialFabricLogEntry Entry;
				Entry.Adapter   = TEXT("DS100");
				Entry.Direction = TEXT("OUT");
				Entry.Address   = Addr;
				Entry.ValueStr  = FString::Printf(TEXT("%.3f %.3f %.3f"), M.X, M.Y, M.Z);
				Entry.Timestamp = Timestamp;
				OnLog(Entry);
			}
		}
		else
		{
			// /dbaudio1/coordinatemapping/source_position_xy/{area}/{id}  x  y
			const FVector2D Mapped = FSpatialMath::NormalizedToDS100Mapped(State.StageNormalized);
			const FString   Addr   = FString::Printf(
				TEXT("/dbaudio1/coordinatemapping/source_position_xy/%d/%d"),
				CoordMappingArea, ID);
			Client->SendMultiArg(Addr, { (float)Mapped.X, (float)Mapped.Y });

			if (OnLog)
			{
				FSpatialFabricLogEntry Entry;
				Entry.Adapter   = TEXT("DS100");
				Entry.Direction = TEXT("OUT");
				Entry.Address   = Addr;
				Entry.ValueStr  = FString::Printf(TEXT("%.3f %.3f"), Mapped.X, Mapped.Y);
				Entry.Timestamp = Timestamp;
				OnLog(Entry);
			}
		}
	}

	// ── Spread (only if changed) ──────────────────────────────────────────────
	// Fixed mode uses Width01; Proximity mode uses inverse-square listener distance.
	// Proximity spread can change when the listener moves even if the object is static.

	const float Spread = ComputeSpread(State, Snapshot.ListenerNormalized);
	if (Spread != Cache.Spread)
	{
		Cache.Spread = Spread;

		const FString SpreadAddr = FString::Printf(
			TEXT("/dbaudio1/positioning/source_spread/%d"), ID);
		Client->SendFloat(SpreadAddr, Spread);

		if (OnLog)
		{
			FSpatialFabricLogEntry Entry;
			Entry.Adapter   = TEXT("DS100");
			Entry.Direction = TEXT("OUT");
			Entry.Address   = SpreadAddr;
			Entry.ValueStr  = FString::Printf(TEXT("%.3f"), Spread);
			Entry.Timestamp = Timestamp;
			OnLog(Entry);
		}
	}

	// ── Delay mode (only if changed) ──────────────────────────────────────────

	if (const FSpatialObjectBinding* B = CachedBindingsByObjectID.Find(State.ObjectID))
	{
		if (B->DS100DelayMode >= 0 && B->DS100DelayMode != Cache.DelayMode)
		{
			Cache.DelayMode = B->DS100DelayMode;

			const FString DelayAddr = FString::Printf(
				TEXT("/dbaudio1/positioning/source_delaymode/%d"), ID);
			Client->SendInt(DelayAddr, B->DS100DelayMode);

			if (OnLog)
			{
				FSpatialFabricLogEntry Entry;
				Entry.Adapter   = TEXT("DS100");
				Entry.Direction = TEXT("OUT");
				Entry.Address   = DelayAddr;
				Entry.ValueStr  = FString::Printf(TEXT("%d"), B->DS100DelayMode);
				Entry.Timestamp = Timestamp;
				OnLog(Entry);
			}
		}
	}
}
