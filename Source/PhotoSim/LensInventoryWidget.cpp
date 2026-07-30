// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensInventoryWidget.h"
#include "TP_CameraComponent.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace
{
	TSharedRef<SWidget> MakeFlatColorBox(const FLinearColor& Color)
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Color);
	}
}

void ULensInventoryWidget::SetOwningCamera(UTP_CameraComponent* InCamera)
{
	OwningCamera = InCamera;
}

TSharedRef<SWidget> ULensInventoryWidget::MakeLensRow(int32 LensIndex)
{
	if (!OwningCamera.IsValid() || !OwningCamera->AvailableLenses.IsValidIndex(LensIndex))
	{
		return SNullWidget::NullWidget;
	}

	const FLensDefinition& Lens = OwningCamera->AvailableLenses[LensIndex];
	const bool bEquipped = OwningCamera->EquippedLensIndex == LensIndex;

	const FString FocalRange = FMath::IsNearlyEqual(Lens.MinFocalLengthMM, Lens.MaxFocalLengthMM)
		? FString::Printf(TEXT("%dmm"), FMath::RoundToInt(Lens.MaxFocalLengthMM))
		: FString::Printf(TEXT("%d-%dmm"), FMath::RoundToInt(Lens.MinFocalLengthMM), FMath::RoundToInt(Lens.MaxFocalLengthMM));

	const FString ApertureRange = FString::Printf(TEXT("f/%.1f-f/%.0f"), Lens.MinAperture, Lens.MaxAperture);

	const FString Label = FString::Printf(TEXT("%s     %s     %s%s"),
		*Lens.Name.ToString(), *FocalRange, *ApertureRange, bEquipped ? TEXT("     [Equipped]") : TEXT(""));

	return SNew(SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.ContentPadding(FMargin(0.f))
		.OnClicked(FOnClicked::CreateUObject(this, &ULensInventoryWidget::OnLensClicked, LensIndex))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, bEquipped ? 0.18f : 0.08f))
			.Padding(FMargin(24.f, 10.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
				.ColorAndOpacity(FLinearColor::White)
			]
		];
}

FReply ULensInventoryWidget::OnLensClicked(int32 LensIndex)
{
	if (OwningCamera.IsValid())
	{
		OwningCamera->EquipLens(LensIndex);
		OwningCamera->ToggleLensInventory();
	}
	return FReply::Handled();
}

TSharedRef<SWidget> ULensInventoryWidget::RebuildWidget()
{
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 28);

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);

	const int32 LensCount = OwningCamera.IsValid() ? OwningCamera->AvailableLenses.Num() : 0;
	for (int32 Index = 0; Index < LensCount; ++Index)
	{
		Rows->AddSlot()
			.AutoHeight()
			.Padding(0.f, 4.f)
			[
				MakeLensRow(Index)
			];
	}

	return SNew(SOverlay)

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
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 20.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("LENSES")))
					.Font(TitleFont)
					.ColorAndOpacity(FLinearColor::White)
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					Rows
				]
			]
		];
}
