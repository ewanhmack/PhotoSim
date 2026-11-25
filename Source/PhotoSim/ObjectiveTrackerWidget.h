// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ObjectiveTrackerWidget.generated.h"

class UPhotoSubjectComponent;

/**
 * Small always-on HUD panel listing every objective and whether it's been photographed yet. Built
 * once from whatever APhotoSimGameMode::GetSubjects() returns at round start - the list itself
 * doesn't change mid-round, only each row's captured state, which is read live via an attribute
 * binding (Subject->bIsCaptured) rather than rebuilding the widget on every capture.
 */
UCLASS()
class PHOTOSIM_API UObjectiveTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Must be called before AddToViewport - RebuildWidget uses whatever this was last set to */
	void SetSubjects(const TArray<TObjectPtr<UPhotoSubjectComponent>>& InSubjects);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedRef<SWidget> MakeObjectiveRow(TWeakObjectPtr<UPhotoSubjectComponent> Subject) const;
	FText GetObjectiveRowText(TWeakObjectPtr<UPhotoSubjectComponent> Subject) const;
	FSlateColor GetObjectiveRowColor(TWeakObjectPtr<UPhotoSubjectComponent> Subject) const;

	TArray<TWeakObjectPtr<UPhotoSubjectComponent>> Subjects;
};
