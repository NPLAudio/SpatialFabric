// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

// Custom OSC: ResolveAddress substitutes placeholders; BuildArgs fills float list.

#include "Adapters/CustomAdapter.h"
#include "SpatialMath.h"
#include "SpatialOSCClientComponent.h"

void FCustomAdapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config = InConfig;
	CachedSendRateHz = InConfig.SendRateHz;
	LastSentByID.Reset();
	ParseArgTemplate();
}

void FCustomAdapter::SetClientComponent(USpatialOSCClientComponent* InClient)
{
	Client = InClient;
}

void FCustomAdapter::ParseArgTemplate()
{
	CachedArgNames.Empty();
	FString Template = Config.CustomArgTemplate.TrimStartAndEnd();
	if (Template.IsEmpty()) { return; }

	TArray<FString> Parts;
	Template.ParseIntoArray(Parts, TEXT(" ")); // "x y z" → three tokens
	for (const FString& P : Parts)
	{
		FString Trimmed = P.TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			CachedArgNames.Add(Trimmed.ToLower());
		}
	}
}


/** Return axis name for {axis} placeholder in Discrete mode (x,y,z or azimuth,elevation,distance). */
static FString GetAxisNameForAddress(const FString& ArgName)
{
	if (ArgName == TEXT("azimuth") || ArgName == TEXT("a")) return TEXT("azimuth");
	if (ArgName == TEXT("elevation") || ArgName == TEXT("e")) return TEXT("elevation");
	if (ArgName == TEXT("distance") || ArgName == TEXT("d")) return TEXT("distance");
	return ArgName;
}


/** Map value from input range to output range. */
static float MapRange(float Value, float InMin, float InMax, float OutMin, float OutMax)
{
	// Linear remap: Value at InMin→OutMin, InMax→OutMax, clamped to [OutMin,OutMax] via t∈[0,1]
	const float t = (InMax > InMin) ? FMath::Clamp((Value - InMin) / (InMax - InMin), 0.f, 1.f) : 0.f;
	return OutMin + t * (OutMax - OutMin);
}

FString FCustomAdapter::ResolveAddress(const FSpatialNormalizedState& State, const FString* AxisOverride) const
{
	FString Addr = Config.CustomAddressTemplate;

	// Apply range mapping
	const float X = MapRange(State.StageNormalized.X, -1.f, 1.f, Config.CustomPositionRange.X, Config.CustomPositionRange.Y);
	const float Y = MapRange(State.StageNormalized.Y, -1.f, 1.f, Config.CustomPositionRange.X, Config.CustomPositionRange.Y);
	const float Z = MapRange(State.StageNormalized.Z, -1.f, 1.f, Config.CustomPositionRange.X, Config.CustomPositionRange.Y);
	const float Gain = MapRange(State.GainLinear, 0.f, 1.f, Config.CustomGainRange.X, Config.CustomGainRange.Y);
	const float Width = MapRange(State.Width01, 0.f, 1.f, Config.CustomWidthRange.X, Config.CustomWidthRange.Y);

	// Built-in placeholders
	Addr = Addr.Replace(TEXT("{id}"), *FString::FromInt(State.ObjectID));
	Addr = Addr.Replace(TEXT("{x}"), *FString::SanitizeFloat(X));
	Addr = Addr.Replace(TEXT("{y}"), *FString::SanitizeFloat(Y));
	Addr = Addr.Replace(TEXT("{z}"), *FString::SanitizeFloat(Z));
	Addr = Addr.Replace(TEXT("{gain}"), *FString::SanitizeFloat(Gain));
	Addr = Addr.Replace(TEXT("{width}"), *FString::SanitizeFloat(Width));
	Addr = Addr.Replace(TEXT("{label}"), *State.Label);

	// CustomFields placeholders
	for (const auto& Pair : State.CustomFields)
	{
		Addr = Addr.Replace(*FString::Printf(TEXT("{%s}"), *Pair.Key), *Pair.Value);
	}

	// Discrete mode: {axis} placeholder for per-value address
	if (AxisOverride)
	{
		const FString AxisStr = *AxisOverride;
		Addr = Addr.Replace(TEXT("{axis}"), *AxisStr);
	}

	return Addr;
}

TArray<float> FCustomAdapter::BuildArgs(const FSpatialNormalizedState& State) const
{
	const float X = MapRange(State.StageNormalized.X, -1.f, 1.f, Config.CustomPositionRange.X, Config.CustomPositionRange.Y);
	const float Y = MapRange(State.StageNormalized.Y, -1.f, 1.f, Config.CustomPositionRange.X, Config.CustomPositionRange.Y);
	const float Z = MapRange(State.StageNormalized.Z, -1.f, 1.f, Config.CustomPositionRange.X, Config.CustomPositionRange.Y);
	const float Gain = MapRange(State.GainLinear, 0.f, 1.f, Config.CustomGainRange.X, Config.CustomGainRange.Y);
	const float Width = MapRange(State.Width01, 0.f, 1.f, Config.CustomWidthRange.X, Config.CustomWidthRange.Y);

	float Azimuth = 0.f, Elevation = 0.f, DistanceNorm = 0.f;
	if (Config.CustomCoordinateMode == ECustomCoordinateMode::AED)
	{
		const FVector Polar = FSpatialMath::CartesianToPolar(State.StageNormalized);
		Azimuth = MapRange(Polar.X, -180.f, 180.f, Config.CustomPositionRange.X, Config.CustomPositionRange.Y);
		Elevation = MapRange(Polar.Y, -90.f, 90.f, Config.CustomPositionRange.X, Config.CustomPositionRange.Y);
		DistanceNorm = MapRange(FMath::Clamp(Polar.Z / FMath::Sqrt(3.f), 0.f, 1.f), 0.f, 1.f, Config.CustomPositionRange.X, Config.CustomPositionRange.Y);
	}

	TArray<float> Args;
	for (const FString& Name : CachedArgNames)
	{
		if (Name == TEXT("x")) { Args.Add(X); }
		else if (Name == TEXT("y")) { Args.Add(Y); }
		else if (Name == TEXT("z")) { Args.Add(Z); }
		else if (Name == TEXT("gain")) { Args.Add(Gain); }
		else if (Name == TEXT("width")) { Args.Add(Width); }
		else if (Name == TEXT("id")) { Args.Add(static_cast<float>(State.ObjectID)); }
		else if (Name == TEXT("azimuth") || Name == TEXT("a")) { Args.Add(Azimuth); }
		else if (Name == TEXT("elevation") || Name == TEXT("e")) { Args.Add(Elevation); }
		else if (Name == TEXT("distance") || Name == TEXT("d")) { Args.Add(DistanceNorm); }
	}
	return Args;
}

void FCustomAdapter::SendObject(const FSpatialNormalizedState& State)
{
	if (!Client) { return; }

	const TArray<float> FloatArgs = BuildArgs(State);
	if (Config.CustomSendMode == ECustomSendMode::Discrete && FloatArgs.Num() > 0)
	{
		for (int32 i = 0; i < FloatArgs.Num(); ++i)
		{
			const FString AxisName = GetAxisNameForAddress(CachedArgNames[i]);
			const FString Addr = ResolveAddress(State, &AxisName);
			Client->SendMultiArg(Addr, { FloatArgs[i] });
			if (OnLog)
			{
				FSpatialFabricLogEntry Entry;
				Entry.Adapter = TEXT("Custom");
				Entry.Direction = TEXT("OUT");
				Entry.Address = Addr;
				Entry.ValueStr = FString::Printf(TEXT("%.3f"), FloatArgs[i]);
				FDateTime Now = FDateTime::Now();
				Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"), Now.GetHour(), Now.GetMinute(), Now.GetSecond());
				OnLog(Entry);
			}
		}
	}
	else
	{
		const FString Address = ResolveAddress(State);
		if (FloatArgs.Num() > 0)
		{
			Client->SendMultiArg(Address, FloatArgs);
		}
		else
		{
			Client->SendMessage(Address, {}, {}, {});
		}
			if (OnLog)
			{
		FSpatialFabricLogEntry Entry;
		Entry.Adapter   = TEXT("Custom");
		Entry.Direction = TEXT("OUT");
		Entry.Address   = Address;
		FString ValStr;
		for (float V : FloatArgs) { ValStr += FString::Printf(TEXT("%.3f "), V); }
		Entry.ValueStr  = ValStr.TrimEnd();
		FDateTime Now = FDateTime::Now();
		Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"),
			Now.GetHour(), Now.GetMinute(), Now.GetSecond());
		OnLog(Entry);
			}
	}
}

void FCustomAdapter::ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime)
{
	if (!Config.bEnabled || !Client) { return; }
	if (!ShouldSendThisFrame(DeltaTime)) { return; }

	Client->Connect(Config.TargetIP, Config.TargetPort); // same shared client as ADM, new destination

	for (const FSpatialNormalizedState& State : Snapshot.Objects)
	{
		if (Config.bSendOnlyOnChange)
		{
			FCustomCachedState& Cache = LastSentByID.FindOrAdd(State.ObjectID);
			const bool bPosChanged = !State.StageNormalized.Equals(Cache.PosNorm, UE_KINDA_SMALL_NUMBER);
			const bool bGainChanged = FMath::Abs(State.GainLinear - Cache.GainLinear) > UE_KINDA_SMALL_NUMBER;
			const bool bWidthChanged = FMath::Abs(State.Width01 - Cache.Width01) > UE_KINDA_SMALL_NUMBER;
			if (!bPosChanged && !bGainChanged && !bWidthChanged && Cache.bEverSent)
			{
				continue;
			}
			Cache.PosNorm = State.StageNormalized;
			Cache.GainLinear = State.GainLinear;
			Cache.Width01 = State.Width01;
			Cache.bEverSent = true;
		}
		SendObject(State);
	}
}
