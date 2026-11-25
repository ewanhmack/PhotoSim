// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "TP_CameraComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExposureEVRoundTripAperturePriorityTest, "PhotoSim.ExposureMath.SolveShutterSpeedRoundTrips", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FExposureEVRoundTripAperturePriorityTest::RunTest(const FString& Parameters)
{
	const float TargetEV = UTP_CameraComponent::ComputeExposureEV(4.f, 125.f, 100.f);
	const float SolvedShutterSpeed = UTP_CameraComponent::SolveShutterSpeedForEV(4.f, 100.f, TargetEV);

	TestTrue(TEXT("Solving shutter speed for the EV that produced it returns the original value"),
		FMath::IsNearlyEqual(SolvedShutterSpeed, 125.f, 0.01f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExposureEVRoundTripShutterPriorityTest, "PhotoSim.ExposureMath.SolveApertureRoundTrips", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FExposureEVRoundTripShutterPriorityTest::RunTest(const FString& Parameters)
{
	const float TargetEV = UTP_CameraComponent::ComputeExposureEV(2.8f, 250.f, 400.f);
	const float SolvedAperture = UTP_CameraComponent::SolveApertureForEV(250.f, 400.f, TargetEV);

	TestTrue(TEXT("Solving aperture for the EV that produced it returns the original value"),
		FMath::IsNearlyEqual(SolvedAperture, 2.8f, 0.01f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExposureEVRoundTripISOTest, "PhotoSim.ExposureMath.SolveISORoundTrips", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FExposureEVRoundTripISOTest::RunTest(const FString& Parameters)
{
	const float TargetEV = UTP_CameraComponent::ComputeExposureEV(5.6f, 60.f, 800.f);
	const float SolvedISO = UTP_CameraComponent::SolveISOForEV(5.6f, 60.f, TargetEV);

	TestTrue(TEXT("Solving ISO for the EV that produced it returns the original value"),
		FMath::IsNearlyEqual(SolvedISO, 800.f, 0.1f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExposureEVBrighterApertureTest, "PhotoSim.ExposureMath.WiderApertureNeedsFasterShutterForSameEV", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FExposureEVBrighterApertureTest::RunTest(const FString& Parameters)
{
	// A wider aperture (lower f-number) lets in more light, so holding the same EV/ISO needs a
	// faster shutter (higher denominator) to compensate.
	const float TargetEV = UTP_CameraComponent::ComputeExposureEV(4.f, 125.f, 100.f);
	const float ShutterAtWideAperture = UTP_CameraComponent::SolveShutterSpeedForEV(2.f, 100.f, TargetEV);
	const float ShutterAtNarrowAperture = UTP_CameraComponent::SolveShutterSpeedForEV(8.f, 100.f, TargetEV);

	TestTrue(TEXT("A wider aperture (f/2) needs a faster shutter than a narrower one (f/8) for the same EV"),
		ShutterAtWideAperture > ShutterAtNarrowAperture);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExposureModeStepForwardTest, "PhotoSim.ExposureMath.ModeStepsForwardThroughAllFiveAndWraps", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FExposureModeStepForwardTest::RunTest(const FString& Parameters)
{
	ECameraExposureMode Mode = ECameraExposureMode::Manual;
	Mode = UTP_CameraComponent::GetNextExposureMode(Mode, 1);
	TestTrue(TEXT("Manual -> AperturePriority"), Mode == ECameraExposureMode::AperturePriority);
	Mode = UTP_CameraComponent::GetNextExposureMode(Mode, 1);
	TestTrue(TEXT("AperturePriority -> ShutterPriority"), Mode == ECameraExposureMode::ShutterPriority);
	Mode = UTP_CameraComponent::GetNextExposureMode(Mode, 1);
	TestTrue(TEXT("ShutterPriority -> ISOPriority"), Mode == ECameraExposureMode::ISOPriority);
	Mode = UTP_CameraComponent::GetNextExposureMode(Mode, 1);
	TestTrue(TEXT("ISOPriority -> Auto"), Mode == ECameraExposureMode::Auto);
	Mode = UTP_CameraComponent::GetNextExposureMode(Mode, 1);
	TestTrue(TEXT("Auto wraps back to Manual"), Mode == ECameraExposureMode::Manual);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExposureModeStepBackwardTest, "PhotoSim.ExposureMath.ModeStepsBackwardAndWraps", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FExposureModeStepBackwardTest::RunTest(const FString& Parameters)
{
	const ECameraExposureMode Mode = UTP_CameraComponent::GetNextExposureMode(ECameraExposureMode::Manual, -1);
	TestTrue(TEXT("Stepping backward from Manual wraps to Auto"), Mode == ECameraExposureMode::Auto);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollNormalizeWrapsNegativeTest, "PhotoSim.ExposureMath.RollNormalizeWrapsNegativeDegrees", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FRollNormalizeWrapsNegativeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("-5 degrees wraps to 355"), UTP_CameraComponent::NormalizeRollDegrees(-5.f), 355.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollNormalizeWrapsAboveFullTurnTest, "PhotoSim.ExposureMath.RollNormalizeWrapsAboveFullTurn", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FRollNormalizeWrapsAboveFullTurnTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("370 degrees wraps to 10"), UTP_CameraComponent::NormalizeRollDegrees(370.f), 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollNormalizeIsIdentityInRangeTest, "PhotoSim.ExposureMath.RollNormalizeLeavesInRangeValuesUnchanged", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FRollNormalizeIsIdentityInRangeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A value already in [0, 360) is left unchanged"), UTP_CameraComponent::NormalizeRollDegrees(127.5f), 127.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameScaleZeroRollTest, "PhotoSim.ExposureMath.FrameScaleIsOneAtZeroRoll", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFrameScaleZeroRollTest::RunTest(const FString& Parameters)
{
	const float Scale = UTP_CameraComponent::GetFrameScaleForRoll(0.f, FVector2D(1920.f, 1080.f));
	TestTrue(TEXT("An unrotated frame should need no shrinking"), FMath::IsNearlyEqual(Scale, 1.f, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameScaleNinetyRollTest, "PhotoSim.ExposureMath.FrameScaleAtNinetyDegreesMatchesAspectRatioSwap", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFrameScaleNinetyRollTest::RunTest(const FString& Parameters)
{
	// At 90 degrees the frame is effectively portrait-shaped on a landscape screen, so it can only
	// be as wide as the screen is tall - the scale that achieves that is exactly Height/Width.
	const float Scale = UTP_CameraComponent::GetFrameScaleForRoll(90.f, FVector2D(1920.f, 1080.f));
	TestTrue(TEXT("90 degree roll should scale by Height/Width"), FMath::IsNearlyEqual(Scale, 1080.f / 1920.f, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameScaleFortyFiveRollSquareTest, "PhotoSim.ExposureMath.FrameScaleAtFortyFiveDegreesOnASquareScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFrameScaleFortyFiveRollSquareTest::RunTest(const FString& Parameters)
{
	// A square rotated 45 degrees needs its bounding box shrunk by 1/sqrt(2) to fit back inside
	// itself - a clean, independently-checkable case since width and height bind equally.
	const float Scale = UTP_CameraComponent::GetFrameScaleForRoll(45.f, FVector2D(1000.f, 1000.f));
	TestTrue(TEXT("45 degree roll on a square screen should scale by 1/sqrt(2)"), FMath::IsNearlyEqual(Scale, 1.f / FMath::Sqrt(2.f), 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameScaleZeroViewportTest, "PhotoSim.ExposureMath.FrameScaleFallsBackToOneForADegenerateViewport", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFrameScaleZeroViewportTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A zero-sized viewport should return the safe fallback of 1, not divide by zero"), UTP_CameraComponent::GetFrameScaleForRoll(45.f, FVector2D::ZeroVector), 1.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
