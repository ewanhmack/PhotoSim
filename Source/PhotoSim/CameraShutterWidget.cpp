// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraShutterWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"

void UCameraShutterWidget::PlayShutter()
{
	ElapsedTime = 0.f;
}

FOptionalSize UCameraShutterWidget::GetBladeHeight() const
{
	float Alpha = 0.f;

	if (ElapsedTime >= 0.f)
	{
		if (ElapsedTime < CloseDuration)
		{
			Alpha = CloseDuration > 0.f ? FMath::Clamp(ElapsedTime / CloseDuration, 0.f, 1.f) : 1.f;
		}
		else if (ElapsedTime < CloseDuration + HoldDuration)
		{
			Alpha = 1.f;
		}
		else
		{
			const float OpenElapsed = ElapsedTime - CloseDuration - HoldDuration;
			const float OpenAlpha = OpenDuration > 0.f ? FMath::Clamp(OpenElapsed / OpenDuration, 0.f, 1.f) : 1.f;
			Alpha = 1.f - OpenAlpha;
		}
	}

	// Each blade covers up to half the screen; together they meet at the centre when Alpha = 1.
	return FOptionalSize(LastSize.Y * 0.5f * Alpha);
}

void UCameraShutterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	LastSize = MyGeometry.GetLocalSize();

	if (ElapsedTime >= 0.f)
	{
		ElapsedTime += InDeltaTime;

		const float TotalDuration = CloseDuration + HoldDuration + OpenDuration;
		if (ElapsedTime >= TotalDuration)
		{
			ElapsedTime = -1.f;
		}
	}
}

TSharedRef<SWidget> UCameraShutterWidget::RebuildWidget()
{
	return SNew(SOverlay)

		+ SOverlay::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Fill)
		[
			SNew(SBox)
			.HeightOverride_UObject(this, &UCameraShutterWidget::GetBladeHeight)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor::Black)
			]
		]

		+ SOverlay::Slot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Fill)
		[
			SNew(SBox)
			.HeightOverride_UObject(this, &UCameraShutterWidget::GetBladeHeight)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor::Black)
			]
		];
}
