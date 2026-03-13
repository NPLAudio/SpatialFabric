// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "SpatialFabricTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: spatial protocol adapter types
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Identifies which spatial protocol adapter a target entry routes to.
 */
UENUM(BlueprintType)
enum class ESpatialAdapterType : uint8
{
	ADMOSC  UMETA(DisplayName = "ADM-OSC (L-ISA / General)"),
	Custom  UMETA(DisplayName = "Custom (Template)"),
};

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: per-frame raw state for one tracked object (pre-normalization)
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct SPATIALFABRIC_API FSpatialObjectState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FGuid SpatialGuid;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FString Label;

	/** Actor world position in UE units (centimeters by default). */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FVector WorldPosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FQuat Orientation = FQuat::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	float GainLinear = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	float Width01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	bool bMuted = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: normalized stage-space state (post-normalization, ready for adapters)
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct SPATIALFABRIC_API FSpatialNormalizedState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FGuid SpatialGuid;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FString Label;

	/**
	 * Stage-normalized position: each axis clamped to [-1, 1].
	 * Origin is the stage volume center (or listener actor when set).
	 * +X = stage front, +Y = stage right (house right), +Z = stage up (after axis remap).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FVector StageNormalized = FVector::ZeroVector;

	/**
	 * Same position expressed in physical meters (StageNormalized * HalfExtentMeters).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FVector StageMeters = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FQuat Orientation = FQuat::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	float GainLinear = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	float Width01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	bool bMuted = false;

	/** ADM-OSC distance reference: normalized distance where physics-based rendering begins. */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	float Dref = 1.0f;

	/** ADM-OSC max distance in meters corresponding to dref = 1.0. 0 = not set. */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	float Dmax = 0.0f;

	/** When true, the ADM-OSC adapter sends /adm/obj/{n}/gain for this object. */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	bool bADMSendGain = false;

	/** When true, the ADM-OSC adapter sends /adm/obj/{n}/mute for this object. */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	bool bADMSendMute = false;

	/** When true, the ADM-OSC adapter sends /adm/obj/{n}/name for this object. */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	bool bADMSendName = false;

	/**
	 * Protocol object/channel ID resolved from the binding's DefaultObjectID
	 * or an adapter-specific ObjectIDOverride.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	int32 ObjectID = 0;

	/**
	 * Custom adapter: per-binding key-value pairs for template placeholders.
	 * Populated by the router from FSpatialAdapterTargetEntry::CustomFields
	 * when routing to the Custom adapter.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	TMap<FString, FString> CustomFields;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: immutable per-frame snapshot of all tracked objects
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct SPATIALFABRIC_API FSpatialFrameSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	TArray<FSpatialNormalizedState> Objects;

	/** World time (seconds) when this snapshot was captured. */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	float Timestamp = 0.0f;

	/**
	 * Listener position in stage-normalized space [-1,1].
	 * Set when a ListenerActor is assigned on the Stage Volume.
	 * Forwarded to adapters that emit listener data under the ADM-OSC listener path.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FVector ListenerNormalized = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FRotator ListenerYPR = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	bool bHasListener = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: ADM-OSC coordinate mode
// ─────────────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EADMCoordinateMode : uint8
{
	Cartesian   UMETA(DisplayName = "Cartesian (xyz)"),
	Polar       UMETA(DisplayName = "Polar (aed)"),
	Both        UMETA(DisplayName = "Both (xyz + aed)"),
};

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: Custom adapter coordinate format
// ─────────────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ECustomCoordinateMode : uint8
{
	XYZ  UMETA(DisplayName = "Cartesian (x y z)"),
	AED  UMETA(DisplayName = "Polar (azimuth elevation distance)"),
};

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: Custom adapter send mode
// ─────────────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ECustomSendMode : uint8
{
	Bundled  UMETA(DisplayName = "Bundled (one message, multiple args)"),
	Discrete UMETA(DisplayName = "Discrete (one message per value)"),
};

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: adapter endpoint configuration
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Network endpoint and rate configuration for one protocol adapter.
 * Stored per-adapter type in ASpatialFabricManagerActor::AdapterConfigs.
 */
USTRUCT(BlueprintType)
struct SPATIALFABRIC_API FSpatialAdapterConfig
{
	GENERATED_BODY()

	/** Target IP address for this adapter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter")
	FString TargetIP = TEXT("127.0.0.1");

	/** Target UDP port for this adapter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (ClampMin = "1024", ClampMax = "65535"))
	int32 TargetPort = 9000;

	/** Maximum packets per second sent by this adapter (rate limiting). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float SendRateHz = 50.f;

	/** When false, this adapter is completely skipped each frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter")
	bool bEnabled = true;

	/**
	 * When true, only transmit when coordinates (and gain/width/mute) change.
	 * Sends full state on level start, then only when values differ from last send.
	 * Reduces network traffic when objects are static.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter")
	bool bSendOnlyOnChange = false;

	/**
	 * ADM-OSC: coordinate format to send each frame.
	 * Cartesian sends /xyz, Polar sends /aed, Both sends both.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (EditCondition = "false"))
	EADMCoordinateMode ADMCoordinateMode = EADMCoordinateMode::Polar;

	/**
	 * Custom adapter: OSC address template. Placeholders: {id}, {x}, {y}, {z},
	 * {gain}, {width}, {label}, plus any CustomFields keys as {key}.
	 * Example: /source/{id}/position
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (EditCondition = "false"))
	FString CustomAddressTemplate = TEXT("/source/{id}/position");

	/**
	 * Custom adapter: space-separated arg names to send as floats.
	 * Supported: x, y, z, gain, width, id (XYZ mode); azimuth, elevation, distance or a, e, d (AED mode)
	 * Example: "x y z" sends StageNormalized.X, Y, Z
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (EditCondition = "false"))
	FString CustomArgTemplate = TEXT("x y z");

	/** Custom adapter: Cartesian (xyz) or Polar (azimuth elevation distance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (EditCondition = "false"))
	ECustomCoordinateMode CustomCoordinateMode = ECustomCoordinateMode::XYZ;

	/** Custom adapter: Bundled = one message with multiple args; Discrete = one message per value (use {axis} in address). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (EditCondition = "false"))
	ECustomSendMode CustomSendMode = ECustomSendMode::Bundled;

	/**
	 * Custom adapter: output range for position (x, y, z).
	 * Normalized input [-1..1] is mapped to [CustomPositionRangeMin..CustomPositionRangeMax].
	 * E.g. 0, 100 sends 0–100 instead of -1–1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (EditCondition = "false"))
	FVector2D CustomPositionRange = FVector2D(-1.0, 1.0);

	/**
	 * Custom adapter: output range for gain.
	 * Input [0..1] is mapped to [CustomGainRangeMin..CustomGainRangeMax].
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (EditCondition = "false"))
	FVector2D CustomGainRange = FVector2D(0.0, 1.0);

	/**
	 * Custom adapter: output range for width.
	 * Input [0..1] is mapped to [CustomWidthRangeMin..CustomWidthRangeMax].
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (EditCondition = "false"))
	FVector2D CustomWidthRange = FVector2D(0.0, 1.0);
};

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: per-binding adapter target entry
// ─────────────────────────────────────────────────────────────────────────────

/**
 * One adapter target within an FSpatialObjectBinding.
 * An object can have zero or more targets, each routing to a different adapter.
 */
USTRUCT(BlueprintType)
struct SPATIALFABRIC_API FSpatialAdapterTargetEntry
{
	GENERATED_BODY()

	/** Which adapter receives this object's data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric")
	ESpatialAdapterType AdapterType = ESpatialAdapterType::ADMOSC;

	/**
	 * Override the binding's DefaultObjectID for this specific adapter target.
	 * Set to -1 to use the binding's DefaultObjectID.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric",
		meta = (ClampMin = "-1"))
	int32 ObjectIDOverride = -1;

	/** When false, this target entry is skipped for this binding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric")
	bool bEnabled = true;

	/**
	 * Custom adapter: key-value pairs for template placeholders.
	 * E.g. "slot" -> "5", "channel" -> "A". Template references as {slot}, {channel}.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Custom")
	TMap<FString, FString> CustomFields;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: one actor → N adapter targets binding
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Binds one UE actor to one or more protocol adapter targets.
 * The actor's world position is read each frame by USpatialObjectRegistry
 * and routed to every enabled FSpatialAdapterTargetEntry.
 */
USTRUCT(BlueprintType)
struct SPATIALFABRIC_API FSpatialObjectBinding
{
	GENERATED_BODY()

	/** Human-readable name shown in the editor panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric")
	FString Label;

	/** The actor whose world position this binding tracks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric")
	TSoftObjectPtr<AActor> TargetActor;

	/** Cached actor label for re-resolution after PIE restart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric")
	FString CachedActorLabel;

	/**
	 * Default protocol object/channel ID for this binding.
	 * Used by all adapter targets that do not specify an ObjectIDOverride.
	 * E.g. ADM-OSC object index.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric",
		meta = (ClampMin = "1"))
	int32 DefaultObjectID = 1;

	/** Linear gain multiplier applied before forwarding to adapters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric",
		meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float GainLinear = 1.f;

	/** Object width / spread [0..1] forwarded to adapters that support it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Width01 = 0.f;

	// ── ADM-OSC per-object send flags ───────────────────────────────────────

	/** ADM-OSC: send /adm/obj/{n}/gain each frame (linear amplitude multiplier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|ADMOSC")
	bool bADMSendGain = false;

	/** ADM-OSC: send /adm/obj/{n}/mute each frame (0 = unmuted, 1 = muted). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|ADMOSC")
	bool bADMSendMute = false;

	/** ADM-OSC: send /adm/obj/{n}/name each frame (string object label). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|ADMOSC")
	bool bADMSendName = false;

	/**
	 * ADM-OSC: custom name sent as /adm/obj/{n}/name.
	 * If empty, falls back to the bound actor's label.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|ADMOSC")
	FString ADMNameOverride;

	/**
	 * ADM-OSC: normalized distance where physics-based rendering replaces dimensionless rendering.
	 * Default 1.0 per spec.  Sent as /adm/obj/{n}/dref.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|ADMOSC",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ADMDref = 1.0f;

	/**
	 * ADM-OSC: distance in meters signified by dref = 1.0.
	 * 0 = suppress sending /dmax.  Sent as /adm/obj/{n}/dmax.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|ADMOSC",
		meta = (ClampMin = "0.0"))
	float ADMDmax = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric")
	bool bMuted = false;

	/** When false, this binding is completely ignored each frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric")
	bool bEnabled = true;

	/** One or more adapter targets this object routes to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric")
	TArray<FSpatialAdapterTargetEntry> Targets;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: log entry
// ─────────────────────────────────────────────────────────────────────────────

/** A single entry in the SpatialFabric message log. */
USTRUCT(BlueprintType)
struct SPATIALFABRIC_API FSpatialFabricLogEntry
{
	GENERATED_BODY()

	/** Wall-clock time formatted as HH:MM:SS. */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FString Timestamp;

	/** Adapter name (e.g. "DS100", "ADMOSC"). */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FString Adapter;

	/** "OUT" for transmitted, "IN" for received. */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FString Direction;

	/** OSC address or binary protocol descriptor. */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FString Address;

	/** Value summary as a human-readable string. */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FString ValueStr;
};
