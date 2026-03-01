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
 *   +X = stage front (positive azimuth)
 *   +Y = stage left
 *   +Z = stage up
 *
 * This matches ADM-OSC's recommended convention.  DS100 and QLab mappings
 * are computed from this canonical form.
 */
struct SPATIALFABRIC_API FSpatialMath
{
	// ── Polar conversion ────────────────────────────────────────────────────

	/**
	 * Convert a normalized Cartesian position to spherical polar coordinates.
	 * Returns: X = azimuth (degrees, 0° = front, +CCW from above),
	 *          Y = elevation (degrees, 0° = horizontal, +up),
	 *          Z = distance (magnitude, unnormalized).
	 */
	static FVector CartesianToPolar(FVector Normalized);

	// ── DS100 ───────────────────────────────────────────────────────────────

	/**
	 * Map a stage-normalized position to DS100 coordinate-mapping [0..1] range.
	 * DS100 coordinate mapping: 0 = left/back/bottom, 1 = right/front/top.
	 * Input axes X,Y,Z map to DS100 x,y axes as: DS100_x=(NormX+1)*0.5,
	 * DS100_y=(NormY+1)*0.5.  Z is dropped for 2D mapping.
	 */
	static FVector2D NormalizedToDS100Mapped(FVector Normalized);

	// ── QLab 2D ────────────────────────────────────────────────────────────

	/**
	 * Project a stage-normalized 3D position to a QLab 2D object panning
	 * coordinate in the horizontal plane.
	 * Returns: X = QLab panning X (left=-1, right=+1 convention),
	 *          Y = QLab panning Y (back=-1, front=+1 convention).
	 * Elevation is preserved as a spread hint (returned in Z).
	 */
	static FVector NormalizedToQLab2D(FVector Normalized);

	// ── Gain utilities ──────────────────────────────────────────────────────

	/** Convert linear amplitude [0..inf] to dB.  Returns -144 dB for zero. */
	static float LinearToDb(float Linear);

	/** Convert dB to linear amplitude. */
	static float DbToLinear(float Db);
};
