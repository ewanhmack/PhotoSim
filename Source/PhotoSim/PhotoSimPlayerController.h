// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PhotoSimPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UPauseMenuWidget;
struct FInputActionValue;

/**
 *
 */
UCLASS()
class PHOTOSIM_API APhotoSimPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input Mapping Context to be used for player input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputMappingContext* InputMappingContext;

	/** Optional override for the pause menu widget class. Defaults to UPauseMenuWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pause Menu")
	TSubclassOf<UPauseMenuWidget> PauseMenuWidgetClass;

	/** Mapping Context for the pause toggle. Real asset - see /Game/PhotoSim/Input/. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pause Menu")
	TObjectPtr<UInputMappingContext> PauseMappingContext;

	/** Pause toggle action. Real asset - see /Game/PhotoSim/Input/. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pause Menu")
	TObjectPtr<UInputAction> PauseAction;

public:
	APhotoSimPlayerController();

	// Begin Actor interface
protected:

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// End Actor interface

public:
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void PauseGame();

	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void ResumeGame();

private:
	void TogglePauseMenu(const FInputActionValue& Value);

	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuWidget> PauseMenuInstance;

	bool bIsPaused = false;
};
