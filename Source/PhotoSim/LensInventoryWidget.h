// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LensInventoryWidget.generated.h"

class UTP_CameraComponent;

/**
 * Lens picker, built entirely in C++ via Slate (same dark camera-app styling as the pause
 * menu). Lists every lens in the owning camera's AvailableLenses with its aperture/focal length
 * range; clicking one equips it and closes the menu.
 */
UCLASS()
class PHOTOSIM_API ULensInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Defined in the .cpp, not inline here: TWeakObjectPtr's assignment operator needs the full
	// UTP_CameraComponent definition, not just this header's forward declaration.
	void SetOwningCamera(UTP_CameraComponent* InCamera);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedRef<SWidget> MakeLensRow(int32 LensIndex);
	FReply OnLensClicked(int32 LensIndex);

	TWeakObjectPtr<UTP_CameraComponent> OwningCamera;
};
