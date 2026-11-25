// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Rotates a captured photo by the camera's exact continuous roll angle (not snapped to
 * landscape/portrait) - the canvas expands to the rotated rectangle's bounding box, like
 * rotating a layer in an image editor, so nothing is cropped and an in-between angle shows
 * black corners rather than a stretched or clipped image.
 */
namespace PhotoSim
{
	/** The width/height a SourceSize rectangle's bounding box becomes once rotated by AngleDegrees about its centre */
	FIntPoint GetRotatedCanvasSize(FIntPoint SourceSize, float AngleDegrees);

	/**
	 * Nearest-neighbour rotation of SrcPixels (SrcSize.X x SrcSize.Y, row-major) by AngleDegrees
	 * about its centre, onto a canvas sized by GetRotatedCanvasSize. Destination pixels that land
	 * outside the source image (the expanded canvas's corners, for any non-cardinal angle) are
	 * filled with FillColor.
	 */
	void RotateImageExpandCanvas(const TArray<FColor>& SrcPixels, FIntPoint SrcSize, float AngleDegrees, const FColor& FillColor, TArray<FColor>& OutPixels, FIntPoint& OutSize);
}
