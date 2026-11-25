// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PhotoSimGameState.h"
#include "EndOfRoundWidget.generated.h"

/**
 * Shown once APhotoSimGameState::OnRoundStateChanged fires. Reports the outcome and how many
 * subjects were photographed, with a button to reload the level and try again.
 */
UCLASS()
class PHOTOSIM_API UEndOfRoundWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bound by whoever shows this widget (the PlayerController) to reload the level */
	FSimpleDelegate OnRestartRequested;

	/** Call before AddToViewport so RebuildWidget has something to show */
	void SetOutcome(EPhotoSimRoundState InOutcome, int32 InObjectivesCompleted, int32 InObjectivesTotal);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FReply OnRestartClicked();
	FText GetHeadlineText() const;
	FText GetSubtitleText() const;

	EPhotoSimRoundState Outcome = EPhotoSimRoundState::InProgress;
	int32 ObjectivesCompleted = 0;
	int32 ObjectivesTotal = 0;
};
