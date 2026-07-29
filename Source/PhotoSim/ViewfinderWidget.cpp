// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewfinderWidget.h"
#include "TP_CameraComponent.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
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

	/** An L-shaped corner bracket: a bar along the vertical edge, a bar along the horizontal edge. */
	TSharedRef<SWidget> MakeCornerBracket(EHorizontalAlignment HAlign, EVerticalAlignment VAlign)
	{
		const float ArmLength = 34.f;
		const float Thickness = 3.f;
		const FLinearColor BracketColor = FLinearColor(1.f, 1.f, 1.f, 0.85f);

		return SNew(SBox)
			.WidthOverride(ArmLength)
			.HeightOverride(ArmLength)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign)
				[
					SNew(SBox)
					.HeightOverride(Thickness)
					[
						MakeFlatColorBox(BracketColor)
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign)
				.VAlign(VAlign_Fill)
				[
					SNew(SBox)
					.WidthOverride(Thickness)
					[
						MakeFlatColorBox(BracketColor)
					]
				]
			];
	}

	TSharedRef<SWidget> MakeLetterboxBar(EVerticalAlignment VAlign)
	{
		return SNew(SBox)
			.HeightOverride(24.f)
			.VAlign(VAlign)
			[
				MakeFlatColorBox(FLinearColor::Black)
			];
	}
}

void UViewfinderWidget::SetOwningCamera(UTP_CameraComponent* InCamera)
{
	OwningCamera = InCamera;
}

TSharedRef<SWidget> UViewfinderWidget::MakeRuleOfThirdsGrid() const
{
	const float LineThickness = 1.f;
	const FLinearColor LineColor = FLinearColor(1.f, 1.f, 1.f, 0.35f);

	// Two vertical lines dividing the frame into thirds; FillWidth(1.f) cells are always equal,
	// so this lands exactly on the thirds at any resolution without measuring the viewport.
	TSharedRef<SWidget> VerticalLines =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(LineThickness)[ MakeFlatColorBox(LineColor) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(LineThickness)[ MakeFlatColorBox(LineColor) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f);

	TSharedRef<SWidget> HorizontalLines =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(LineThickness)[ MakeFlatColorBox(LineColor) ]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(LineThickness)[ MakeFlatColorBox(LineColor) ]
		]
		+ SVerticalBox::Slot().FillHeight(1.f);

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			VerticalLines
		]
		+ SOverlay::Slot()
		[
			HorizontalLines
		];
}

FText UViewfinderWidget::GetFocalLengthText() const
{
	const float FocalLength = OwningCamera.IsValid() ? OwningCamera->GetFocalLengthMM() : 0.f;
	return FText::FromString(FString::Printf(TEXT("%dmm"), FMath::RoundToInt(FocalLength)));
}

FText UViewfinderWidget::GetApertureText() const
{
	const float Aperture = OwningCamera.IsValid() ? OwningCamera->GetAperture() : 0.f;
	return FText::FromString(FString::Printf(TEXT("f/%.1f"), Aperture));
}

FText UViewfinderWidget::GetShutterSpeedText() const
{
	const float ShutterSpeed = OwningCamera.IsValid() ? OwningCamera->GetShutterSpeed() : 0.f;
	return FText::FromString(FString::Printf(TEXT("1/%d"), FMath::RoundToInt(ShutterSpeed)));
}

FText UViewfinderWidget::GetISOText() const
{
	const float ISO = OwningCamera.IsValid() ? OwningCamera->GetISO() : 0.f;
	return FText::FromString(FString::Printf(TEXT("ISO %d"), FMath::RoundToInt(ISO)));
}

TSharedRef<SWidget> UViewfinderWidget::MakeExposureReadout() const
{
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	const FLinearColor TextColor = FLinearColor(0.1f, 1.f, 0.2f, 0.95f);

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
		[
			SNew(STextBlock).Text_UObject(this, &UViewfinderWidget::GetApertureText).Font(Font).ColorAndOpacity(TextColor)
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
		[
			SNew(STextBlock).Text_UObject(this, &UViewfinderWidget::GetShutterSpeedText).Font(Font).ColorAndOpacity(TextColor)
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
		[
			SNew(STextBlock).Text_UObject(this, &UViewfinderWidget::GetISOText).Font(Font).ColorAndOpacity(TextColor)
		];
}

TSharedRef<SWidget> UViewfinderWidget::RebuildWidget()
{
	return SNew(SOverlay)

		// Top / bottom viewfinder edge bars
		+ SOverlay::Slot().VAlign(VAlign_Top)[ MakeLetterboxBar(VAlign_Top) ]
		+ SOverlay::Slot().VAlign(VAlign_Bottom)[ MakeLetterboxBar(VAlign_Bottom) ]

		// Rule of thirds composition grid
		+ SOverlay::Slot().Padding(FMargin(0.f, 24.f))
		[
			MakeRuleOfThirdsGrid()
		]

		// Corner AF brackets
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(28.f)[ MakeCornerBracket(HAlign_Left, VAlign_Top) ]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(28.f)[ MakeCornerBracket(HAlign_Right, VAlign_Top) ]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(28.f)[ MakeCornerBracket(HAlign_Left, VAlign_Bottom) ]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(28.f)[ MakeCornerBracket(HAlign_Right, VAlign_Bottom) ]

		// Centre focus reticle
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(2.f)
			.HeightOverride(24.f)
			[
				MakeFlatColorBox(FLinearColor(1.f, 1.f, 1.f, 0.6f))
			]
		]

		// Focal length readout
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(16.f, 0.f, 0.f, 34.f))
		[
			SNew(STextBlock)
			.Text_UObject(this, &UViewfinderWidget::GetFocalLengthText)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			.ColorAndOpacity(FLinearColor(0.1f, 1.f, 0.2f, 0.95f))
		]

		// Aperture / shutter speed / ISO readout - real, controllable values (hold Z/X/C + scroll)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 16.f, 34.f))
		[
			MakeExposureReadout()
		];
}
