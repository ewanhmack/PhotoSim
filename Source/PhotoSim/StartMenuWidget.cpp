// Copyright Epic Games, Inc. All Rights Reserved.

#include "StartMenuWidget.h"
#include "WidgetHelpers.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UStartMenuWidget::RebuildWidget()
{
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 40);
	const FSlateFontInfo SubtitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 16);

	TSharedRef<SWidget> StartButton =
		SNew(SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.OnClicked(FOnClicked::CreateUObject(this, &UStartMenuWidget::OnStartClicked))
		.ContentPadding(FMargin(0.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.1f))
			.Padding(FMargin(40.f, 14.f))
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("START")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
				.ColorAndOpacity(FLinearColor::White)
			]
		];

	return SNew(SOverlay)

		+ SOverlay::Slot()
		[
			MakeFlatColorBox(FLinearColor(0.f, 0.f, 0.f, 0.85f))
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 14.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("PHOTOSIM")))
				.Font(TitleFont)
				.ColorAndOpacity(FLinearColor::White)
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 36.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Photograph every subject in the village before time runs out.")))
				.Font(SubtitleFont)
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.7f))
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				StartButton
			]
		];
}

FReply UStartMenuWidget::OnStartClicked()
{
	OnStartRequested.ExecuteIfBound();
	return FReply::Handled();
}
