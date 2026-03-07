// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "Adapters/ADMOSCAdapter.h"
#include "SpatialOSCClientComponent.h"
#include "SpatialMath.h"

void FADMOSCAdapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config = InConfig;
	CoordMode = InConfig.ADMCoordinateMode;
	CachedSendRateHz = InConfig.SendRateHz;
	LastSentByID.Reset();
	LastListenerNorm = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	LastListenerYPR = FRotator(FLT_MAX, FLT_MAX, FLT_MAX);
}

void FADMOSCAdapter::SetClientComponent(USpatialOSCClientComponent* InClient)
{
	Client = InClient;
}

void FADMOSCAdapter::HandleIncomingOSC(const FString& Address, float /*Value*/)
{
	if (Address.StartsWith(TEXT("/adm/")))
	{
		bConnectionConfirmed = true;
		LastReplyTime = FPlatformTime::Seconds();
	}
}

void FADMOSCAdapter::ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime)
{
	if (!Config.bEnabled || !Client) { return; }

	// Update staleness counter for connection confirmation UI.
	if (LastReplyTime >= 0.0)
	{
		SecondsSinceLastReply = FPlatformTime::Seconds() - LastReplyTime;
	}

	if (!ShouldSendThisFrame(DeltaTime)) { return; }

	Client->Connect(Config.TargetIP, Config.TargetPort);

	for (const FSpatialNormalizedState& State : Snapshot.Objects)
	{
		if (Config.bSendOnlyOnChange)
		{
			FADMCachedState& Cache = LastSentByID.FindOrAdd(State.ObjectID);
			const bool bPosChanged = !State.StageNormalized.Equals(Cache.PosNorm, UE_KINDA_SMALL_NUMBER);
			const bool bGainChanged = FMath::Abs(State.GainLinear - Cache.GainLinear) > UE_KINDA_SMALL_NUMBER;
			const bool bWidthChanged = FMath::Abs(State.Width01 - Cache.Width01) > UE_KINDA_SMALL_NUMBER;
			const bool bMuteChanged = (State.bMuted != Cache.bMuted);
			const bool bLabelChanged = (State.bADMSendName && State.Label != Cache.Label);
			if (!bPosChanged && !bGainChanged && !bWidthChanged && !bMuteChanged && !bLabelChanged && Cache.bEverSent)
			{
				continue;
			}
			Cache.PosNorm = State.StageNormalized;
			Cache.GainLinear = State.GainLinear;
			Cache.Width01 = State.Width01;
			Cache.bMuted = State.bMuted;
			Cache.Label = State.Label;
			Cache.bEverSent = true;
		}
		SendObject(State);
	}

	if (Snapshot.bHasListener)
	{
		if (Config.bSendOnlyOnChange)
		{
			const bool bLisChanged = !Snapshot.ListenerNormalized.Equals(LastListenerNorm, UE_KINDA_SMALL_NUMBER)
				|| !Snapshot.ListenerYPR.Equals(LastListenerYPR, UE_KINDA_SMALL_NUMBER);
			if (!bLisChanged && bListenerEverSent) { /* skip */ }
			else
			{
				LastListenerNorm = Snapshot.ListenerNormalized;
				LastListenerYPR = Snapshot.ListenerYPR;
				bListenerEverSent = true;
				SendListener(Snapshot);
			}
		}
		else
		{
			SendListener(Snapshot);
		}
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

	// ── /adm/obj/{n}/w  (float, horizontal extent [0..1]) ───────────────
	Client->SendFloat(FString::Printf(TEXT("/adm/obj/%d/w"), ID), State.Width01);

	// ── Optional per-object metadata (enabled per-binding) ──────────────
	if (State.bADMSendGain)
	{
		Client->SendFloat(FString::Printf(TEXT("/adm/obj/%d/gain"), ID), State.GainLinear);
	}
	if (State.bADMSendMute)
	{
		Client->SendInt(FString::Printf(TEXT("/adm/obj/%d/mute"), ID), State.bMuted ? 1 : 0);
	}
	if (State.bADMSendName)
	{
		Client->SendString(FString::Printf(TEXT("/adm/obj/%d/name"), ID), State.Label);
	}
}

void FADMOSCAdapter::SendCartesian(int32 ID, const FVector& Norm)
{
	const FString Addr = FString::Printf(TEXT("/adm/obj/%d/xyz"), ID);
	// ADM-OSC Cartesian axis mapping (spec: X=right, Y=forward, Z=up):
	//   ADM X = our +Y (right)
	//   ADM Y = our +X (front)
	//   ADM Z = our +Z (up)
	const float ADMX = (float)Norm.Y;
	const float ADMY = (float)Norm.X;
	const float ADMZ = (float)Norm.Z;
	Client->SendMultiArg(Addr, { ADMX, ADMY, ADMZ });

	if (OnLog)
	{
		FSpatialFabricLogEntry Entry;
		Entry.Adapter   = TEXT("ADMOSC");
		Entry.Direction = TEXT("OUT");
		Entry.Address   = Addr;
		Entry.ValueStr  = FString::Printf(TEXT("%.3f %.3f %.3f"), ADMX, ADMY, ADMZ);
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

	// ADM-OSC spec: dist must be in [0, 1].  Raw Euclidean magnitude of a
	// normalized [-1,1]^3 position can reach √3 ≈ 1.732 at a stage corner.
	// Divide by √3 to map the full stage cube onto [0, 1].
	const float NormalizedDist = FMath::Clamp((float)Polar.Z / FMath::Sqrt(3.0f), 0.0f, 1.0f);
	// Spec: azimuth [-180, 180], elevation [-90, 90].  atan2 stays in range
	// analytically, but clamp defensively for NaN safety and spec compliance.
	const float ClampedAzim = FMath::Clamp((float)Polar.X, -180.0f, 180.0f);
	const float ClampedElev = FMath::Clamp((float)Polar.Y,  -90.0f,  90.0f);

	const FString Addr = FString::Printf(TEXT("/adm/obj/%d/aed"), ID);
	Client->SendMultiArg(Addr, { ClampedAzim, ClampedElev, NormalizedDist });

	if (OnLog)
	{
		FSpatialFabricLogEntry Entry;
		Entry.Adapter   = TEXT("ADMOSC");
		Entry.Direction = TEXT("OUT");
		Entry.Address   = Addr;
		Entry.ValueStr  = FString::Printf(TEXT("%.1f %.1f %.3f"), ClampedAzim, ClampedElev, NormalizedDist);
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

	// ADM-OSC Cartesian axis mapping (spec: X=right, Y=forward, Z=up).
	Client->SendMultiArg(TEXT("/adm/lis/xyz"), { (float)LP.Y, (float)LP.X, (float)LP.Z });
	Client->SendMultiArg(TEXT("/adm/lis/ypr"),
		{ (float)LR.Yaw, (float)LR.Pitch, (float)LR.Roll });
}
