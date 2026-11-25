// Copyright Epic Games, Inc. All Rights Reserved.

#include "StaminaWidget.h"
#include "PhotoSimCharacter.h"
#include "StaminaComponent.h"
#include "InventoryComponent.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor HUDGreen = FLinearColor(0.1f, 1.f, 0.2f, 0.95f);
}

void UStaminaWidget::SetOwningCharacter(APhotoSimCharacter* InCharacter)
{
	OwningCharacter = InCharacter;
}

TOptional<float> UStaminaWidget::GetStaminaPercent() const
{
	if (!OwningCharacter.IsValid() || OwningCharacter->GetStaminaComponent() == nullptr)
	{
		return TOptional<float>(0.f);
	}
	return TOptional<float>(OwningCharacter->GetStaminaComponent()->GetStaminaFraction());
}

FSlateColor UStaminaWidget::GetStaminaBarColor() const
{
	return FSlateColor(HUDGreen);
}

FText UStaminaWidget::GetCarriedWeightText() const
{
	const float WeightKG = (OwningCharacter.IsValid() && OwningCharacter->GetInventoryComponent() != nullptr)
		? OwningCharacter->GetInventoryComponent()->GetTotalWeightKG()
		: 0.f;
	return FText::FromString(FString::Printf(TEXT("%.1fkg"), WeightKG));
}

TSharedRef<SWidget> UStaminaWidget::RebuildWidget()
{
	return SNew(SOverlay)

		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(24.f, 0.f, 0.f, 24.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox)
				.WidthOverride(160.f)
				.HeightOverride(10.f)
				[
					SNew(SProgressBar)
					.Style(&FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar"))
					.Percent_UObject(this, &UStaminaWidget::GetStaminaPercent)
					.FillColorAndOpacity_UObject(this, &UStaminaWidget::GetStaminaBarColor)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_UObject(this, &UStaminaWidget::GetCarriedWeightText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
				.ColorAndOpacity(HUDGreen)
			]
		];
}
