// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "Adapters/TiMaxAdapter.h"

void FTiMaxAdapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config = InConfig;
	InnerADM.Configure(InConfig);

	// Rewrap the inner adapter's log callback so entries show "TiMax"
	// instead of "ADMOSC" in the Output panel.
	InnerADM.OnLog = [this](FSpatialFabricLogEntry Entry)
	{
		Entry.Adapter = TEXT("TiMax");
		if (OnLog) { OnLog(Entry); }
	};
}

void FTiMaxAdapter::SetClientComponent(USpatialOSCClientComponent* InClient)
{
	InnerADM.SetClientComponent(InClient);
}

void FTiMaxAdapter::ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime)
{
	InnerADM.ProcessFrame(Snapshot, DeltaTime);
}

void FTiMaxAdapter::HandleIncomingOSC(const FString& Address, float Value)
{
	InnerADM.HandleIncomingOSC(Address, Value);
}
