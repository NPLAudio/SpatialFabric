// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

/**
 * Implementation of FSpatialMath. Inputs are usually StageNormalized [-1,1] per axis.
 * We use double internally to reduce floating-point drift before casting to float
 * for OSC (which typically uses 32-bit floats).
 */

#include "SpatialMath.h"
#include "Math/UnrealMathUtility.h"

FVector FSpatialMath::CartesianToPolar(FVector Normalized)
{
	// Stage axes: +X front, +Y right, +Z up (see SpatialMath.h)
	const double X = (double)Normalized.X;  // front
	const double Y = (double)Normalized.Y;  // right
	const double Z = (double)Normalized.Z;  // up

	const double Distance  = FMath::Sqrt(X * X + Y * Y + Z * Z); // 3D radius (polar "dist" before ADM scaling)
	const double HorizDist = FMath::Sqrt(X * X + Y * Y);         // ground projection for elevation
	// ADM-OSC azimuth: 0° = front, positive = CCW (left) from above.
	// Our +Y = right, so negate Y for the CCW-positive convention.
	const double AzimRad     = FMath::Atan2(-Y, X);
	const double ElevRad     = FMath::Atan2(Z, HorizDist);

	return FVector(
		(float)FMath::RadiansToDegrees(AzimRad),
		(float)FMath::RadiansToDegrees(ElevRad),
		(float)Distance);
}

FVector FSpatialMath::NormalizedTo2DPanning(FVector Normalized)
{
	// 2D panning: X = left(-1)..right(+1), Y = back(-1)..front(+1)
	// Stage convention: +X = front, +Y = right → Pan X = NormY, Pan Y = NormX
	const float PanX = FMath::Clamp(Normalized.Y, -1.f, 1.f);
	const float PanY = FMath::Clamp(Normalized.X, -1.f, 1.f);
	const float SpreadHint = FMath::Abs(Normalized.Z); // elevation mapped to spread hint
	return FVector(PanX, PanY, SpreadHint);
}

float FSpatialMath::LinearToDb(float Linear)
{
	// log10(0) is undefined; -144 dB is a practical "silence" floor for audio UIs
	if (Linear <= 0.f) { return -144.f; }
	return 20.f * FMath::LogX(10.f, Linear);
}

float FSpatialMath::DbToLinear(float Db)
{
	// Inverse of LinearToDb: amplitude ratio from decibels
	return FMath::Pow(10.f, Db / 20.f);
}
