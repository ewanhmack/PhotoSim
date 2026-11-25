// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PhotoSimPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UPauseMenuWidget;
class UStaminaWidget;
class UStartMenuWidget;
class UEndOfRoundWidget;
class UObjectiveTrackerWidget;
struct FInputActionValue;
enum class EPhotoSimRoundState : uint8;

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

	/** Optional override for the stamina HUD widget class. Defaults to UStaminaWidget. */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UStaminaWidget> StaminaWidgetClass;

	/** Optional override for the opening start-menu widget class. Defaults to UStartMenuWidget. */
	UPROPERTY(EditDefaultsOnly, Category = "Start Menu")
	TSubclassOf<UStartMenuWidget> StartMenuWidgetClass;

	/** Optional override for the win/lose screen widget class. Defaults to UEndOfRoundWidget. */
	UPROPERTY(EditDefaultsOnly, Category = "Round")
	TSubclassOf<UEndOfRoundWidget> EndOfRoundWidgetClass;

	/** Optional override for the objective-tracker HUD widget class. Defaults to UObjectiveTrackerWidget. */
	UPROPERTY(EditDefaultsOnly, Category = "Round")
	TSubclassOf<UObjectiveTrackerWidget> ObjectiveTrackerWidgetClass;

	// Transient, not EditAnywhere: re-resolved by the constructor via ConstructorHelpers every
	// time, so there's nothing for a Blueprint to serialize a stale (e.g. None) override onto.

	/** Mapping Context for the pause toggle. Real asset - see /Game/PhotoSim/Input/. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pause Menu")
	TObjectPtr<UInputMappingContext> PauseMappingContext;

	/** Pause toggle action. Real asset - see /Game/PhotoSim/Input/. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pause Menu")
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

	void ShowStartMenu();
	void HandleStartRequested();
	void HandleRoundStateChanged(EPhotoSimRoundState NewState);
	void HandleRestartRequested();

	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuWidget> PauseMenuInstance;

	UPROPERTY()
	TObjectPtr<UStaminaWidget> StaminaWidgetInstance;

	UPROPERTY(Transient)
	TObjectPtr<UStartMenuWidget> StartMenuInstance;

	UPROPERTY(Transient)
	TObjectPtr<UEndOfRoundWidget> EndOfRoundInstance;

	UPROPERTY(Transient)
	TObjectPtr<UObjectiveTrackerWidget> ObjectiveTrackerInstance;

	bool bIsPaused = false;
};
