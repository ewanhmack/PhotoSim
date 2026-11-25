// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "LensPickup.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensPickupScaleAtMinFocalLengthTest, "PhotoSim.LensPickup.ScaleAtMinFocalLengthIsMinScale", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensPickupScaleAtMinFocalLengthTest::RunTest(const FString& Parameters)
{
	const float Scale = ALensPickup::ComputeLengthScaleForFocalLength(ALensPickup::MinCatalogFocalLengthMM);
	TestEqual(TEXT("14mm (the catalog's shortest lens) maps to MinLengthScale"), Scale, ALensPickup::MinLengthScale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensPickupScaleAtMaxFocalLengthTest, "PhotoSim.LensPickup.ScaleAtMaxFocalLengthIsMaxScale", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensPickupScaleAtMaxFocalLengthTest::RunTest(const FString& Parameters)
{
	const float Scale = ALensPickup::ComputeLengthScaleForFocalLength(ALensPickup::MaxCatalogFocalLengthMM);
	TestEqual(TEXT("400mm (the catalog's longest lens) maps to MaxLengthScale"), Scale, ALensPickup::MaxLengthScale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensPickupScaleAtMidpointTest, "PhotoSim.LensPickup.ScaleInterpolatesLinearly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensPickupScaleAtMidpointTest::RunTest(const FString& Parameters)
{
	const float MidpointFocalLength = (ALensPickup::MinCatalogFocalLengthMM + ALensPickup::MaxCatalogFocalLengthMM) / 2.f;
	const float Scale = ALensPickup::ComputeLengthScaleForFocalLength(MidpointFocalLength);
	const float ExpectedMidpointScale = (ALensPickup::MinLengthScale + ALensPickup::MaxLengthScale) / 2.f;

	TestTrue(TEXT("Halfway between the catalog's focal length extremes gives halfway between the scale extremes"),
		FMath::IsNearlyEqual(Scale, ExpectedMidpointScale, 0.001f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensPickupScaleClampsBelowRangeTest, "PhotoSim.LensPickup.ScaleClampsBelowMinFocalLength", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensPickupScaleClampsBelowRangeTest::RunTest(const FString& Parameters)
{
	const float Scale = ALensPickup::ComputeLengthScaleForFocalLength(ALensPickup::MinCatalogFocalLengthMM - 10.f);
	TestEqual(TEXT("A focal length narrower than the catalog's shortest lens still clamps to MinLengthScale"), Scale, ALensPickup::MinLengthScale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensPickupScaleClampsAboveRangeTest, "PhotoSim.LensPickup.ScaleClampsAboveMaxFocalLength", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensPickupScaleClampsAboveRangeTest::RunTest(const FString& Parameters)
{
	const float Scale = ALensPickup::ComputeLengthScaleForFocalLength(ALensPickup::MaxCatalogFocalLengthMM + 200.f);
	TestEqual(TEXT("A focal length longer than the catalog's longest lens still clamps to MaxLengthScale"), Scale, ALensPickup::MaxLengthScale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensPickupScaleIsMonotonicTest, "PhotoSim.LensPickup.ScaleGrowsWithFocalLength", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensPickupScaleIsMonotonicTest::RunTest(const FString& Parameters)
{
	const float ScaleAt24mm = ALensPickup::ComputeLengthScaleForFocalLength(24.f);
	const float ScaleAt200mm = ALensPickup::ComputeLengthScaleForFocalLength(200.f);
	const float ScaleAt400mm = ALensPickup::ComputeLengthScaleForFocalLength(400.f);

	TestTrue(TEXT("A longer lens never renders smaller than a shorter one"), ScaleAt24mm < ScaleAt200mm && ScaleAt200mm < ScaleAt400mm);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
