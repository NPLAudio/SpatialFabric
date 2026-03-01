// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "SpatialMath.h"
#include "Math/UnrealMathUtility.h"

FVector FSpatialMath::CartesianToPolar(FVector Normalized)
{
	const double X = (double)Normalized.X;
	const double Y = (double)Normalized.Y;
	const double Z = (double)Normalized.Z;

	const double Distance    = FMath::Sqrt(X * X + Y * Y + Z * Z);
	const double HorizDist   = FMath::Sqrt(X * X + Y * Y);
	const double AzimRad     = FMath::Atan2(Y, X);
	const double ElevRad     = FMath::Atan2(Z, HorizDist);

	return FVector(
		(float)FMath::RadiansToDegrees(AzimRad),
		(float)FMath::RadiansToDegrees(ElevRad),
		(float)Distance);
}

FVector2D FSpatialMath::NormalizedToDS100Mapped(FVector Normalized)
{
	// Normalized is in [-1,1]. DS100 coordinate mapping expects [0,1].
	// DS100 X-axis: left=-1 → 0, right=+1 → 1  (maps directly from NormY, left-positive)
	// DS100 Y-axis: back=-1 → 0, front=+1 → 1  (maps from NormX, front-positive)
	const float DS100X = FMath::Clamp((Normalized.Y + 1.f) * 0.5f, 0.f, 1.f); // left-right
	const float DS100Y = FMath::Clamp((Normalized.X + 1.f) * 0.5f, 0.f, 1.f); // front-back
	return FVector2D(DS100X, DS100Y);
}

FVector FSpatialMath::NormalizedToQLab2D(FVector Normalized)
{
	// QLab audio object panning: X = left(-1)..right(+1), Y = back(-1)..front(+1)
	// Stage convention: +X = front, +Y = left → QLab X = -NormY, QLab Y = NormX
	const float QLabX = FMath::Clamp(-Normalized.Y, -1.f, 1.f);
	const float QLabY = FMath::Clamp(Normalized.X, -1.f, 1.f);
	const float SpreadHint = FMath::Abs(Normalized.Z); // elevation mapped to spread hint
	return FVector(QLabX, QLabY, SpreadHint);
}

float FSpatialMath::LinearToDb(float Linear)
{
	if (Linear <= 0.f) { return -144.f; }
	return 20.f * FMath::LogX(10.f, Linear);
}

float FSpatialMath::DbToLinear(float Db)
{
	return FMath::Pow(10.f, Db / 20.f);
}
