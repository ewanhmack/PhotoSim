// Copyright Epic Games, Inc. All Rights Reserved.

#include "EndOfRoundWidget.h"
#include "WidgetHelpers.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

void UEndOfRoundWidget::SetOutcome(EPhotoSimRoundState InOutcome, int32 InObjectivesCompleted, int32 InObjectivesTotal)
{
	Outcome = InOutcome;
	ObjectivesCompleted = InObjectivesCompleted;
	ObjectivesTotal = InObjectivesTotal;
}

TSharedRef<SWidget> UEndOfRoundWidget::RebuildWidget()
{
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 36);
	const FSlateFontInfo SubtitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 16);

	const FLinearColor HeadlineColor = (Outcome == EPhotoSimRoundState::Won)
		? FLinearColor(0.4f, 0.9f, 0.5f)
		: FLinearColor(0.9f, 0.4f, 0.35f);

	TSharedRef<SWidget> RestartButton =
		SNew(SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.OnClicked(FOnClicked::CreateUObject(this, &UEndOfRoundWidget::OnRestartClicked))
		.ContentPadding(FMargin(0.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.1f))
			.Padding(FMargin(32.f, 12.f))
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("PLAY AGAIN")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
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

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 10.f)
			[
				SNew(STextBlock)
				.Text_UObject(this, &UEndOfRoundWidget::GetHeadlineText)
				.Font(TitleFont)
				.ColorAndOpacity(HeadlineColor)
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 32.f)
			[
				SNew(STextBlock)
				.Text_UObject(this, &UEndOfRoundWidget::GetSubtitleText)
				.Font(SubtitleFont)
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.7f))
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				RestartButton
			]
		];
}

FText UEndOfRoundWidget::GetHeadlineText() const
{
	return (Outcome == EPhotoSimRoundState::Won)
		? FText::FromString(TEXT("ALL SUBJECTS CAPTURED"))
		: FText::FromString(TEXT("OUT OF TIME"));
}

FText UEndOfRoundWidget::GetSubtitleText() const
{
	return FText::FromString(FString::Printf(TEXT("%d / %d subjects photographed"), ObjectivesCompleted, ObjectivesTotal));
}

FReply UEndOfRoundWidget::OnRestartClicked()
{
	OnRestartRequested.ExecuteIfBound();
	return FReply::Handled();
}
