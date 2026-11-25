// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
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

	/**
	 * How much one scroll notch changes focal length, in mm. Tunable per lens rather than a
	 * single global step, since a fixed alpha-based step made every lens take the same relative
	 * number of notches to sweep its own range regardless of how many mm that range actually
	 * spans - fine for a 10mm-wide lens, but made a 400mm telephoto range feel like it needed a
	 * lot of scrolling to get anywhere.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float ZoomStepMM = 5.f;

	/**
	 * Closest distance this specific lens can focus - real lenses vary a lot here (wide lenses
	 * often focus much closer than telephotos). The far limit isn't lens-specific: every lens
	 * can focus to infinity, so that's a single shared constant (UTP_CameraComponent::MaxFocusDistance).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float MinFocusDistanceCM = 30.f;

	/** Widest possible aperture at MinFocalLengthMM. Equal to WideOpenApertureAtMaxFocalLength for a prime or a constant-aperture zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float WideOpenApertureAtMinFocalLength = 2.8f;

	/** Widest possible aperture at MaxFocalLengthMM. Equal to WideOpenApertureAtMinFocalLength for a prime or a constant-aperture zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float WideOpenApertureAtMaxFocalLength = 2.8f;

	/** Physical weight of this lens, for the carry-weight system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float WeightKG = 0.5f;

	/** Set only when this lens should override the default placeholder mesh. Left unset for the whole JSON catalog - no individual lens models exist yet, so ALensPickup falls back to a cylinder sized by focal length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	TSoftObjectPtr<UStaticMesh> OverrideMesh;

	/** How many cells wide this lens occupies in the grid inventory. See ULensJSONImporter::ComputeGridFootprint for how the JSON catalog derives this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Inventory")
	int32 GridWidth = 1;

	/** How many cells tall this lens occupies in the grid inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Inventory")
	int32 GridHeight = 1;

	bool IsPrime() const { return FMath::IsNearlyEqual(MinFocalLengthMM, MaxFocalLengthMM); }
};
