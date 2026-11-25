// Copyright Epic Games, Inc. All Rights Reserved.

#include "PauseMenuWidget.h"
#include "PhotoLibrarySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "InputCoreTypes.h"
#include "WidgetHelpers.h"

namespace
{
	// The main photo gets this much room before it's scaled down to fit (see ComputeAspectFitSize).
	const FVector2D MaxPhotoSize(720.f, 440.f);
}

TSharedRef<SWidget> UPauseMenuWidget::MakeMenuButton(const FString& Label, FOnClicked OnClicked) const
{
	return SNew(SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.OnClicked(OnClicked)
		.ContentPadding(FMargin(0.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.08f))
			.Padding(FMargin(28.f, 12.f))
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
				.ColorAndOpacity(FLinearColor::White)
			]
		];
}

TSharedRef<SWidget> UPauseMenuWidget::RebuildWidget()
{
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 28);

	TSharedRef<SVerticalBox> MainMenuPage =
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 28.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("PAUSED")))
			.Font(TitleFont)
			.ColorAndOpacity(FLinearColor::White)
		]

		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0.f, 6.f)
		[ MakeMenuButton(TEXT("Resume"), FOnClicked::CreateUObject(this, &UPauseMenuWidget::OnResumeClicked)) ]

		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0.f, 6.f)
		[ MakeMenuButton(TEXT("Gallery"), FOnClicked::CreateUObject(this, &UPauseMenuWidget::OnGalleryClicked)) ]

		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0.f, 6.f)
		[ MakeMenuButton(TEXT("Quit"), FOnClicked::CreateUObject(this, &UPauseMenuWidget::OnQuitClicked)) ];

	TSharedRef<SVerticalBox> GalleryPlaybackPage =
		SNew(SVerticalBox)

		// Header: back button, title, and a running "N / total" counter like a camera's frame count.
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth()
			[ MakeMenuButton(TEXT("< Back"), FOnClicked::CreateUObject(this, &UPauseMenuWidget::OnBackClicked)) ]

			+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("GALLERY")))
				.Font(TitleFont)
				.ColorAndOpacity(FLinearColor::White)
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_UObject(this, &UPauseMenuWidget::GetPhotoCounterText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.6f))
			]
		]

		// The photo itself, full size, with the shot's camera settings overlaid like an LCD readout.
		+ SVerticalBox::Slot().FillHeight(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				SNew(SBox)
				.WidthOverride_UObject(this, &UPauseMenuWidget::GetPhotoWidthOverride)
				.HeightOverride_UObject(this, &UPauseMenuWidget::GetPhotoHeightOverride)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.2f))
					.Padding(FMargin(4.f))
					[
						SNew(SImage).Image_UObject(this, &UPauseMenuWidget::GetCurrentBrush)
					]
				]
			]

			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(14.f)
			[
				SNew(STextBlock)
				.Text_UObject(this, &UPauseMenuWidget::GetPhotoSettingsText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.85f))
			]

			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("No photos yet.")))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.5f))
				.Visibility_UObject(this, &UPauseMenuWidget::GetEmptyLibraryTextVisibility)
			]
		]

		// Filmstrip: a horizontal strip of every shot, click one to jump to it, Prev/Next either side.
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[ MakeMenuButton(TEXT("<"), FOnClicked::CreateUObject(this, &UPauseMenuWidget::OnPrevClicked)) ]

			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SBox).HeightOverride(56.f)
				[
					SAssignNew(FilmstripBox, SScrollBox)
					.Orientation(Orient_Horizontal)
				]
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
			[ MakeMenuButton(TEXT(">"), FOnClicked::CreateUObject(this, &UPauseMenuWidget::OnNextClicked)) ]
		]

		// Actions on the currently shown photo.
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f).HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(0.f))
				.OnClicked(FOnClicked::CreateUObject(this, &UPauseMenuWidget::OnFavoriteClicked))
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.08f))
					.Padding(FMargin(20.f, 10.f))
					[
						SNew(STextBlock)
						.Text_UObject(this, &UPauseMenuWidget::GetFavoriteButtonLabel)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						.ColorAndOpacity_UObject(this, &UPauseMenuWidget::GetFavoriteButtonColor)
					]
				]
			]

			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(0.f))
				.OnClicked(FOnClicked::CreateUObject(this, &UPauseMenuWidget::OnDeleteClicked))
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.08f))
					.Padding(FMargin(20.f, 10.f))
					[
						SNew(STextBlock)
						.Text_UObject(this, &UPauseMenuWidget::GetDeleteButtonLabel)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						.ColorAndOpacity(FLinearColor(1.f, 0.4f, 0.35f))
					]
				]
			]
		];

	return SNew(SOverlay)

		// Full-screen dark scrim behind the menu panel
		+ SOverlay::Slot()
		[
			MakeFlatColorBox(FLinearColor(0.f, 0.f, 0.f, 0.65f))
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.04f, 0.97f))
			.Padding(FMargin(36.f))
			[
				SAssignNew(Switcher, SWidgetSwitcher)

				+ SWidgetSwitcher::Slot()
				[
					MainMenuPage
				]

				+ SWidgetSwitcher::Slot()
				[
					GalleryPlaybackPage
				]
			]
		];
}

UPhotoLibrarySubsystem* UPauseMenuWidget::GetPhotoLibrary() const
{
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UPhotoLibrarySubsystem>() : nullptr;
}

void UPauseMenuWidget::RefreshGallery()
{
	GalleryBrushes.Empty();
	if (FilmstripBox.IsValid())
	{
		FilmstripBox->ClearChildren();
	}

	UPhotoLibrarySubsystem* Library = GetPhotoLibrary();
	const TArray<FPhotoRecord> EmptyPhotos;
	const TArray<FPhotoRecord>& Photos = Library != nullptr ? Library->GetPhotos() : EmptyPhotos;

	for (const FPhotoRecord& Record : Photos)
	{
		TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
		if (Record.Texture != nullptr)
		{
			Brush->SetResourceObject(Record.Texture);
			Brush->ImageSize = FVector2D(Record.Texture->GetSizeX(), Record.Texture->GetSizeY());
			Brush->DrawAs = ESlateBrushDrawType::Image;
		}
		GalleryBrushes.Add(Brush);
	}

	CurrentPhotoIndex = FMath::Clamp(CurrentPhotoIndex, 0, FMath::Max(Photos.Num() - 1, 0));

	if (!FilmstripBox.IsValid())
	{
		return;
	}

	for (int32 Index = 0; Index < GalleryBrushes.Num(); ++Index)
	{
		FilmstripBox->AddSlot()
		.Padding(FMargin(3.f, 0.f))
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.ContentPadding(FMargin(0.f))
			.OnClicked(FOnClicked::CreateUObject(this, &UPauseMenuWidget::OnThumbnailClicked, Index))
			[
				SNew(SBox)
				.WidthOverride(64.f)
				.HeightOverride(40.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([this, Index]()
					{
						return Index == CurrentPhotoIndex ? FLinearColor(1.f, 1.f, 1.f, 0.9f) : FLinearColor(1.f, 1.f, 1.f, 0.15f);
					})
					.Padding(FMargin(2.f))
					[
						SNew(SImage).Image(GalleryBrushes[Index].Get())
					]
				]
			]
		];
	}
}

void UPauseMenuWidget::StepPhoto(int32 Direction)
{
	UPhotoLibrarySubsystem* Library = GetPhotoLibrary();
	const int32 Num = Library != nullptr ? Library->GetPhotos().Num() : 0;
	if (Num == 0)
	{
		return;
	}

	// Wraps both ways (Prev from the first shot lands on the last, Next from the last wraps to the first).
	CurrentPhotoIndex = ((CurrentPhotoIndex + Direction) % Num + Num) % Num;
	bConfirmingDelete = false;
}

const FSlateBrush* UPauseMenuWidget::GetCurrentBrush() const
{
	return GalleryBrushes.IsValidIndex(CurrentPhotoIndex) ? GalleryBrushes[CurrentPhotoIndex].Get() : nullptr;
}

FOptionalSize UPauseMenuWidget::GetPhotoWidthOverride() const
{
	const FSlateBrush* Brush = GetCurrentBrush();
	if (Brush == nullptr)
	{
		return MaxPhotoSize.X;
	}
	return ComputeAspectFitSize(Brush->ImageSize.X, Brush->ImageSize.Y, MaxPhotoSize.X, MaxPhotoSize.Y).X;
}

FOptionalSize UPauseMenuWidget::GetPhotoHeightOverride() const
{
	const FSlateBrush* Brush = GetCurrentBrush();
	if (Brush == nullptr)
	{
		return MaxPhotoSize.Y;
	}
	return ComputeAspectFitSize(Brush->ImageSize.X, Brush->ImageSize.Y, MaxPhotoSize.X, MaxPhotoSize.Y).Y;
}

FText UPauseMenuWidget::GetPhotoCounterText() const
{
	UPhotoLibrarySubsystem* Library = GetPhotoLibrary();
	const int32 Num = Library != nullptr ? Library->GetPhotos().Num() : 0;
	if (Num == 0)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentPhotoIndex + 1, Num));
}

FText UPauseMenuWidget::GetPhotoSettingsText() const
{
	UPhotoLibrarySubsystem* Library = GetPhotoLibrary();
	if (Library == nullptr || !Library->GetPhotos().IsValidIndex(CurrentPhotoIndex))
	{
		return FText::GetEmpty();
	}

	const FPhotoMetadata& Metadata = Library->GetPhotos()[CurrentPhotoIndex].Metadata;
	return FText::FromString(FString::Printf(TEXT("%s\nf/%.1f  1/%.0f  ISO %.0f  %.0fmm\n%s"),
		*Metadata.CameraBodyName,
		Metadata.Aperture,
		Metadata.ShutterSpeed,
		Metadata.ISO,
		Metadata.FocalLengthMM,
		*Metadata.Timestamp.ToString(TEXT("%Y-%m-%d  %H:%M"))));
}

FText UPauseMenuWidget::GetFavoriteButtonLabel() const
{
	UPhotoLibrarySubsystem* Library = GetPhotoLibrary();
	const bool bFavorite = Library != nullptr && Library->GetPhotos().IsValidIndex(CurrentPhotoIndex) && Library->GetPhotos()[CurrentPhotoIndex].bFavorite;
	return FText::FromString(bFavorite ? TEXT("Favorited") : TEXT("Favorite"));
}

FSlateColor UPauseMenuWidget::GetFavoriteButtonColor() const
{
	UPhotoLibrarySubsystem* Library = GetPhotoLibrary();
	const bool bFavorite = Library != nullptr && Library->GetPhotos().IsValidIndex(CurrentPhotoIndex) && Library->GetPhotos()[CurrentPhotoIndex].bFavorite;
	return bFavorite ? FSlateColor(FLinearColor(1.f, 0.78f, 0.1f)) : FSlateColor(FLinearColor::White);
}

FText UPauseMenuWidget::GetDeleteButtonLabel() const
{
	return FText::FromString(bConfirmingDelete ? TEXT("Confirm Delete?") : TEXT("Delete"));
}

EVisibility UPauseMenuWidget::GetEmptyLibraryTextVisibility() const
{
	return GalleryBrushes.Num() == 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply UPauseMenuWidget::OnResumeClicked()
{
	OnResumeRequested.ExecuteIfBound();
	return FReply::Handled();
}

FReply UPauseMenuWidget::OnGalleryClicked()
{
	// Opens on the most recent shot, like pressing the review button on a real camera.
	if (UPhotoLibrarySubsystem* Library = GetPhotoLibrary())
	{
		CurrentPhotoIndex = Library->GetPhotos().Num() - 1;
	}
	bConfirmingDelete = false;
	RefreshGallery();

	if (Switcher.IsValid())
	{
		Switcher->SetActiveWidgetIndex(1);
	}
	return FReply::Handled();
}

FReply UPauseMenuWidget::OnQuitClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
	return FReply::Handled();
}

FReply UPauseMenuWidget::OnBackClicked()
{
	if (Switcher.IsValid())
	{
		Switcher->SetActiveWidgetIndex(0);
	}
	return FReply::Handled();
}

FReply UPauseMenuWidget::OnThumbnailClicked(int32 PhotoIndex)
{
	CurrentPhotoIndex = PhotoIndex;
	bConfirmingDelete = false;
	return FReply::Handled();
}

FReply UPauseMenuWidget::OnPrevClicked()
{
	StepPhoto(-1);
	return FReply::Handled();
}

FReply UPauseMenuWidget::OnNextClicked()
{
	StepPhoto(1);
	return FReply::Handled();
}

FReply UPauseMenuWidget::OnFavoriteClicked()
{
	UPhotoLibrarySubsystem* Library = GetPhotoLibrary();
	if (Library != nullptr && Library->GetPhotos().IsValidIndex(CurrentPhotoIndex))
	{
		Library->ToggleFavorite(Library->GetPhotos()[CurrentPhotoIndex].Id);
	}
	return FReply::Handled();
}

FReply UPauseMenuWidget::OnDeleteClicked()
{
	UPhotoLibrarySubsystem* Library = GetPhotoLibrary();
	if (Library == nullptr || !Library->GetPhotos().IsValidIndex(CurrentPhotoIndex))
	{
		return FReply::Handled();
	}

	// First click arms it, second click (while still armed) actually deletes - see bConfirmingDelete.
	if (!bConfirmingDelete)
	{
		bConfirmingDelete = true;
		return FReply::Handled();
	}

	Library->DeletePhoto(Library->GetPhotos()[CurrentPhotoIndex].Id);
	bConfirmingDelete = false;
	RefreshGallery();
	return FReply::Handled();
}

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (Switcher.IsValid() && Switcher->GetActiveWidgetIndex() == 1)
	{
		if (InKeyEvent.GetKey() == EKeys::Left)
		{
			StepPhoto(-1);
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::Right)
		{
			StepPhoto(1);
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
