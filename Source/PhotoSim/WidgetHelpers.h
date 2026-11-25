// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"

// Shared by every HUD widget that needs a plain filled rectangle (brackets, scrims, dividers).
// Inline so it's safe to include from multiple translation units that may get unity-combined.
inline TSharedRef<SWidget> MakeFlatColorBox(const FLinearColor& Color)
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(Color);
}

// Scales (NativeWidth, NativeHeight) down to fit within (MaxWidth, MaxHeight) preserving aspect -
// used by the gallery so a portrait photo displays tall instead of being squashed into a
// landscape-shaped box. Falls back to the max box itself if the native size is degenerate.
inline FVector2D ComputeAspectFitSize(float NativeWidth, float NativeHeight, float MaxWidth, float MaxHeight)
{
	if (NativeWidth <= 0.f || NativeHeight <= 0.f)
	{
		return FVector2D(MaxWidth, MaxHeight);
	}
	const float Scale = FMath::Min(MaxWidth / NativeWidth, MaxHeight / NativeHeight);
	return FVector2D(NativeWidth * Scale, NativeHeight * Scale);
}
