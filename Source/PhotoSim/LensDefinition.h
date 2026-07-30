// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LensDefinition.generated.h"

/**
 * A swappable lens's specs - what aperture and focal length range it allows once equipped.
 * Purely data for now; no 3D model or pickup of its own yet.
 */
USTRUCT(BlueprintType)
struct FLensDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	FText Name;

	/** Widest aperture this lens allows (lowest f-number, so brightest / shallowest depth of field) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float MinAperture = 2.8f;

	/** Narrowest aperture this lens allows (highest f-number) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float MaxAperture = 16.f;

	/** Widest end of the zoom range. Equal to MaxFocalLengthMM for a fixed (prime) lens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float MinFocalLengthMM = 24.f;

	/** Most zoomed-in end of the zoom range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float MaxFocalLengthMM = 70.f;
};
