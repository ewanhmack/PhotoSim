// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhotoSubjectComponent.h"

bool UPhotoSubjectComponent::TryCapture()
{
	if (bIsCaptured)
	{
		return false;
	}

	bIsCaptured = true;
	OnCaptured.Broadcast();
	return true;
}
