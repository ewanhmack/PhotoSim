// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhotoSubjectComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnSubjectCaptured);

/**
 * Tags an actor as a photo objective. Add this to anything in the level that should count toward
 * the round's win condition - a patrol NPC, a landmark, a prop. UTP_CameraComponent::TakePhoto
 * looks for one of these on whatever the autofocus raycast is currently resting on at the moment
 * the shutter fires and calls TryCapture on it, so being in focus and in frame is what "counts" -
 * no separate detection logic needed.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PHOTOSIM_API UPhotoSubjectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Purely descriptive - for an objective list / HUD prompt, if anything reads it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Photo Subject")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Photo Subject")
	bool bIsCaptured = false;

	/** Marks this subject captured and broadcasts OnCaptured. Returns false if it was already captured. */
	bool TryCapture();

	FOnSubjectCaptured OnCaptured;
};
