// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "Adapters/CustomAdapter.h"
#include "SpatialOSCClientComponent.h"

void FCustomAdapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config = InConfig;
	CachedSendRateHz = InConfig.SendRateHz;
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
	Template.ParseIntoArray(Parts, TEXT(" "));
	for (const FString& P : Parts)
	{
		FString Trimmed = P.TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			CachedArgNames.Add(Trimmed.ToLower());
		}
	}
}

FString FCustomAdapter::ResolveAddress(const FSpatialNormalizedState& State) const
{
	FString Addr = Config.CustomAddressTemplate;

	// Built-in placeholders
	Addr = Addr.Replace(TEXT("{id}"), *FString::FromInt(State.ObjectID));
	Addr = Addr.Replace(TEXT("{x}"), *FString::SanitizeFloat(State.StageNormalized.X));
	Addr = Addr.Replace(TEXT("{y}"), *FString::SanitizeFloat(State.StageNormalized.Y));
	Addr = Addr.Replace(TEXT("{z}"), *FString::SanitizeFloat(State.StageNormalized.Z));
	Addr = Addr.Replace(TEXT("{gain}"), *FString::SanitizeFloat(State.GainLinear));
	Addr = Addr.Replace(TEXT("{width}"), *FString::SanitizeFloat(State.Width01));
	Addr = Addr.Replace(TEXT("{label}"), *State.Label);

	// CustomFields placeholders
	for (const auto& Pair : State.CustomFields)
	{
		Addr = Addr.Replace(*FString::Printf(TEXT("{%s}"), *Pair.Key), *Pair.Value);
	}

	return Addr;
}

TArray<float> FCustomAdapter::BuildArgs(const FSpatialNormalizedState& State) const
{
	TArray<float> Args;
	for (const FString& Name : CachedArgNames)
	{
		if (Name == TEXT("x")) { Args.Add(State.StageNormalized.X); }
		else if (Name == TEXT("y")) { Args.Add(State.StageNormalized.Y); }
		else if (Name == TEXT("z")) { Args.Add(State.StageNormalized.Z); }
		else if (Name == TEXT("gain")) { Args.Add(State.GainLinear); }
		else if (Name == TEXT("width")) { Args.Add(State.Width01); }
		else if (Name == TEXT("id")) { Args.Add(static_cast<float>(State.ObjectID)); }
	}
	return Args;
}

void FCustomAdapter::SendObject(const FSpatialNormalizedState& State)
{
	if (!Client) { return; }

	const FString Address = ResolveAddress(State);
	const TArray<float> FloatArgs = BuildArgs(State);

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

void FCustomAdapter::ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime)
{
	if (!Config.bEnabled || !Client) { return; }
	if (!ShouldSendThisFrame(DeltaTime)) { return; }

	Client->Connect(Config.TargetIP, Config.TargetPort);

	for (const FSpatialNormalizedState& State : Snapshot.Objects)
	{
		SendObject(State);
	}
}
