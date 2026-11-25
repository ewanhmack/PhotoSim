// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StaminaWidget.generated.h"

class APhotoSimCharacter;

/**
 * Always-visible bottom-left HUD element: a stamina bar plus a carried-weight readout. Built
 * entirely in C++ via Slate, same house style as the other HUD widgets.
 */
UCLASS()
class PHOTOSIM_API UStaminaWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Defined in the .cpp, not inline here: TWeakObjectPtr's assignment operator needs the full
	// APhotoSimCharacter definition, not just this header's forward declaration.
	void SetOwningCharacter(APhotoSimCharacter* InCharacter);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TOptional<float> GetStaminaPercent() const;
	FSlateColor GetStaminaBarColor() const;
	FText GetCarriedWeightText() const;

	TWeakObjectPtr<APhotoSimCharacter> OwningCharacter;
};
