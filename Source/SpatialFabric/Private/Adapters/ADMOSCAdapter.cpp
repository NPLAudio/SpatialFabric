// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "Adapters/ADMOSCAdapter.h"
#include "LiveOSCClientComponent.h"
#include "SpatialMath.h"

void FADMOSCAdapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config = InConfig;
	CoordMode = InConfig.ADMCoordinateMode;
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

	Client->Connect(Config.TargetIP, Config.TargetPort);

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

	// ── Position (coordinate mode) ──────────────────────────────────────
	if (CoordMode == EADMCoordinateMode::Cartesian || CoordMode == EADMCoordinateMode::Both)
	{
		SendCartesian(ID, P);
	}
	if (CoordMode == EADMCoordinateMode::Polar || CoordMode == EADMCoordinateMode::Both)
	{
		SendPolar(ID, P);
	}

	// ── /adm/obj/{n}/gain  (float, linear amplitude) ────────────────────
	Client->SendFloat(FString::Printf(TEXT("/adm/obj/%d/gain"), ID), State.GainLinear);

	// ── /adm/obj/{n}/w  (float, horizontal extent [0..1]) ───────────────
	Client->SendFloat(FString::Printf(TEXT("/adm/obj/%d/w"), ID), State.Width01);

	// ── /adm/obj/{n}/mute  (int32, 0 or 1 per spec type 'i') ───────────
	Client->SendInt(FString::Printf(TEXT("/adm/obj/%d/mute"), ID),
		State.bMuted ? 1 : 0);

	// ── /adm/obj/{n}/name  (string, object label) ───────────────────────
	if (!State.Label.IsEmpty())
	{
		Client->SendString(FString::Printf(TEXT("/adm/obj/%d/name"), ID), State.Label);
	}

	// ── /adm/obj/{n}/dref  (float, distance reference [0..1]) ───────────
	if (State.Dref != 1.0f)
	{
		Client->SendFloat(FString::Printf(TEXT("/adm/obj/%d/dref"), ID), State.Dref);
	}

	// ── /adm/obj/{n}/dmax  (float, max distance in metres) ──────────────
	if (State.Dmax > 0.0f)
	{
		Client->SendFloat(FString::Printf(TEXT("/adm/obj/%d/dmax"), ID), State.Dmax);
	}
}

void FADMOSCAdapter::SendCartesian(int32 ID, const FVector& Norm)
{
	const FString Addr = FString::Printf(TEXT("/adm/obj/%d/xyz"), ID);
	Client->SendMultiArg(Addr, { (float)Norm.X, (float)Norm.Y, (float)Norm.Z });

	if (OnLog)
	{
		FSpatialFabricLogEntry Entry;
		Entry.Adapter   = TEXT("ADMOSC");
		Entry.Direction = TEXT("OUT");
		Entry.Address   = Addr;
		Entry.ValueStr  = FString::Printf(TEXT("%.3f %.3f %.3f"), Norm.X, Norm.Y, Norm.Z);
		FDateTime Now = FDateTime::Now();
		Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"),
			Now.GetHour(), Now.GetMinute(), Now.GetSecond());
		OnLog(Entry);
	}
}

void FADMOSCAdapter::SendPolar(int32 ID, const FVector& Norm)
{
	// CartesianToPolar returns (azimuth°, elevation°, distance)
	const FVector Polar = FSpatialMath::CartesianToPolar(Norm);

	const FString Addr = FString::Printf(TEXT("/adm/obj/%d/aed"), ID);
	Client->SendMultiArg(Addr, { (float)Polar.X, (float)Polar.Y, (float)Polar.Z });

	if (OnLog)
	{
		FSpatialFabricLogEntry Entry;
		Entry.Adapter   = TEXT("ADMOSC");
		Entry.Direction = TEXT("OUT");
		Entry.Address   = Addr;
		Entry.ValueStr  = FString::Printf(TEXT("%.1f %.1f %.3f"), Polar.X, Polar.Y, Polar.Z);
		FDateTime Now = FDateTime::Now();
		Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"),
			Now.GetHour(), Now.GetMinute(), Now.GetSecond());
		OnLog(Entry);
	}
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
