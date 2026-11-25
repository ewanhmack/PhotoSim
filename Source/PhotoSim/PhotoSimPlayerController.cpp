// Copyright Epic Games, Inc. All Rights Reserved.


#include "PhotoSimPlayerController.h"
#include "PauseMenuWidget.h"
#include "StaminaWidget.h"
#include "StartMenuWidget.h"
#include "EndOfRoundWidget.h"
#include "ObjectiveTrackerWidget.h"
#include "PhotoSubjectComponent.h"
#include "PhotoSimGameMode.h"
#include "PhotoSimGameState.h"
#include "PhotoSimCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
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

	TSubclassOf<UStaminaWidget> WidgetClass = StaminaWidgetClass;
	if (WidgetClass == nullptr)
	{
		// No Blueprint child assigned - fall back to the base C++ class directly, since this
		// widget needs no graph and works with zero manual Blueprint setup.
		WidgetClass = UStaminaWidget::StaticClass();
	}

	StaminaWidgetInstance = CreateWidget<UStaminaWidget>(this, WidgetClass);
	if (StaminaWidgetInstance != nullptr)
	{
		StaminaWidgetInstance->AddToViewport();
		StaminaWidgetInstance->SetOwningCharacter(Cast<APhotoSimCharacter>(GetPawn()));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("'%s' failed to create its stamina HUD widget"), *GetNameSafe(this));
	}

	if (APhotoSimGameState* PhotoGameState = GetWorld()->GetGameState<APhotoSimGameState>())
	{
		PhotoGameState->OnRoundStateChanged.AddUObject(this, &APhotoSimPlayerController::HandleRoundStateChanged);
	}

	ShowStartMenu();
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

void APhotoSimPlayerController::ShowStartMenu()
{
	if (StartMenuInstance != nullptr)
	{
		return;
	}

	TSubclassOf<UStartMenuWidget> WidgetClass = StartMenuWidgetClass;
	if (WidgetClass == nullptr)
	{
		WidgetClass = UStartMenuWidget::StaticClass();
	}

	StartMenuInstance = CreateWidget<UStartMenuWidget>(this, WidgetClass);
	if (StartMenuInstance == nullptr)
	{
		return;
	}

	StartMenuInstance->OnStartRequested.BindUObject(this, &APhotoSimPlayerController::HandleStartRequested);
	StartMenuInstance->AddToViewport(300);

	SetShowMouseCursor(true);
	SetInputMode(FInputModeUIOnly().SetWidgetToFocus(StartMenuInstance->TakeWidget()).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
	SetPause(true);
}

void APhotoSimPlayerController::HandleStartRequested()
{
	if (StartMenuInstance != nullptr)
	{
		StartMenuInstance->RemoveFromParent();
	}

	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	SetPause(false);

	APhotoSimGameMode* PhotoGameMode = GetWorld()->GetAuthGameMode<APhotoSimGameMode>();
	if (PhotoGameMode == nullptr)
	{
		return;
	}

	PhotoGameMode->StartRound();

	TSubclassOf<UObjectiveTrackerWidget> WidgetClass = ObjectiveTrackerWidgetClass;
	if (WidgetClass == nullptr)
	{
		WidgetClass = UObjectiveTrackerWidget::StaticClass();
	}

	ObjectiveTrackerInstance = CreateWidget<UObjectiveTrackerWidget>(this, WidgetClass);
	if (ObjectiveTrackerInstance != nullptr)
	{
		ObjectiveTrackerInstance->SetSubjects(PhotoGameMode->GetSubjects());
		ObjectiveTrackerInstance->AddToViewport();
	}
}

void APhotoSimPlayerController::HandleRoundStateChanged(EPhotoSimRoundState NewState)
{
	if (NewState == EPhotoSimRoundState::InProgress)
	{
		return;
	}

	APhotoSimGameState* PhotoGameState = GetWorld()->GetGameState<APhotoSimGameState>();
	if (PhotoGameState == nullptr)
	{
		return;
	}

	if (EndOfRoundInstance == nullptr)
	{
		TSubclassOf<UEndOfRoundWidget> WidgetClass = EndOfRoundWidgetClass;
		if (WidgetClass == nullptr)
		{
			WidgetClass = UEndOfRoundWidget::StaticClass();
		}

		EndOfRoundInstance = CreateWidget<UEndOfRoundWidget>(this, WidgetClass);
		if (EndOfRoundInstance != nullptr)
		{
			EndOfRoundInstance->OnRestartRequested.BindUObject(this, &APhotoSimPlayerController::HandleRestartRequested);
		}
	}

	if (EndOfRoundInstance == nullptr)
	{
		return;
	}

	EndOfRoundInstance->SetOutcome(NewState, PhotoGameState->ObjectivesCompleted, PhotoGameState->ObjectivesTotal);
	EndOfRoundInstance->AddToViewport(300);

	SetShowMouseCursor(true);
	SetInputMode(FInputModeUIOnly().SetWidgetToFocus(EndOfRoundInstance->TakeWidget()).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
	SetPause(true);
}

void APhotoSimPlayerController::HandleRestartRequested()
{
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()));
}
