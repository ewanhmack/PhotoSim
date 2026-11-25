// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PhotoSimGameState.generated.h"

UENUM(BlueprintType)
enum class EPhotoSimRoundState : uint8
{
	InProgress,
	Won,
	Lost
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoundStateChanged, EPhotoSimRoundState /*NewState*/);

/**
 * Owns the vertical slice's win/lose state: how many of the level's UPhotoSubjectComponents have
 * been photographed, and how much time is left to get the rest. APhotoSimGameMode discovers
 * subjects and drives the round timer; this class owns the actual decision of when the round ends,
 * so the rule ("captured them all, or ran out of time") lives in exactly one place and is testable
 * without spawning a GameMode or a world full of actors.
 */
UCLASS()
class PHOTOSIM_API APhotoSimGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Round")
	EPhotoSimRoundState RoundState = EPhotoSimRoundState::InProgress;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 ObjectivesCompleted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 ObjectivesTotal = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	float TimeRemainingSeconds = 0.f;

	/** Fires exactly once, on the InProgress -> Won/Lost transition */
	FOnRoundStateChanged OnRoundStateChanged;

	/** Resets round tracking for a fresh InProgress round with InObjectivesTotal subjects and InTimeLimitSeconds on the clock */
	void InitRound(int32 InObjectivesTotal, float InTimeLimitSeconds);

	/** Call once per subject photographed. Wins the round once ObjectivesCompleted reaches ObjectivesTotal. No-op once the round is already decided. */
	void NotifySubjectCaptured();

	/** Counts the clock down by DeltaSeconds. Loses the round once it reaches zero. No-op once the round is already decided. */
	void TickTimer(float DeltaSeconds);

private:
	void SetRoundState(EPhotoSimRoundState NewState);
};
