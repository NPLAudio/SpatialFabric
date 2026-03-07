// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"

/**
 * FSpatialMath
 *
 * Pure static utility functions for spatial coordinate transforms used
 * by SpatialFabric adapters.  All intermediate calculations use double
 * precision; results are returned as float for OSC wire format compatibility.
 *
 * Coordinate conventions (after stage-volume normalization):
 *   +X = front (audience-facing forward)
 *   +Y = right (audience / house right)
 *   +Z = up
 *
 * This follows UE's native axis orientation and matches the audience /
 * listener perspective (house left/right).  DS100 mappings are
 * computed from this canonical form.
 */
struct SPATIALFABRIC_API FSpatialMath
{
	// ── Polar conversion ────────────────────────────────────────────────────

	/**
	 * Convert a normalized Cartesian position to ADM-OSC spherical polar.
	 * Returns: X = azimuth (degrees, 0° = front, +left / CCW from above),
	 *          Y = elevation (degrees, 0° = horizontal, +up),
	 *          Z = distance (magnitude, unnormalized).
	 * Note: our +Y = right, but ADM azimuth is left-positive, so Y is
	 * negated internally before computing atan2.
	 */
	static FVector CartesianToPolar(FVector Normalized);

	// ── DS100 ───────────────────────────────────────────────────────────────

	/**
	 * Map a stage-normalized position to DS100 coordinate-mapping [0..1] range.
	 * DS100 coordinate mapping: X: 0=left, 1=right.  Y: 0=back, 1=front.
	 * Stage +Y (right) maps to DS100 X=1; stage +X (front) maps to DS100 Y=1.
	 * Z is dropped for 2D mapping.
	 */
	static FVector2D NormalizedToDS100Mapped(FVector Normalized);

	// ── 2D panning (protocol-agnostic) ─────────────────────────────────────

	/**
	 * Project a stage-normalized 3D position to 2D horizontal panning coordinates.
	 * Returns: X = pan X (left=-1, right=+1 — maps from NormY),
	 *          Y = pan Y (back=-1, front=+1 — maps from NormX).
	 * Elevation is preserved as a spread hint (returned in Z).
	 */
	static FVector NormalizedTo2DPanning(FVector Normalized);

	// ── Gain utilities ──────────────────────────────────────────────────────

	/** Convert linear amplitude [0..inf] to dB.  Returns -144 dB for zero. */
	static float LinearToDb(float Linear);

	/** Convert dB to linear amplitude. */
	static float DbToLinear(float Db);
};
