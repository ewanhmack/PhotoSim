// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhotoSimGameState.h"

void APhotoSimGameState::InitRound(int32 InObjectivesTotal, float InTimeLimitSeconds)
{
	ObjectivesTotal = InObjectivesTotal;
	ObjectivesCompleted = 0;
	TimeRemainingSeconds = InTimeLimitSeconds;
	RoundState = EPhotoSimRoundState::InProgress;
}

void APhotoSimGameState::NotifySubjectCaptured()
{
	if (RoundState != EPhotoSimRoundState::InProgress)
	{
		return;
	}

	++ObjectivesCompleted;

	if (ObjectivesTotal > 0 && ObjectivesCompleted >= ObjectivesTotal)
	{
		SetRoundState(EPhotoSimRoundState::Won);
	}
}

void APhotoSimGameState::TickTimer(float DeltaSeconds)
{
	if (RoundState != EPhotoSimRoundState::InProgress)
	{
		return;
	}

	TimeRemainingSeconds = FMath::Max(0.f, TimeRemainingSeconds - DeltaSeconds);

	if (TimeRemainingSeconds <= 0.f)
	{
		SetRoundState(EPhotoSimRoundState::Lost);
	}
}

void APhotoSimGameState::SetRoundState(EPhotoSimRoundState NewState)
{
	if (RoundState == NewState)
	{
		return;
	}

	RoundState = NewState;
	OnRoundStateChanged.Broadcast(NewState);
}
