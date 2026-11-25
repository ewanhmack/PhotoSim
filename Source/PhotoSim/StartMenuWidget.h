// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartMenuWidget.generated.h"

/**
 * The game's opening screen: title, a one-line brief, and a Start button. Built entirely in C++
 * via Slate, matching UPauseMenuWidget's house conventions (MakeFlatColorBox scrim, FCoreStyle
 * fonts/buttons) - see that class for the pattern this follows.
 */
UCLASS()
class PHOTOSIM_API UStartMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bound by whoever shows this menu (the PlayerController) to actually begin the round */
	FSimpleDelegate OnStartRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FReply OnStartClicked();
};
