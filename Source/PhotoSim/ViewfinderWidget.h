// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rendering/SlateRenderTransform.h"
#include "Types/SlateStructs.h"
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

	// Public and Blueprint-bindable so a Designer-built child widget (one with its own widget
	// tree, which replaces RebuildWidget() below rather than adding to it) can bind Text blocks
	// straight to these instead of losing the readout entirely.
	UFUNCTION(BlueprintPure, Category = "Camera|Readout")
	FText GetFocalLengthText() const;

	UFUNCTION(BlueprintPure, Category = "Camera|Readout")
	FText GetApertureText() const;

	UFUNCTION(BlueprintPure, Category = "Camera|Readout")
	FText GetShutterSpeedText() const;

	UFUNCTION(BlueprintPure, Category = "Camera|Readout")
	FText GetISOText() const;

	UFUNCTION(BlueprintPure, Category = "Camera|Readout")
	FText GetFocusDistanceText() const;

	UFUNCTION(BlueprintPure, Category = "Camera|Readout")
	FText GetExposureModeText() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TSharedRef<SWidget> MakeRuleOfThirdsGrid() const;
	TSharedRef<SWidget> MakeExposureReadout() const;
	TSharedRef<SWidget> MakeFocusBar() const;

	/** Rule-of-thirds grid + corner AF brackets + centre reticle */
	TSharedRef<SWidget> MakeFramingGuides() const;
	/** Thin outline + MakeFramingGuides - the whole thing is rotated live by GetFrameRenderTransform, rather than this itself changing shape */
	TSharedRef<SWidget> MakeFrame() const;
	/** A black "picture frame" the size of the screen plus a wide margin, with a transparent window cut out the size of the frame - rotates rigidly with it (see RebuildWidget) so whatever the roll turns away from the frame reads as black instead of showing extra scene the photo won't actually capture */
	TSharedRef<SWidget> MakeRotationMask() const;

	/** Rotates the frame by the camera's exact continuous roll - live, not rebuilt, so it tracks every scroll notch instead of snapping between fixed states */
	TOptional<FSlateRenderTransform> GetFrameRenderTransform() const;

	/** 1 at zero roll, shrinking as the roll increases - the largest scale that keeps the screen-shaped frame, once rotated, entirely within the screen rather than letting its corners run off it */
	float GetFrameScale() const;

	/** The frame and its mask's hole both track the viewport size scaled by GetFrameScale, so the frame fills the screen at zero roll and shrinks to stay fully on screen as it rotates */
	FOptionalSize GetFrameWidth() const;
	FOptionalSize GetFrameHeight() const;
	/** The mask's own outer size - the (shrunk) frame size plus a margin on every side wide enough to still cover the screen's corners once rotated */
	FOptionalSize GetMaskOuterWidth() const;
	FOptionalSize GetMaskOuterHeight() const;

	/** 0-1 position of the current focus distance along the (log-scaled) min/max focus range */
	float GetFocusFraction() const;
	/** Left padding, in pixels, that positions the focus bar's marker inside its fixed-width track */
	FMargin GetFocusMarkerPadding() const;

	TWeakObjectPtr<UTP_CameraComponent> OwningCamera;

	// Updated once per NativeTick from the geometry Slate actually hands this widget that frame,
	// rather than read back independently by each of the Get*Width/Height functions above - those
	// used to call GetCachedGeometry()/GetViewportSize() directly, which could each resolve a
	// slightly different size within the same frame (GetCachedGeometry() in particular reads back
	// zero before this widget's first paint) and threw the frame and the mask's hole out of sync
	// with each other.
	FVector2D CachedViewportSize = FVector2D(1920.f, 1080.f);
};
