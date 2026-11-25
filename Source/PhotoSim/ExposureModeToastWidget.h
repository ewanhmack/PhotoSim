// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ExposureModeToastWidget.generated.h"

/**
 * Small text toast on the side of the screen naming the exposure mode just switched to, then
 * fading out. Built purely in C++/Slate and driven by NativeTick, mirroring UCameraShutterWidget's
 * close/hold/open timing pattern.
 */
UCLASS()
class PHOTOSIM_API UExposureModeToastWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Restarts the toast showing ModeText */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ShowMode(const FText& ModeText);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FText GetModeText() const { return CurrentModeText; }
	FSlateColor GetTextColor() const;

	FText CurrentModeText;

	/** Negative while idle */
	float ElapsedTime = -1.f;

	static constexpr float HoldDuration = 1.2f;
	static constexpr float FadeDuration = 0.4f;
};
