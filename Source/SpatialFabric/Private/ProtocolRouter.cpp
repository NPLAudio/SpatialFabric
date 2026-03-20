// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

/**
 * ProtocolRouter implementation.
 *
 * FilterSnapshotForAdapter must walk Bindings in the same order as
 * USpatialObjectRegistry::BuildSnapshot (skip disabled bindings, one row in
 * Full.Objects per enabled binding). That way ObjectIdx lines up with binding index.
 */

#include "ProtocolRouter.h"

void FProtocolRouter::RegisterAdapter(TSharedPtr<ISpatialProtocolAdapter> Adapter)
{
	if (Adapter.IsValid())
	{
		Adapters.Add(Adapter); // Order = ProcessFrame iteration order (typically ADM then Custom)
	}
}

void FProtocolRouter::ClearAdapters()
{
	Adapters.Empty();
}

void FProtocolRouter::ProcessFrame(
	const FSpatialFrameSnapshot& Snapshot,
	const TArray<FSpatialObjectBinding>& Bindings,
	float DeltaTime)
{
	// Each adapter gets a *subset* snapshot: only objects whose binding lists
	// an enabled FSpatialAdapterTargetEntry for that adapter type.
	for (const TSharedPtr<ISpatialProtocolAdapter>& Adapter : Adapters)
	{
		if (!Adapter.IsValid() || !Adapter->IsEnabled()) { continue; }

		const FSpatialFrameSnapshot Filtered =
			FilterSnapshotForAdapter(Snapshot, Bindings, Adapter->GetAdapterType());

		// Only call ProcessFrame if there are objects routed to this adapter
		// (or if it is a listener-only adapter like ADM-OSC with bHasListener).
		if (Filtered.Objects.Num() > 0 || Filtered.bHasListener)
		{
			Adapter->SetBindings(Bindings);
			Adapter->ProcessFrame(Filtered, DeltaTime);
		}
	}
}

void FProtocolRouter::DispatchIncomingOSC(const FString& Address, float Value)
{
	// Fan-out: every enabled adapter may inspect the same inbound message (e.g. ADM echo)
	for (const TSharedPtr<ISpatialProtocolAdapter>& Adapter : Adapters)
	{
		if (Adapter.IsValid() && Adapter->IsEnabled())
		{
			Adapter->HandleIncomingOSC(Address, Value);
		}
	}
}

ISpatialProtocolAdapter* FProtocolRouter::FindAdapter(ESpatialAdapterType Type) const
{
	for (const TSharedPtr<ISpatialProtocolAdapter>& Adapter : Adapters)
	{
		if (Adapter.IsValid() && Adapter->GetAdapterType() == Type)
		{
			return Adapter.Get();
		}
	}
	return nullptr;
}

FSpatialFrameSnapshot FProtocolRouter::FilterSnapshotForAdapter(
	const FSpatialFrameSnapshot& Full,
	const TArray<FSpatialObjectBinding>& Bindings,
	ESpatialAdapterType AdapterType) const
{
	FSpatialFrameSnapshot Filtered;
	Filtered.Timestamp        = Full.Timestamp;
	Filtered.ListenerNormalized = Full.ListenerNormalized;
	Filtered.ListenerYPR      = Full.ListenerYPR;
	Filtered.bHasListener     = Full.bHasListener;

	// Full.Objects are in the same order as Bindings (skipping disabled)
	// We need to re-walk bindings to match objects to target entries.
	int32 ObjectIdx = 0;
	for (int32 BindingIdx = 0; BindingIdx < Bindings.Num(); ++BindingIdx)
	{
		const FSpatialObjectBinding& Binding = Bindings[BindingIdx];
		if (!Binding.bEnabled) { continue; }
		if (ObjectIdx >= Full.Objects.Num()) { break; }

		const FSpatialNormalizedState& ObjState = Full.Objects[ObjectIdx];
		++ObjectIdx; // advance in lockstep: one snapshot row per enabled binding

		// Find a target entry for this adapter type
		for (const FSpatialAdapterTargetEntry& Target : Binding.Targets)
		{
			if (!Target.bEnabled) { continue; }
			if (Target.AdapterType != AdapterType) { continue; }

			// Clone the state and apply any per-target overrides
			FSpatialNormalizedState TargetState = ObjState;

			if (Target.ObjectIDOverride >= 0)
			{
				TargetState.ObjectID = Target.ObjectIDOverride;
			}
			if (AdapterType == ESpatialAdapterType::Custom)
			{
				TargetState.CustomFields = Target.CustomFields;
			}

			Filtered.Objects.Add(TargetState);
			break; // one entry per adapter per binding
		}
	}

	return Filtered;
}
