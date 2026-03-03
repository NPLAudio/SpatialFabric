// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "Adapters/SpaceMapGoAdapter.h"
#include "SpatialOSCClientComponent.h"

void FSpaceMapGoAdapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config = InConfig;
	CachedSendRateHz = InConfig.SendRateHz;
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
		SendSource(State);
	}
}

void FSpaceMapGoAdapter::SendSource(const FSpatialNormalizedState& State)
{
	if (!Client) { return; }

	const int32 ID = State.ObjectID;
	// SpaceMap Go coordinate mapping:
	//   xpan [-1,1]: left (-1) to right (+1) — our +Y = right, so pass Y directly
	//   ypan [-1,1]: back (-1) to front (+1) — our +X = front, so pass X directly
	const float XPan = FMath::Clamp((float)State.StageNormalized.Y, -1.f, 1.f);
	const float YPan = FMath::Clamp((float)State.StageNormalized.X, -1.f, 1.f);

	Client->SendFloat(FString::Printf(TEXT("/source/%d/xpan"), ID), XPan);
	Client->SendFloat(FString::Printf(TEXT("/source/%d/ypan"), ID), YPan);
	Client->SendFloat(FString::Printf(TEXT("/source/%d/spread"), ID),
		FMath::Clamp(State.Width01, 0.f, 1.f));

	if (OnLog)
	{
		FSpatialFabricLogEntry Entry;
		Entry.Adapter   = TEXT("SpaceMapGo");
		Entry.Direction = TEXT("OUT");
		Entry.Address   = FString::Printf(TEXT("/source/%d/xpan+ypan"), ID);
		Entry.ValueStr  = FString::Printf(TEXT("%.3f %.3f"), XPan, YPan);
		FDateTime Now = FDateTime::Now();
		Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"),
			Now.GetHour(), Now.GetMinute(), Now.GetSecond());
		OnLog(Entry);
	}
}

void FSpaceMapGoAdapter::RecallSnapshot(int32 SnapshotID)
{
	if (!Client || !Config.bEnabled) { return; }
	Client->Connect(Config.TargetIP, Config.TargetPort);
	Client->SendInt(TEXT("/Console/recall/system"), SnapshotID);
}
