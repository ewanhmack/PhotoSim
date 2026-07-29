// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ViewfinderWidget.generated.h"

class UTP_CameraComponent;

/**
 * Viewfinder HUD overlay built entirely in C++ via Slate (letterbox bars, rule-of-thirds grid,
 * corner brackets, centre reticle, focal length readout) so it works without any UMG designer
 * asset. Shown while the camera prop is being aimed through.
 */
UCLASS()
class PHOTOSIM_API UViewfinderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Lets the readout react to the camera's current zoom level */
	void SetOwningCamera(UTP_CameraComponent* InCamera);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedRef<SWidget> MakeRuleOfThirdsGrid() const;
	TSharedRef<SWidget> MakeExposureReadout() const;

	FText GetFocalLengthText() const;
	FText GetApertureText() const;
	FText GetShutterSpeedText() const;
	FText GetISOText() const;

	TWeakObjectPtr<UTP_CameraComponent> OwningCamera;
};
