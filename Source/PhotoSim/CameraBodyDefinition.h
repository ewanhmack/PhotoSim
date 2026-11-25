// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "CameraBodyDefinition.generated.h"

UENUM(BlueprintType)
enum class ESensorFormat : uint8
{
	FullFrame,
	APSC
};

UENUM(BlueprintType)
enum class ECameraMediumType : uint8
{
	Digital,
	Film
};

/**
 * A camera body's specs - sensor format, whether it shoots digital or film, and (for film)
 * how many shots a roll holds before it needs developing. Purely data, placed per-instance on
 * ACameraPickup rather than subclassed in C++, the same way FLensDefinition works for lenses.
 */
USTRUCT(BlueprintType)
struct FCameraBodyDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Body")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Body")
	ESensorFormat SensorFormat = ESensorFormat::FullFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Body")
	float SensorWidthMM = 36.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Body")
	ECameraMediumType MediumType = ECameraMediumType::Digital;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Body")
	int32 FilmRollCapacity = 36;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Body")
	float WeightKG = 0.7f;

	/** Set only when this body should override the default pickup mesh (e.g. the vintage film body). Left unset for digital variants that reuse SM_Camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Body")
	TSoftObjectPtr<UStaticMesh> OverrideMesh;
};
