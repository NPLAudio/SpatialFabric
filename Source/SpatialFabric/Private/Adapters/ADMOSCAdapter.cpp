// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "Adapters/ADMOSCAdapter.h"
#include "LiveOSCClientComponent.h"

void FADMOSCAdapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config = InConfig;
	CachedSendRateHz = InConfig.SendRateHz;
}

void FADMOSCAdapter::SetClientComponent(ULiveOSCClientComponent* InClient)
{
	Client = InClient;
}

void FADMOSCAdapter::ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime)
{
	if (!Config.bEnabled || !Client) { return; }
	if (!ShouldSendThisFrame(DeltaTime)) { return; }

	if (!Client->IsConnected())
	{
		Client->Connect(Config.TargetIP, Config.TargetPort);
	}

	for (const FSpatialNormalizedState& State : Snapshot.Objects)
	{
		SendObject(State);
	}

	if (Snapshot.bHasListener)
	{
		SendListener(Snapshot);
	}
}

void FADMOSCAdapter::SendObject(const FSpatialNormalizedState& State)
{
	if (!Client) { return; }

	const int32 ID = State.ObjectID;
	const FVector& P = State.StageNormalized;

	// /adm/obj/{n}/xyz  x  y  z   (normalized, each axis [-1..1])
	{
		const FString Addr = FString::Printf(TEXT("/adm/obj/%d/xyz"), ID);
		Client->SendMultiArg(Addr, { (float)P.X, (float)P.Y, (float)P.Z });

		if (OnLog)
		{
			FSpatialFabricLogEntry Entry;
			Entry.Adapter   = TEXT("ADMOSC");
			Entry.Direction = TEXT("OUT");
			Entry.Address   = Addr;
			Entry.ValueStr  = FString::Printf(TEXT("%.3f %.3f %.3f"), P.X, P.Y, P.Z);
			FDateTime Now = FDateTime::Now();
			Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"),
				Now.GetHour(), Now.GetMinute(), Now.GetSecond());
			OnLog(Entry);
		}
	}

	// /adm/obj/{n}/gain  g   (linear amplitude)
	if (State.GainLinear != 1.f)
	{
		Client->SendFloat(FString::Printf(TEXT("/adm/obj/%d/gain"), ID), State.GainLinear);
	}

	// /adm/obj/{n}/w  w   (width, [0..1])
	if (State.Width01 > 0.f)
	{
		Client->SendFloat(FString::Printf(TEXT("/adm/obj/%d/w"), ID), State.Width01);
	}

	// /adm/obj/{n}/mute  m   (0 or 1)
	Client->SendFloat(FString::Printf(TEXT("/adm/obj/%d/mute"), ID),
		State.bMuted ? 1.f : 0.f);
}

void FADMOSCAdapter::SendListener(const FSpatialFrameSnapshot& Snapshot)
{
	if (!Client) { return; }

	const FVector& LP = Snapshot.ListenerNormalized;
	const FRotator& LR = Snapshot.ListenerYPR;

	Client->SendMultiArg(TEXT("/adm/lis/xyz"), { (float)LP.X, (float)LP.Y, (float)LP.Z });
	Client->SendMultiArg(TEXT("/adm/lis/ypr"),
		{ (float)LR.Yaw, (float)LR.Pitch, (float)LR.Roll });
}
