// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CameraShutterWidget.generated.h"

/**
 * Full-screen shutter-blade wipe shown when a photo is taken: two black bars slide in from the
 * top and bottom edges to meet at the centre, hold briefly, then slide back open. Built purely
 * in C++/Slate (no UMG designer asset needed) and driven by NativeTick rather than a keyframed
 * animation.
 */
UCLASS()
class PHOTOSIM_API UCameraShutterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Starts (or restarts) the close/open animation */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void PlayShutter();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FOptionalSize GetBladeHeight() const;

	/** Negative while idle */
	float ElapsedTime = -1.f;
	FVector2D LastSize = FVector2D::ZeroVector;

	static constexpr float CloseDuration = 0.06f;
	static constexpr float HoldDuration = 0.05f;
	static constexpr float OpenDuration = 0.12f;
};
