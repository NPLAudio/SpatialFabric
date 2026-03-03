// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "Adapters/QLabObjectAdapter.h"
#include "SpatialMath.h"
#include "SpatialOSCClientComponent.h"

void FQLabObjectAdapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config = InConfig;
	CachedSendRateHz = InConfig.SendRateHz;
}

void FQLabObjectAdapter::SetClientComponent(USpatialOSCClientComponent* InClient)
{
	Client = InClient;
}

void FQLabObjectAdapter::ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime)
{
	if (!Config.bEnabled || !Client) { return; }
	if (!ShouldSendThisFrame(DeltaTime)) { return; }

	Client->Connect(Config.TargetIP, Config.TargetPort);

	for (const FSpatialNormalizedState& State : Snapshot.Objects)
	{
		SendObject(State);
	}
}

void FQLabObjectAdapter::SendObject(const FSpatialNormalizedState& State)
{
	if (!Client) { return; }

	FString CueID, ObjectName;
	if (!ParsePackedLabel(State.Label, CueID, ObjectName))
	{
		// Fallback: use objectID as cue, label as object name
		CueID      = FString::FromInt(State.ObjectID);
		ObjectName = State.Label;
	}

	// Compute QLab 2D coordinates from normalized 3D position
	// FSpatialMath::NormalizedToQLab2D returns (QLabX, QLabY, SpreadHint)
	const FVector QL = FSpatialMath::NormalizedToQLab2D(State.StageNormalized);
	const float QLabX = QL.X; // left(-1)..right(+1)
	const float QLabY = QL.Y; // back(-1)..front(+1)

	// Spread: prefer binding Width01, fall back to elevation-derived hint
	const float Spread = (State.Width01 > 0.f) ? State.Width01 : QL.Z;

	// /cue/{cueID}/object/{objectName}/position/live  x  y
	{
		const FString Addr = FString::Printf(
			TEXT("/cue/%s/object/%s/position/live"), *CueID, *ObjectName);
		Client->SendMultiArg(Addr, { QLabX, QLabY });

		if (OnLog)
		{
			FSpatialFabricLogEntry Entry;
			Entry.Adapter   = TEXT("QLabObject");
			Entry.Direction = TEXT("OUT");
			Entry.Address   = Addr;
			Entry.ValueStr  = FString::Printf(TEXT("%.3f %.3f"), QLabX, QLabY);
			FDateTime Now = FDateTime::Now();
			Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"),
				Now.GetHour(), Now.GetMinute(), Now.GetSecond());
			OnLog(Entry);
		}
	}

	// /cue/{cueID}/object/{objectName}/spread/live  s
	{
		const FString SpreadAddr = FString::Printf(
			TEXT("/cue/%s/object/%s/spread/live"), *CueID, *ObjectName);
		Client->SendFloat(SpreadAddr, FMath::Clamp(Spread, 0.f, 1.f));
	}
}

bool FQLabObjectAdapter::ParsePackedLabel(
	const FString& PackedLabel,
	FString& OutCueID,
	FString& OutObjectName)
{
	// Format: "<label>|<cueID>|<objectName>"
	TArray<FString> Parts;
	PackedLabel.ParseIntoArray(Parts, TEXT("|"), false);
	if (Parts.Num() >= 3)
	{
		OutCueID      = Parts[1];
		OutObjectName = Parts[2];
		return (!OutCueID.IsEmpty() && !OutObjectName.IsEmpty());
	}
	return false;
}
