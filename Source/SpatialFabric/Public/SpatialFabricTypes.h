// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "SpatialFabricTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: spatial protocol adapter types
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Identifies which spatial protocol adapter a target entry routes to.
 * All four Phase-1 adapters (ADMOSC, DS100, RTTrPM, QLabObject) are fully
 * implemented.  Phase-2 adapters (QLabCue, SpaceMapGo, TiMax) are stubs.
 */
UENUM(BlueprintType)
enum class ESpatialAdapterType : uint8
{
	ADMOSC      UMETA(DisplayName = "ADM-OSC (L-ISA / General)"),
	DS100       UMETA(DisplayName = "d&b DS100 (Soundscape)"),
	RTTrPM      UMETA(DisplayName = "RTTrPM (Binary UDP)"),
	QLabObject  UMETA(DisplayName = "QLab Object Audio"),
	QLabCue     UMETA(DisplayName = "QLab Cue Control"),
	SpaceMapGo  UMETA(DisplayName = "Meyer SpaceMap Go"),
	TiMax       UMETA(DisplayName = "TiMax SoundHub / panLab"),
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

	/** Actor world position in UE units (centimetres by default). */
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
	 * Origin is the stage volume centre (or listener actor when set).
	 * +X = stage front, +Y = stage left, +Z = stage up (after axis remap).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FVector StageNormalized = FVector::ZeroVector;

	/**
	 * Same position expressed in physical metres (StageNormalized * HalfExtentMeters).
	 * Used directly by RTTrPM and for DS100 absolute-position mode.
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

	/**
	 * Protocol object/channel ID resolved from the binding's DefaultObjectID
	 * or an adapter-specific ObjectIDOverride.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	int32 ObjectID = 0;
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
	double Timestamp = 0.0;

	/**
	 * Listener position in stage-normalized space [-1,1].
	 * Set when a ListenerActor is assigned on the Stage Volume.
	 * Forwarded to adapters that emit listener data (ADM-OSC /adm/lis/*).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FVector ListenerNormalized = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	FRotator ListenerYPR = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialFabric")
	bool bHasListener = false;
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
	 * QLab-only: workspace ID string (e.g. "1" or a QLab workspace GUID).
	 * Unused by other adapters.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (EditCondition = "false"))
	FString QLabWorkspaceID;

	/**
	 * DS100-only: channel ID offset applied to all object IDs.
	 * Final channel = binding.DefaultObjectID + ChannelOffset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (EditCondition = "false"))
	int32 DS100ChannelOffset = 0;

	/**
	 * DS100-only: when true, send absolute-position messages
	 * (/dbaudio1/positioning/source_position/{id} x y z in metres)
	 * instead of normalized coordinate-mapping messages.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric|Adapter",
		meta = (EditCondition = "false"))
	bool bDS100AbsoluteMode = true;
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

	/**
	 * QLab cue identifier (cue number or name) for QLabObject / QLabCue adapters.
	 * Example: "5" for cue 5, or "myAudioCue" if QLab uses name-based addressing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric")
	FString QLabCueID;

	/**
	 * QLab audio object name within the cue (QLabObject adapter only).
	 * Maps to the {name} token in /cue/{cue}/object/{name}/position/live.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric")
	FString QLabObjectName;

	/** When false, this target entry is skipped for this binding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialFabric")
	bool bEnabled = true;
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
	 * E.g. DS100 channel number, ADM-OSC object index, QLab audio object index.
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
//  Struct: log entry (mirrors FLiveOSCLogEntry pattern)
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
