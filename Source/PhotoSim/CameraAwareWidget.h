// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CameraAwareWidget.generated.h"

class UTP_CameraComponent;

UINTERFACE(BlueprintType)
class PHOTOSIM_API UCameraAwareWidget : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement this on any viewfinder widget - C++ or a pure Blueprint one - that needs a reference
 * to the camera it's being shown for. ToggleViewfinder calls this automatically on whatever class
 * is set as ViewfinderWidgetClass, so a Blueprint widget gets the camera for free instead of
 * having to fish for it via GetOwningPlayerPawn + GetComponentByClass itself.
 */
class PHOTOSIM_API ICameraAwareWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Camera")
	void SetOwningCamera(UTP_CameraComponent* Camera);
};
