// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "SpatialFabricTypes.h"
#include "ISpatialProtocolAdapter.h"

/**
 * FProtocolRouter
 *
 * Not a UObject — no UCLASS, no garbage collection. The manager holds a
 * TUniquePtr<FProtocolRouter>. Each registered adapter is a TSharedPtr; here the
 * router is the main owner.
 *
 * Plain C++ class (no UObject) that fans out each FSpatialFrameSnapshot
 * to all enabled protocol adapters.
 *
 * Owned by ASpatialFabricManagerActor as a TUniquePtr.  Adapters are
 * registered once during BeginPlay and driven each tick via ProcessFrame().
 *
 * Each per-binding FSpatialAdapterTargetEntry specifies which adapter type
 * receives that object; the router slices the snapshot per adapter so each
 * adapter only processes objects routed to it.
 */
class SPATIALFABRIC_API FProtocolRouter
{
public:
	FProtocolRouter() = default;
	~FProtocolRouter() = default;

	// Non-copyable
	FProtocolRouter(const FProtocolRouter&) = delete;
	FProtocolRouter& operator=(const FProtocolRouter&) = delete;

	/**
	 * Register an adapter.  The router takes shared ownership.
	 * Call during ASpatialFabricManagerActor::BeginPlay before the first tick.
	 */
	void RegisterAdapter(TSharedPtr<ISpatialProtocolAdapter> Adapter);

	/** Remove all registered adapters. */
	void ClearAdapters();

	/**
	 * Route Snapshot to all enabled adapters.
	 * Called every game tick by ASpatialFabricManagerActor.
	 *
	 * @param Snapshot     Full normalized frame snapshot.
	 * @param Bindings     Manager's binding array — used to map each object
	 *                     to its target adapter entries.
	 * @param DeltaTime    Elapsed seconds since last frame (for rate limiting).
	 */
	void ProcessFrame(const FSpatialFrameSnapshot& Snapshot,
	                  const TArray<FSpatialObjectBinding>& Bindings,
	                  float DeltaTime);

	/** Forward an incoming OSC message to all adapters. */
	void DispatchIncomingOSC(const FString& Address, float Value);

	/** Returns the registered adapter for the given type, or nullptr. */
	ISpatialProtocolAdapter* FindAdapter(ESpatialAdapterType Type) const;

private:
	TArray<TSharedPtr<ISpatialProtocolAdapter>> Adapters;

	/**
	 * Build a per-adapter subset snapshot: only the objects whose binding
	 * has at least one enabled target entry for AdapterType.
	 * ObjectID is resolved from the matching target entry (or DefaultObjectID).
	 */
	FSpatialFrameSnapshot FilterSnapshotForAdapter(
		const FSpatialFrameSnapshot& Full,
		const TArray<FSpatialObjectBinding>& Bindings,
		ESpatialAdapterType AdapterType) const;
};
