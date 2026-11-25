// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PhotoSimGameMode.generated.h"

class UPhotoSubjectComponent;

/**
 * Discovers every UPhotoSubjectComponent in the level at BeginPlay and wires them, plus a
 * countdown timer, into APhotoSimGameState - see that class for the actual win/lose decision.
 * This class is deliberately thin: it owns finding subjects and ticking the clock, nothing else.
 */
UCLASS(minimalapi)
class APhotoSimGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APhotoSimGameMode();

	/** Seconds the player has to photograph every subject before the round is lost. Counts down from StartRound, not from BeginPlay, so time spent on the start menu is free. */
	UPROPERTY(EditAnywhere, Category = "Round")
	float TimeLimitSeconds = 300.f;

	/** How often the round timer is re-evaluated */
	UPROPERTY(EditAnywhere, Category = "Round")
	float TimerTickInterval = 0.25f;

	/** Begins the round countdown. Called once by the start menu. */
	UFUNCTION(BlueprintCallable, Category = "Round")
	void StartRound();

	/** Tests Location/Forward - the player's position and camera direction at the moment a photo was taken - against every APhotoObjectiveArea in the level */
	void TryCaptureObjectiveAt(const FVector& Location, const FVector& Forward);

	/** Every objective found at BeginPlay, for an objective-tracker HUD to list. Each one's DisplayName/bIsCaptured can be read directly off the component. */
	const TArray<TObjectPtr<UPhotoSubjectComponent>>& GetSubjects() const { return Subjects; }

protected:
	virtual void BeginPlay() override;

private:
	void HandleSubjectCaptured();
	void TickRoundTimer();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPhotoSubjectComponent>> Subjects;

	FTimerHandle RoundTimerHandle;
};
