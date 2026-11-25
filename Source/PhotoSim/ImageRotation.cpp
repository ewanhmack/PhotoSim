// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageRotation.h"

FIntPoint PhotoSim::GetRotatedCanvasSize(FIntPoint SourceSize, float AngleDegrees)
{
	const float AngleRad = FMath::DegreesToRadians(AngleDegrees);
	const float AbsCos = FMath::Abs(FMath::Cos(AngleRad));
	const float AbsSin = FMath::Abs(FMath::Sin(AngleRad));

	const int32 Width = FMath::Max(1, FMath::RoundToInt(SourceSize.X * AbsCos + SourceSize.Y * AbsSin));
	const int32 Height = FMath::Max(1, FMath::RoundToInt(SourceSize.X * AbsSin + SourceSize.Y * AbsCos));
	return FIntPoint(Width, Height);
}

void PhotoSim::RotateImageExpandCanvas(const TArray<FColor>& SrcPixels, FIntPoint SrcSize, float AngleDegrees, const FColor& FillColor, TArray<FColor>& OutPixels, FIntPoint& OutSize)
{
	OutSize = GetRotatedCanvasSize(SrcSize, AngleDegrees);
	OutPixels.SetNumUninitialized(OutSize.X * OutSize.Y);

	const float AngleRad = FMath::DegreesToRadians(AngleDegrees);
	const float CosA = FMath::Cos(AngleRad);
	const float SinA = FMath::Sin(AngleRad);

	const float SrcCentreX = SrcSize.X * 0.5f;
	const float SrcCentreY = SrcSize.Y * 0.5f;
	const float DstCentreX = OutSize.X * 0.5f;
	const float DstCentreY = OutSize.Y * 0.5f;

	for (int32 DstY = 0; DstY < OutSize.Y; ++DstY)
	{
		for (int32 DstX = 0; DstX < OutSize.X; ++DstX)
		{
			// Inverse mapping: for every destination pixel, rotate its offset from centre
			// BACKWARD by AngleDegrees to find where it came from in the source - this fills every
			// destination pixel with no holes, rather than scattering source pixels forward and
			// leaving gaps.
			const float Dx = (DstX + 0.5f) - DstCentreX;
			const float Dy = (DstY + 0.5f) - DstCentreY;
			const float SrcX = CosA * Dx + SinA * Dy + SrcCentreX;
			const float SrcY = -SinA * Dx + CosA * Dy + SrcCentreY;

			const int32 SampleX = FMath::FloorToInt(SrcX);
			const int32 SampleY = FMath::FloorToInt(SrcY);

			FColor Pixel = FillColor;
			if (SampleX >= 0 && SampleX < SrcSize.X && SampleY >= 0 && SampleY < SrcSize.Y)
			{
				Pixel = SrcPixels[SampleY * SrcSize.X + SampleX];
			}
			OutPixels[DstY * OutSize.X + DstX] = Pixel;
		}
	}
}
