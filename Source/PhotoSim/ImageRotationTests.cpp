// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ImageRotation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImageRotationCanvasSizeIdentityAtZeroTest, "PhotoSim.ImageRotation.CanvasSizeAtZeroDegreesIsUnchanged", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FImageRotationCanvasSizeIdentityAtZeroTest::RunTest(const FString& Parameters)
{
	const FIntPoint Result = PhotoSim::GetRotatedCanvasSize(FIntPoint(640, 360), 0.f);
	TestTrue(TEXT("No rotation leaves the canvas exactly as it was"), Result == FIntPoint(640, 360));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImageRotationCanvasSizeSwapsAt90Test, "PhotoSim.ImageRotation.CanvasSizeAt90DegreesSwapsWidthAndHeight", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FImageRotationCanvasSizeSwapsAt90Test::RunTest(const FString& Parameters)
{
	const FIntPoint Result = PhotoSim::GetRotatedCanvasSize(FIntPoint(640, 360), 90.f);
	TestTrue(TEXT("A quarter turn swaps width and height with no padding"), Result == FIntPoint(360, 640));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImageRotationCanvasSizeAt45Test, "PhotoSim.ImageRotation.CanvasSizeAt45DegreesGrowsToADiagonalSquare", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FImageRotationCanvasSizeAt45Test::RunTest(const FString& Parameters)
{
	// At exactly 45 degrees, Width = Height = (SourceWidth + SourceHeight) * cos(45) for ANY
	// source rectangle, since sin(45) == cos(45) - a 640x360 source grows to a 707x707 square.
	const FIntPoint Result = PhotoSim::GetRotatedCanvasSize(FIntPoint(640, 360), 45.f);
	TestTrue(TEXT("A 45 degree roll grows the canvas into a square, not a cropped or stretched rectangle"), Result == FIntPoint(707, 707));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImageRotationIdentityAtZeroDegreesTest, "PhotoSim.ImageRotation.RotateByZeroDegreesIsPixelIdentity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FImageRotationIdentityAtZeroDegreesTest::RunTest(const FString& Parameters)
{
	const TArray<FColor> Src = { FColor::Red, FColor::Green, FColor::Blue, FColor::White };
	const FIntPoint SrcSize(2, 2);

	TArray<FColor> Out;
	FIntPoint OutSize;
	PhotoSim::RotateImageExpandCanvas(Src, SrcSize, 0.f, FColor::Black, Out, OutSize);

	TestTrue(TEXT("No rotation leaves the canvas size unchanged"), OutSize == SrcSize);
	TestTrue(TEXT("No rotation leaves every pixel exactly where it was"), Out == Src);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImageRotationFillsCornersAt45Test, "PhotoSim.ImageRotation.RotateBy45DegreesFillsExpandedCornersWithFillColor", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FImageRotationFillsCornersAt45Test::RunTest(const FString& Parameters)
{
	// A solid white square rotated 45 degrees becomes a diamond inside a larger square canvas -
	// the canvas's own corners sit outside that diamond and must show FillColor, not the source.
	constexpr int32 SourceSide = 20;
	TArray<FColor> Src;
	Src.Init(FColor::White, SourceSide * SourceSide);

	TArray<FColor> Out;
	FIntPoint OutSize;
	PhotoSim::RotateImageExpandCanvas(Src, FIntPoint(SourceSide, SourceSide), 45.f, FColor::Black, Out, OutSize);

	TestEqual(TEXT("Top-left corner of the expanded canvas is outside the rotated diamond"), Out[0], FColor::Black);
	TestEqual(TEXT("Centre of the canvas is still inside the rotated diamond"), Out[(OutSize.Y / 2) * OutSize.X + (OutSize.X / 2)], FColor::White);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
