// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhotoSimGameMode.h"
#include "PhotoSimCharacter.h"
#include "PhotoSimGameState.h"
#include "PhotoSubjectComponent.h"
#include "PhotoObjectiveArea.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

APhotoSimGameMode::APhotoSimGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	GameStateClass = APhotoSimGameState::StaticClass();
}

void APhotoSimGameMode::BeginPlay()
{
	Super::BeginPlay();

	Subjects.Reset();
	for (TActorIterator<APhotoObjectiveArea> It(GetWorld()); It; ++It)
	{
		if (UPhotoSubjectComponent* Subject = It->PhotoSubject)
		{
			Subjects.Add(Subject);
			Subject->OnCaptured.AddUObject(this, &APhotoSimGameMode::HandleSubjectCaptured);
		}
	}

	if (Subjects.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PhotoSimGameMode] No APhotoObjectiveArea found in the level - place one (or more) so the round has something to win."));
	}

	if (APhotoSimGameState* PhotoGameState = GetGameState<APhotoSimGameState>())
	{
		PhotoGameState->InitRound(Subjects.Num(), TimeLimitSeconds);
	}
}

void APhotoSimGameMode::StartRound()
{
	GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &APhotoSimGameMode::TickRoundTimer, TimerTickInterval, true);
}

void APhotoSimGameMode::TryCaptureObjectiveAt(const FVector& Location, const FVector& Forward)
{
	for (TActorIterator<APhotoObjectiveArea> It(GetWorld()); It; ++It)
	{
		It->TryCaptureFrom(Location, Forward);
	}
}

void APhotoSimGameMode::HandleSubjectCaptured()
{
	if (APhotoSimGameState* PhotoGameState = GetGameState<APhotoSimGameState>())
	{
		PhotoGameState->NotifySubjectCaptured();
	}
}

void APhotoSimGameMode::TickRoundTimer()
{
	APhotoSimGameState* PhotoGameState = GetGameState<APhotoSimGameState>();
	if (PhotoGameState == nullptr)
	{
		return;
	}

	PhotoGameState->TickTimer(TimerTickInterval);

	if (PhotoGameState->RoundState != EPhotoSimRoundState::InProgress)
	{
		GetWorldTimerManager().ClearTimer(RoundTimerHandle);
	}
}
