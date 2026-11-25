// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhotoObjectiveArea.generated.h"

class UBoxComponent;
class UTextRenderComponent;
class UPhotoSubjectComponent;

/**
 * A placeable objective: photograph this spot while standing inside AreaVolume and facing roughly
 * the direction this actor itself points. Rotate the actor in the level to set the required
 * facing; resize AreaVolume to set where the player needs to be standing - no raycast, no focus
 * requirement, just "were you here, looking that way, when the shutter fired."
 *
 * TP_CameraComponent::TakePhoto reports the player's location and camera forward to
 * APhotoSimGameMode::TryCaptureObjectiveAt on every shot, which tests it against every area in the
 * level. Label is a Text Render Component per the brief's own suggestion for annotating level
 * zones - visible in the editor always, in-game only if bShowLabelInGame is set.
 */
UCLASS()
class PHOTOSIM_API APhotoObjectiveArea : public AActor
{
	GENERATED_BODY()

public:
	APhotoObjectiveArea();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Photo Subject")
	TObjectPtr<UPhotoSubjectComponent> PhotoSubject;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective Area")
	TObjectPtr<UBoxComponent> AreaVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective Area")
	TObjectPtr<UTextRenderComponent> Label;

	/** Half-angle, in degrees, around this actor's forward vector the player's camera must be facing for a photo taken here to count */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective Area")
	float RequiredFacingToleranceDegrees = 45.f;

	/** Shows Label in-game, not just in-editor. Off by default - a floating prompt over the subject rather defeats the point of finding the shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective Area")
	bool bShowLabelInGame = false;

	/**
	 * Tests CandidateLocation against the area and CandidateForward against the required facing;
	 * if both pass, captures PhotoSubject. Called with the player's location and camera forward at
	 * the moment the shutter fires. Returns false if either check fails or the subject is already captured.
	 */
	bool TryCaptureFrom(const FVector& CandidateLocation, const FVector& CandidateForward);

	/** True if LocalPoint, already in the box's own local (unscaled) space, is within +-Extent on every axis. Pure so the area test is unit-testable without a live UBoxComponent. */
	UFUNCTION(BlueprintPure, Category = "Objective Area")
	static bool IsPointInsideBoxExtent(const FVector& LocalPoint, const FVector& Extent);

	/** True if CandidateForward is within ToleranceDegrees of RequiredForward. Both are expected normalized. */
	UFUNCTION(BlueprintPure, Category = "Objective Area")
	static bool IsWithinFacingTolerance(const FVector& CandidateForward, const FVector& RequiredForward, float ToleranceDegrees);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	bool IsLocationInsideArea(const FVector& CandidateLocation) const;
	bool IsLookingInRightDirection(const FVector& CandidateForward) const;
};
