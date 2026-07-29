// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class SWidgetSwitcher;
class SWrapBox;
struct FSlateBrush;

/**
 * Pause menu with Resume / Gallery / Quit, built entirely in C++ via Slate. The gallery and the
 * enlarged photo view are further "pages" within the same widget (swapped via an
 * SWidgetSwitcher): a scrollable grid of thumbnails, camera-roll style, and clicking one swaps
 * to a large view of that photo.
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

private:
	TSharedRef<SWidget> MakeMenuButton(const FString& Label, FOnClicked OnClicked) const;
	void RefreshGallery();
	const FSlateBrush* GetEnlargedBrush() const;

	FReply OnResumeClicked();
	FReply OnGalleryClicked();
	FReply OnQuitClicked();
	FReply OnBackClicked();
	FReply OnThumbnailClicked(int32 PhotoIndex);
	FReply OnBackFromEnlargedClicked();

	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SWrapBox> GalleryWrapBox;

	/** Keeps the brushes referencing each photo's render target alive while the gallery is shown */
	TArray<TSharedPtr<FSlateBrush>> GalleryBrushes;

	/** Which brush the enlarged-view page currently shows */
	TSharedPtr<FSlateBrush> EnlargedBrush;
};
