// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "Adapters/SpaceMapGoAdapter.h"
#include "SpatialOSCClientComponent.h"

void FSpaceMapGoAdapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config = InConfig;
	CachedSendRateHz = InConfig.SendRateHz;
	LastSentByID.Reset();
}

void FSpaceMapGoAdapter::SetClientComponent(USpatialOSCClientComponent* InClient)
{
	Client = InClient;
}

void FSpaceMapGoAdapter::ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime)
{
	if (!Config.bEnabled || !Client) { return; }
	if (!ShouldSendThisFrame(DeltaTime)) { return; }

	Client->Connect(Config.TargetIP, Config.TargetPort);

	for (const FSpatialNormalizedState& State : Snapshot.Objects)
	{
		if (Config.bSendOnlyOnChange)
		{
			FSpaceMapGoCachedState& Cache = LastSentByID.FindOrAdd(State.ObjectID);
			const float Spread = FMath::Clamp(State.Width01 * 100.f, 0.f, 100.f);
			const bool bPosChanged = !State.StageNormalized.Equals(Cache.PosNorm, UE_KINDA_SMALL_NUMBER);
			const bool bSpreadChanged = FMath::Abs(Spread - Cache.Spread) > UE_KINDA_SMALL_NUMBER;
			if (!bPosChanged && !bSpreadChanged && Cache.bEverSent)
			{
				continue;
			}
			Cache.PosNorm = State.StageNormalized;
			Cache.Spread = Spread;
			Cache.bEverSent = true;
		}
		SendSource(State);
	}
}

void FSpaceMapGoAdapter::SendSource(const FSpatialNormalizedState& State)
{
	if (!Client) { return; }

	const int32 ID = State.ObjectID;
	// SpaceMapGo coordinate mapping (range -1000 to 1000):
	//   X: left (-1000) to right (+1000) — our +Y = right
	//   Y: back (-1000) to front (+1000) — our +X = front
	const float SMX = FMath::Clamp(State.StageNormalized.Y * 1000.f, -1000.f, 1000.f);
	const float SMY = FMath::Clamp(State.StageNormalized.X * 1000.f, -1000.f, 1000.f);
	// Spread: range 0–100 (logarithmic). 14 = optimal default per SpaceMapGo spec.
	const float Spread = FMath::Clamp(State.Width01 * 100.f, 0.f, 100.f);

	const FString PosAddr = FString::Printf(TEXT("/channel/%d/position"), ID);
	Client->SendMultiArg(PosAddr, { SMX, SMY });
	Client->SendFloat(FString::Printf(TEXT("/channel/%d/spread"), ID), Spread);

	if (OnLog)
	{
		FSpatialFabricLogEntry Entry;
		Entry.Adapter   = TEXT("SpaceMapGo");
		Entry.Direction = TEXT("OUT");
		Entry.Address   = PosAddr;
		Entry.ValueStr  = FString::Printf(TEXT("%.0f %.0f  spread %.0f"), SMX, SMY, Spread);
		FDateTime Now = FDateTime::Now();
		Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"),
			Now.GetHour(), Now.GetMinute(), Now.GetSecond());
		OnLog(Entry);
	}
}
