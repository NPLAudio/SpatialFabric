// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "SpatialFabricTypes.h"

class USpatialOSCClientComponent;

/**
 * ISpatialProtocolAdapter
 *
 * In C++, this *interface* is a class with only *virtual* functions and no shared
 * implementation — concrete classes like FADMOSCAdapter must implement every "= 0"
 * method. That lets new protocols (L-ISA, d&b, custom OSC) be added without changing
 * the manager actor.
 *
 * Pure abstract interface for all spatial protocol adapters.
 * Each adapter translates an FSpatialFrameSnapshot into protocol-specific
 * network packets (OSC messages) and sends them via the provided
 * USpatialOSCClientComponent.
 *
 * Adapters are owned and driven by FProtocolRouter.  They must be
 * thread-compatible: Configure() and ProcessFrame() are always called on
 * the game thread.
 */
class SPATIALFABRIC_API ISpatialProtocolAdapter
{
public:
	virtual ~ISpatialProtocolAdapter() = default;

	/** Unique name used for logging and adapter lookup. */
	virtual FName GetName() const = 0;

	/** Adapter type enum value — used by the router for type-keyed lookups. */
	virtual ESpatialAdapterType GetAdapterType() const = 0;

	/**
	 * Apply endpoint/settings configuration.
	 * Called once on startup and whenever the user changes adapter settings.
	 * For OSC adapters, SetClientComponent must also be called before
	 * ProcessFrame is invoked.
	 */
	virtual void Configure(const FSpatialAdapterConfig& InConfig) = 0;

	/**
	 * Provide the shared OSC client component used to send outgoing messages.
	 */
	virtual void SetClientComponent(USpatialOSCClientComponent* InClient) {}

	/**
	 * Optional: supply the full binding array so adapters that need per-binding
	 * overrides (e.g. DS100 spread mode, delay mode) can look them up by ObjectID.
	 * Called by FProtocolRouter before each ProcessFrame.
	 */
	virtual void SetBindings(const TArray<FSpatialObjectBinding>& /*Bindings*/) {}

	/**
	 * Process one frame: read relevant objects from Snapshot and emit
	 * protocol packets.  Rate limiting (FSpatialAdapterConfig::SendRateHz)
	 * is enforced internally.  Called each game tick.
	 *
	 * @param Snapshot    Per-adapter filtered frame snapshot.
	 * @param DeltaTime   Elapsed seconds since the last game tick.  Used by
	 *                    ShouldSendThisFrame() for accurate rate limiting.
	 */
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime) = 0;

	/**
	 * Handle an incoming OSC message forwarded from the server component.
	 * Optional — only implement for adapters that consume inbound data
	 * (e.g. tracking-in or adapter-specific reply handling).
	 */
	virtual void HandleIncomingOSC(const FString& Address, float Value) {}

	/** Returns true if this adapter is enabled (FSpatialAdapterConfig::bEnabled). */
	virtual bool IsEnabled() const = 0;

	/** Append a log entry to the shared log.  Set by the router after construction. */
	TFunction<void(const FSpatialFabricLogEntry&)> OnLog;

protected:
	/**
	 * Rate-limiting helper: returns true only when SendRateHz allows another send.
	 * DeltaTime is seconds since last *game tick* (not since last send), so we
	 * accumulate time until MinInterval = 1/SendRateHz has passed.
	 */
	bool ShouldSendThisFrame(float DeltaTime)
	{
		if (CachedSendRateHz <= 0.f) { return true; } // unlimited rate
		TimeSinceLastSend += DeltaTime;
		const float MinInterval = 1.f / CachedSendRateHz;
		if (TimeSinceLastSend >= MinInterval)
		{
			// Carry overflow into next window so average rate stays near SendRateHz
			TimeSinceLastSend = FMath::Fmod(TimeSinceLastSend, MinInterval);
			return true;
		}
		return false;
	}

	float CachedSendRateHz = 50.f;
	float TimeSinceLastSend = 0.f;
};
