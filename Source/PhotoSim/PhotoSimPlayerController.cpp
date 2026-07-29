// Copyright Epic Games, Inc. All Rights Reserved.


#include "PhotoSimPlayerController.h"
#include "PauseMenuWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"

APhotoSimPlayerController::APhotoSimPlayerController()
{
	// Real assets (see /Game/PhotoSim/Input/), not created at runtime - see TP_CameraComponent's
	// constructor comment for why: runtime-created UInputAction/UInputMappingContext instances
	// destabilize editor systems that enumerate objects, regardless of when they're created.
	static ConstructorHelpers::FObjectFinder<UInputAction> PauseActionAsset(TEXT("/Game/PhotoSim/Input/IA_Pause.IA_Pause"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> PauseMappingContextAsset(TEXT("/Game/PhotoSim/Input/IMC_Pause.IMC_Pause"));
	PauseAction = PauseActionAsset.Object;
	PauseMappingContext = PauseMappingContextAsset.Object;
}

void APhotoSimPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// get the enhanced input subsystem
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		// add the mapping context so we get controls
		Subsystem->AddMappingContext(InputMappingContext, 0);
	}
}

void APhotoSimPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		// High priority so Escape always reaches the pause toggle regardless of what else is held
		Subsystem->AddMappingContext(PauseMappingContext, 10);
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(PauseAction, ETriggerEvent::Started, this, &APhotoSimPlayerController::TogglePauseMenu);
	}
}

void APhotoSimPlayerController::TogglePauseMenu(const FInputActionValue& Value)
{
	if (bIsPaused)
	{
		ResumeGame();
	}
	else
	{
		PauseGame();
	}
}

void APhotoSimPlayerController::PauseGame()
{
	if (bIsPaused)
	{
		return;
	}

	if (PauseMenuInstance == nullptr)
	{
		TSubclassOf<UPauseMenuWidget> WidgetClass = PauseMenuWidgetClass;
		if (WidgetClass == nullptr)
		{
			WidgetClass = UPauseMenuWidget::StaticClass();
		}

		PauseMenuInstance = CreateWidget<UPauseMenuWidget>(this, WidgetClass);
		if (PauseMenuInstance != nullptr)
		{
			PauseMenuInstance->OnResumeRequested.BindUObject(this, &APhotoSimPlayerController::ResumeGame);
		}
	}

	if (PauseMenuInstance == nullptr)
	{
		return;
	}

	PauseMenuInstance->AddToViewport(200);

	bIsPaused = true;
	SetShowMouseCursor(true);
	SetInputMode(FInputModeUIOnly().SetWidgetToFocus(PauseMenuInstance->TakeWidget()).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
	SetPause(true);
}

void APhotoSimPlayerController::ResumeGame()
{
	if (!bIsPaused)
	{
		return;
	}

	if (PauseMenuInstance != nullptr)
	{
		PauseMenuInstance->RemoveFromParent();
	}

	bIsPaused = false;
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	SetPause(false);
}
