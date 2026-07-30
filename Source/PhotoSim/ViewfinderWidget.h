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
	TSharedRef<SWidget> MakeFocusBar() const;

	FText GetFocalLengthText() const;
	FText GetApertureText() const;
	FText GetShutterSpeedText() const;
	FText GetISOText() const;
	FText GetFocusDistanceText() const;

	/** 0-1 position of the current focus distance along the (log-scaled) min/max focus range */
	float GetFocusFraction() const;
	/** Left padding, in pixels, that positions the focus bar's marker inside its fixed-width track */
	FMargin GetFocusMarkerPadding() const;

	TWeakObjectPtr<UTP_CameraComponent> OwningCamera;
};
