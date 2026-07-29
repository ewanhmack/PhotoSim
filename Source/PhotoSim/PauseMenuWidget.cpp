// Copyright Epic Games, Inc. All Rights Reserved.

#include "PauseMenuWidget.h"
#include "PhotoLibrarySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameInstance.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"

namespace
{
	TSharedRef<SWidget> MakeFlatColorBox(const FLinearColor& Color)
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Color);
	}
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

	TSharedRef<SVerticalBox> GalleryPage =
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
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
		]

		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SBox)
			.WidthOverride(560.f)
			.HeightOverride(420.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(GalleryWrapBox, SWrapBox)
					.UseAllottedWidth(true)
					.InnerSlotPadding(FVector2D(6.f, 6.f))
				]
			]
		];

	TSharedRef<SVerticalBox> EnlargedPage =
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
		[
			MakeMenuButton(TEXT("< Back"), FOnClicked::CreateUObject(this, &UPauseMenuWidget::OnBackFromEnlargedClicked))
		]

		+ SVerticalBox::Slot().FillHeight(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(640.f)
			.HeightOverride(360.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.2f))
				.Padding(FMargin(4.f))
				[
					SNew(SImage).Image_UObject(this, &UPauseMenuWidget::GetEnlargedBrush)
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
					GalleryPage
				]

				+ SWidgetSwitcher::Slot()
				[
					EnlargedPage
				]
			]
		];
}

void UPauseMenuWidget::RefreshGallery()
{
	if (!GalleryWrapBox.IsValid())
	{
		return;
	}

	GalleryWrapBox->ClearChildren();
	GalleryBrushes.Empty();

	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	UPhotoLibrarySubsystem* Library = GameInstance != nullptr ? GameInstance->GetSubsystem<UPhotoLibrarySubsystem>() : nullptr;

	const TArray<TObjectPtr<UTextureRenderTarget2D>> EmptyPhotos;
	const TArray<TObjectPtr<UTextureRenderTarget2D>>& Photos = Library != nullptr ? Library->GetPhotos() : EmptyPhotos;

	if (Photos.Num() == 0)
	{
		GalleryWrapBox->AddSlot()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No photos yet.")))
			.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.5f))
		];
		return;
	}

	for (int32 Index = 0; Index < Photos.Num(); ++Index)
	{
		UTextureRenderTarget2D* Photo = Photos[Index];
		if (Photo == nullptr)
		{
			continue;
		}

		TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
		Brush->SetResourceObject(Photo);
		Brush->ImageSize = FVector2D(Photo->SizeX, Photo->SizeY);
		Brush->DrawAs = ESlateBrushDrawType::Image;
		GalleryBrushes.Add(Brush);

		GalleryWrapBox->AddSlot()
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.ContentPadding(FMargin(0.f))
			.OnClicked(FOnClicked::CreateUObject(this, &UPauseMenuWidget::OnThumbnailClicked, Index))
			[
				SNew(SBox)
				.WidthOverride(160.f)
				.HeightOverride(90.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.2f))
					.Padding(FMargin(2.f))
					[
						SNew(SImage).Image(Brush.Get())
					]
				]
			]
		];
	}
}

FReply UPauseMenuWidget::OnResumeClicked()
{
	OnResumeRequested.ExecuteIfBound();
	return FReply::Handled();
}

FReply UPauseMenuWidget::OnGalleryClicked()
{
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
	if (GalleryBrushes.IsValidIndex(PhotoIndex))
	{
		EnlargedBrush = GalleryBrushes[PhotoIndex];
		if (Switcher.IsValid())
		{
			Switcher->SetActiveWidgetIndex(2);
		}
	}
	return FReply::Handled();
}

FReply UPauseMenuWidget::OnBackFromEnlargedClicked()
{
	if (Switcher.IsValid())
	{
		Switcher->SetActiveWidgetIndex(1);
	}
	return FReply::Handled();
}

const FSlateBrush* UPauseMenuWidget::GetEnlargedBrush() const
{
	return EnlargedBrush.IsValid() ? EnlargedBrush.Get() : nullptr;
}
