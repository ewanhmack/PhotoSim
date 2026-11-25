// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateStructs.h"
#include "PauseMenuWidget.generated.h"

class SWidgetSwitcher;
class SScrollBox;
struct FSlateBrush;
class UPhotoLibrarySubsystem;

/**
 * Pause menu with Resume / Gallery / Quit, built entirely in C++ via Slate. The gallery is a
 * camera-style playback page (swapped in via an SWidgetSwitcher): one photo on screen at a time,
 * with a filmstrip to jump between shots and the settings it was taken with read off as an
 * overlay - the shape of a real camera's rear LCD review mode, not a flat thumbnail grid.
 */
UCLASS()
class PHOTOSIM_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bound by whoever opens this menu (the PlayerController) to actually unpause */
	FSimpleDelegate OnResumeRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TSharedRef<SWidget> MakeMenuButton(const FString& Label, FOnClicked OnClicked) const;
	UPhotoLibrarySubsystem* GetPhotoLibrary() const;
	void RefreshGallery();
	void StepPhoto(int32 Direction);

	const FSlateBrush* GetCurrentBrush() const;

	/** Fits the current photo's own size within a bounding box so a portrait photo displays tall rather than stretched into a landscape frame */
	FOptionalSize GetPhotoWidthOverride() const;
	FOptionalSize GetPhotoHeightOverride() const;

	FText GetPhotoCounterText() const;
	FText GetPhotoSettingsText() const;
	FText GetFavoriteButtonLabel() const;
	FSlateColor GetFavoriteButtonColor() const;
	FText GetDeleteButtonLabel() const;
	EVisibility GetEmptyLibraryTextVisibility() const;

	FReply OnResumeClicked();
	FReply OnGalleryClicked();
	FReply OnQuitClicked();
	FReply OnBackClicked();
	FReply OnThumbnailClicked(int32 PhotoIndex);
	FReply OnPrevClicked();
	FReply OnNextClicked();
	FReply OnFavoriteClicked();
	FReply OnDeleteClicked();

	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SScrollBox> FilmstripBox;

	/** Keeps a brush alive per photo, in the same order as UPhotoLibrarySubsystem::GetPhotos(), for as long as the gallery page holds it */
	TArray<TSharedPtr<FSlateBrush>> GalleryBrushes;

	/** Index into the library's Photos array (and GalleryBrushes) the playback page is currently showing */
	int32 CurrentPhotoIndex = 0;

	/** Armed by one click on Delete, cleared by navigating or confirming - stops a single misclick from permanently deleting a photo */
	bool bConfirmingDelete = false;
};
