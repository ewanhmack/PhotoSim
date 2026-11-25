// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LensDefinition.h"
#include "LensJSONImporter.generated.h"

/**
 * Parses Julien's lens catalog JSON (focal/fstop/focusDistance/weight/prime per entry) into
 * FLensDefinition structs. See ParseFocalRange/ParseFstopRange for the string formats accepted.
 */
UCLASS()
class PHOTOSIM_API ULensJSONImporter : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Lens Import")
	static bool ImportLensesFromFile(const FString& AbsoluteFilePath, TArray<FLensDefinition>& OutLenses, TArray<FString>& OutWarnings);

	UFUNCTION(BlueprintCallable, Category = "Lens Import")
	static bool ImportLensesFromString(const FString& JSONString, TArray<FLensDefinition>& OutLenses, TArray<FString>& OutWarnings);

	/** "24mm" -> Min=Max=24. "14-24mm" -> Min=14, Max=24. False (with a warning appended) if unparseable. */
	static bool ParseFocalRange(const FString& Focal, float& OutMin, float& OutMax);

	/** "f/2.8" -> both ends 2.8. "f/4.5-5.6" -> OutWideAtMin=4.5, OutWideAtMax=5.6. False (with a warning appended) if unparseable. */
	static bool ParseFstopRange(const FString& Fstop, float& OutWideAtMin, float& OutWideAtMax);

	/**
	 * Grid-inventory footprint for a lens, derived from physical size proxies rather than stored
	 * per catalog entry: width from reach (a long lens occupies more width - mirrors how
	 * ALensPickup::ComputeLengthScaleForFocalLength already scales the world mesh by the same
	 * MaxFocalLengthMM), height from how heavy/chunky it is. A 50mm prime (reach <= 50mm, under
	 * 1.2kg) comes out 1x1; a 1.1kg 100-400mm zoom comes out 2x1.
	 */
	static void ComputeGridFootprint(float MaxFocalLengthMM, float WeightKG, int32& OutWidth, int32& OutHeight);
};
