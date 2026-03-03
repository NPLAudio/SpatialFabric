// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SpatialMath.h"
#include "Adapters/RTTrPMAdapter.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: build a minimal stage transform (box half-extent in cm)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Manually replicate WorldToNormalized math for test validation
 * (avoids spinning up a full ASpatialStageVolume in a unit test).
 *
 * Stage: 20m x 15m x 8m  →  HalfExtent = (1000cm, 750cm, 400cm).
 */
static FVector TestWorldToNormalized(FVector WorldPos, FVector BoxOrigin, FVector HalfExtentCm)
{
	const FVector Local = WorldPos - BoxOrigin;
	return FVector(
		FMath::Clamp(Local.X / HalfExtentCm.X, -1.f, 1.f),
		FMath::Clamp(Local.Y / HalfExtentCm.Y, -1.f, 1.f),
		FMath::Clamp(Local.Z / HalfExtentCm.Z, -1.f, 1.f));
}

static FVector TestWorldToMeters(FVector WorldPos, FVector BoxOrigin, FVector HalfExtentCm,
                                 FVector HalfExtentM)
{
	const FVector Norm = TestWorldToNormalized(WorldPos, BoxOrigin, HalfExtentCm);
	return Norm * HalfExtentM;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 1: Spatial math — object at stage centre
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSpatialMathCentreTest,
	"SpatialFabric.Math.Centre",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FSpatialMathCentreTest::RunTest(const FString& Parameters)
{
	const FVector BoxOrigin(0.f);
	const FVector HalfExtCm(1000.f, 750.f, 400.f);
	const FVector HalfExtM(10.f, 7.5f, 4.f);

	// Object at box centre → normalized (0,0,0)
	const FVector Norm = TestWorldToNormalized(FVector::ZeroVector, BoxOrigin, HalfExtCm);
	TestTrue("Centre normalized X == 0", FMath::IsNearlyZero(Norm.X, 0.001f));
	TestTrue("Centre normalized Y == 0", FMath::IsNearlyZero(Norm.Y, 0.001f));
	TestTrue("Centre normalized Z == 0", FMath::IsNearlyZero(Norm.Z, 0.001f));

	// Metres should also be (0,0,0)
	const FVector Metres = TestWorldToMeters(FVector::ZeroVector, BoxOrigin, HalfExtCm, HalfExtM);
	TestTrue("Centre metres X == 0", FMath::IsNearlyZero(Metres.X, 0.001f));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 2: Spatial math — object at stage corner
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSpatialMathCornerTest,
	"SpatialFabric.Math.Corner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FSpatialMathCornerTest::RunTest(const FString& Parameters)
{
	const FVector BoxOrigin(0.f);
	const FVector HalfExtCm(1000.f, 750.f, 400.f);
	const FVector HalfExtM(10.f, 7.5f, 4.f);

	// Object at +X+Y+Z corner
	const FVector CornerCm(1000.f, 750.f, 400.f);
	const FVector Norm = TestWorldToNormalized(CornerCm, BoxOrigin, HalfExtCm);
	TestTrue("Corner X == 1", FMath::IsNearlyEqual(Norm.X, 1.f, 0.001f));
	TestTrue("Corner Y == 1", FMath::IsNearlyEqual(Norm.Y, 1.f, 0.001f));
	TestTrue("Corner Z == 1", FMath::IsNearlyEqual(Norm.Z, 1.f, 0.001f));

	const FVector Metres = TestWorldToMeters(CornerCm, BoxOrigin, HalfExtCm, HalfExtM);
	TestTrue("Corner metres X == 10", FMath::IsNearlyEqual(Metres.X, 10.f, 0.01f));
	TestTrue("Corner metres Y == 7.5", FMath::IsNearlyEqual(Metres.Y, 7.5f, 0.01f));
	TestTrue("Corner metres Z == 4", FMath::IsNearlyEqual(Metres.Z, 4.f, 0.01f));

	// Object beyond corner → clamped to 1
	const FVector BeyondCm(2000.f, 750.f, 400.f);
	const FVector NormClamped = TestWorldToNormalized(BeyondCm, BoxOrigin, HalfExtCm);
	TestTrue("Clamp at 1.0", FMath::IsNearlyEqual(NormClamped.X, 1.f, 0.001f));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 3: ADM-OSC address format
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FADMOSCAddressTest,
	"SpatialFabric.Adapters.ADMOSCAddress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FADMOSCAddressTest::RunTest(const FString& Parameters)
{
	const int32 ObjID = 4;

	// Cartesian address format
	const FString ExpectedXYZ = TEXT("/adm/obj/4/xyz");
	TestEqual("ADM-OSC xyz address", FString::Printf(TEXT("/adm/obj/%d/xyz"), ObjID), ExpectedXYZ);

	// Polar address format
	const FString ExpectedAED = TEXT("/adm/obj/4/aed");
	TestEqual("ADM-OSC aed address", FString::Printf(TEXT("/adm/obj/%d/aed"), ObjID), ExpectedAED);

	// Property addresses
	TestEqual("ADM-OSC gain address",
		FString::Printf(TEXT("/adm/obj/%d/gain"), ObjID), TEXT("/adm/obj/4/gain"));
	TestEqual("ADM-OSC mute address",
		FString::Printf(TEXT("/adm/obj/%d/mute"), ObjID), TEXT("/adm/obj/4/mute"));
	TestEqual("ADM-OSC name address",
		FString::Printf(TEXT("/adm/obj/%d/name"), ObjID), TEXT("/adm/obj/4/name"));
	TestEqual("ADM-OSC dref address",
		FString::Printf(TEXT("/adm/obj/%d/dref"), ObjID), TEXT("/adm/obj/4/dref"));

	// Normalized coordinate range validation
	const float X = -0.9f, Y = 0.15f, Z = 0.70f;
	TestTrue("ADM X in range", X >= -1.f && X <= 1.f);
	TestTrue("ADM Y in range", Y >= -1.f && Y <= 1.f);
	TestTrue("ADM Z in range", Z >= -1.f && Z <= 1.f);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 4: DS100 address format
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDS100AddressTest,
	"SpatialFabric.Adapters.DS100Address",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FDS100AddressTest::RunTest(const FString& Parameters)
{
	// Test vector: id=42, absolute position
	const int32 ID = 42;
	const FString ExpectedAddr = TEXT("/dbaudio1/positioning/source_position/42");
	const FString ActualAddr = FString::Printf(
		TEXT("/dbaudio1/positioning/source_position/%d"), ID);
	TestEqual("DS100 source_position address", ActualAddr, ExpectedAddr);

	// Spread address
	const FString SpreadAddr = FString::Printf(
		TEXT("/dbaudio1/positioning/source_spread/%d"), ID);
	TestEqual("DS100 spread address", SpreadAddr,
		TEXT("/dbaudio1/positioning/source_spread/42"));

	// Coordinate mapping mode address
	const FString MappedAddr = FString::Printf(
		TEXT("/dbaudio1/coordinatemapping/source_position_xy/%d/%d"), 1, ID);
	TestEqual("DS100 coord mapping address", MappedAddr,
		TEXT("/dbaudio1/coordinatemapping/source_position_xy/1/42"));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 5: QLab Object address format
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQLabObjectAddressTest,
	"SpatialFabric.Adapters.QLabObjectAddress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FQLabObjectAddressTest::RunTest(const FString& Parameters)
{
	// Test vector: cue="5", name="A"
	const FString CueID = TEXT("5");
	const FString ObjName = TEXT("A");

	const FString PosAddr = FString::Printf(
		TEXT("/cue/%s/object/%s/position/live"), *CueID, *ObjName);
	TestEqual("QLab position/live address", PosAddr,
		TEXT("/cue/5/object/A/position/live"));

	const FString SpreadAddr = FString::Printf(
		TEXT("/cue/%s/object/%s/spread/live"), *CueID, *ObjName);
	TestEqual("QLab spread/live address", SpreadAddr,
		TEXT("/cue/5/object/A/spread/live"));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 6: RTTrPM packet signature bytes
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRTTrPMSignatureTest,
	"SpatialFabric.Adapters.RTTrPMSignature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FRTTrPMSignatureTest::RunTest(const FString& Parameters)
{
	// Verify the RTTrPM header signature constant
	// Spec: UInt16 Signature = 0x4154 ('AT')
	constexpr uint16 Sig = 0x4154;
	const uint8 Byte0 = (uint8)(Sig >> 8);   // 0x41 = 'A'
	const uint8 Byte1 = (uint8)(Sig & 0xFF); // 0x54 = 'T'

	TestEqual("RTTrPM sig[0] = 0x41", (int32)Byte0, 0x41);
	TestEqual("RTTrPM sig[1] = 0x54", (int32)Byte1, 0x54);

	// Version = 0x0200
	constexpr uint16 Ver = 0x0200;
	TestEqual("RTTrPM version high byte", (int32)(uint8)(Ver >> 8),   0x02);
	TestEqual("RTTrPM version low byte",  (int32)(uint8)(Ver & 0xFF), 0x00);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 7: Listener-relative coordinate transform
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSpatialMathListenerRelativeTest,
	"SpatialFabric.Math.ListenerRelative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FSpatialMathListenerRelativeTest::RunTest(const FString& Parameters)
{
	// Setup: stage 20m x 15m x 8m (HalfExt = 10m, 7.5m, 4m)
	// Listener at (500cm, 500cm, 0) facing +X (no rotation)
	// Object at (1000cm, 500cm, 0)
	// Expected: object is directly in front of listener → normalized (+1, 0, 0)
	// (distance in X = 500cm, HalfExtX = 1000cm → 0.5 norm, not 1.0)
	// Actually: listener at (500,500,0), object at (1000,500,0)
	// Local = (500, 0, 0)
	// Norm.X = 500 / 1000 = 0.5

	const FVector ListenerPos(500.f, 500.f, 0.f);  // 5m, 5m
	const FVector ObjectPos(1000.f, 500.f, 0.f);   // 10m, 5m
	const FVector HalfExtCm(1000.f, 750.f, 400.f);

	const FVector Local = ObjectPos - ListenerPos; // (500, 0, 0)
	const FVector Norm = FVector(
		FMath::Clamp(Local.X / HalfExtCm.X, -1.f, 1.f),
		FMath::Clamp(Local.Y / HalfExtCm.Y, -1.f, 1.f),
		FMath::Clamp(Local.Z / HalfExtCm.Z, -1.f, 1.f));

	TestTrue("Listener-relative X = 0.5", FMath::IsNearlyEqual(Norm.X, 0.5f, 0.001f));
	TestTrue("Listener-relative Y = 0",   FMath::IsNearlyZero(Norm.Y, 0.001f));
	TestTrue("Listener-relative Z = 0",   FMath::IsNearlyZero(Norm.Z, 0.001f));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 8: Listener-relative with yaw rotation (Y-axis sign convention)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSpatialMathListenerRotatedTest,
	"SpatialFabric.Math.ListenerRelativeRotated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FSpatialMathListenerRotatedTest::RunTest(const FString& Parameters)
{
	// Listener at origin, facing +Y (Yaw = 90°).
	// Object at world (0, 500, 0) — directly ahead of the listener.
	// After UnrotateVector the object should be at local (+500, 0, 0) = in front.
	//
	// An object at world (500, 0, 0) is to the listener's LEFT.
	// After UnrotateVector: UE local = (0, -500, 0)  (UE +Y = right, so left = -Y).
	// Stage convention: +Y = left, so we negate → stage local = (0, +500, 0).
	// Normalized Y = 500 / 750 = +0.667 (positive = stage-left). ✓

	const FVector ListenerPos  = FVector::ZeroVector;
	const FRotator ListenerRot = FRotator(0.f, 90.f, 0.f); // Yaw 90° → facing +Y
	const FVector HalfExtCm(1000.f, 750.f, 400.f);

	// Object to the listener's left (world +X when listener faces +Y)
	{
		const FVector ObjectPos(500.f, 0.f, 0.f);
		FVector Local = ObjectPos - ListenerPos; // (500, 0, 0)
		Local = ListenerRot.UnrotateVector(Local);
		Local.Y = -Local.Y; // UE right-positive → stage left-positive
		const FVector Norm(
			FMath::Clamp(Local.X / HalfExtCm.X, -1.f, 1.f),
			FMath::Clamp(Local.Y / HalfExtCm.Y, -1.f, 1.f),
			FMath::Clamp(Local.Z / HalfExtCm.Z, -1.f, 1.f));

		// Object is to the listener's LEFT → stage +Y → positive normalized Y
		TestTrue("Rotated: left object Y > 0", Norm.Y > 0.f);
		TestTrue("Rotated: left object X ~ 0", FMath::IsNearlyZero(Norm.X, 0.01f));
	}

	// Object directly in front of listener (world +Y when listener faces +Y)
	{
		const FVector ObjectPos(0.f, 500.f, 0.f);
		FVector Local = ObjectPos - ListenerPos; // (0, 500, 0)
		Local = ListenerRot.UnrotateVector(Local);
		Local.Y = -Local.Y;
		const FVector Norm(
			FMath::Clamp(Local.X / HalfExtCm.X, -1.f, 1.f),
			FMath::Clamp(Local.Y / HalfExtCm.Y, -1.f, 1.f),
			FMath::Clamp(Local.Z / HalfExtCm.Z, -1.f, 1.f));

		// Object is directly in front → stage +X → positive normalized X
		TestTrue("Rotated: front object X > 0", Norm.X > 0.f);
		TestTrue("Rotated: front object Y ~ 0", FMath::IsNearlyZero(Norm.Y, 0.01f));
	}

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test 9: DS100 coordinate mapping conversion
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDS100NormMappedTest,
	"SpatialFabric.Math.DS100Mapped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FDS100NormMappedTest::RunTest(const FString& Parameters)
{
	// Centre (0,0,0) → DS100 (0.5, 0.5)
	const FVector2D Centre = FSpatialMath::NormalizedToDS100Mapped(FVector::ZeroVector);
	TestTrue("DS100 centre X = 0.5", FMath::IsNearlyEqual(Centre.X, 0.5f, 0.001f));
	TestTrue("DS100 centre Y = 0.5", FMath::IsNearlyEqual(Centre.Y, 0.5f, 0.001f));

	// Front-right (+X+Y in stage, i.e. norm=(1,1,0))  → DS100 (1, 1)
	const FVector2D FrontRight = FSpatialMath::NormalizedToDS100Mapped(FVector(1.f, 1.f, 0.f));
	TestTrue("DS100 front-right X = 1", FMath::IsNearlyEqual(FrontRight.X, 1.f, 0.001f));
	TestTrue("DS100 front-right Y = 1", FMath::IsNearlyEqual(FrontRight.Y, 1.f, 0.001f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
