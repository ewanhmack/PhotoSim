// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExposureModeToastWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

void UExposureModeToastWidget::ShowMode(const FText& ModeText)
{
	CurrentModeText = ModeText;
	ElapsedTime = 0.f;
}

FSlateColor UExposureModeToastWidget::GetTextColor() const
{
	float Alpha = 0.f;

	if (ElapsedTime >= 0.f)
	{
		if (ElapsedTime < HoldDuration)
		{
			Alpha = 1.f;
		}
		else
		{
			const float FadeElapsed = ElapsedTime - HoldDuration;
			Alpha = FadeDuration > 0.f ? 1.f - FMath::Clamp(FadeElapsed / FadeDuration, 0.f, 1.f) : 0.f;
		}
	}

	return FSlateColor(FLinearColor(0.1f, 1.f, 0.2f, 0.95f * Alpha));
}

void UExposureModeToastWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ElapsedTime >= 0.f)
	{
		ElapsedTime += InDeltaTime;

		const float TotalDuration = HoldDuration + FadeDuration;
		if (ElapsedTime >= TotalDuration)
		{
			ElapsedTime = -1.f;
		}
	}
}

TSharedRef<SWidget> UExposureModeToastWidget::RebuildWidget()
{
	return SNew(SOverlay)

		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 48.f, 0.f))
		[
			SNew(STextBlock)
			.Font(FCoreStyle::Get().GetFontStyle("NormalFontBold"))
			.ColorAndOpacity_UObject(this, &UExposureModeToastWidget::GetTextColor)
			.Text_UObject(this, &UExposureModeToastWidget::GetModeText)
		];
}
