// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "Adapters/DS100Adapter.h"
#include "SpatialMath.h"
#include "LiveOSCClientComponent.h"

void FDS100Adapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config = InConfig;
	CachedSendRateHz = InConfig.SendRateHz;
}

void FDS100Adapter::SetClientComponent(ULiveOSCClientComponent* InClient)
{
	Client = InClient;
}

void FDS100Adapter::ProcessFrame(const FSpatialFrameSnapshot& Snapshot)
{
	if (!Config.bEnabled || !Client) { return; }
	if (!ShouldSendThisFrame(1.f / 60.f)) { return; }

	if (!Client->IsConnected())
	{
		Client->Connect(Config.TargetIP, Config.TargetPort);
	}

	for (const FSpatialNormalizedState& State : Snapshot.Objects)
	{
		SendObject(State);
	}
}

void FDS100Adapter::SendObject(const FSpatialNormalizedState& State)
{
	if (!Client) { return; }

	const int32 ID = State.ObjectID + Config.DS100ChannelOffset;

	if (Config.bDS100AbsoluteMode)
	{
		// Absolute position in metres
		// /dbaudio1/positioning/source_position/{id}  x  y  z
		const FVector& M = State.StageMeters;
		const FString Addr = FString::Printf(
			TEXT("/dbaudio1/positioning/source_position/%d"), ID);
		Client->SendMultiArg(Addr, { (float)M.X, (float)M.Y, (float)M.Z });

		if (OnLog)
		{
			FSpatialFabricLogEntry Entry;
			Entry.Adapter   = TEXT("DS100");
			Entry.Direction = TEXT("OUT");
			Entry.Address   = Addr;
			Entry.ValueStr  = FString::Printf(TEXT("%.3fm %.3fm %.3fm"), M.X, M.Y, M.Z);
			FDateTime Now = FDateTime::Now();
			Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"),
				Now.GetHour(), Now.GetMinute(), Now.GetSecond());
			OnLog(Entry);
		}
	}
	else
	{
		// Coordinate-mapping mode [0..1]
		// /dbaudio1/coordinatemapping/source_position_xy/{area}/{id}  x  y
		const FVector2D Mapped = FSpatialMath::NormalizedToDS100Mapped(State.StageNormalized);
		const FString Addr = FString::Printf(
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
			FDateTime Now = FDateTime::Now();
			Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"),
				Now.GetHour(), Now.GetMinute(), Now.GetSecond());
			OnLog(Entry);
		}
	}

	// Spread: /dbaudio1/positioning/source_spread/{id}  s   [0..1]
	if (State.Width01 > 0.f)
	{
		Client->SendFloat(
			FString::Printf(TEXT("/dbaudio1/positioning/source_spread/%d"), ID),
			State.Width01);
	}
}
