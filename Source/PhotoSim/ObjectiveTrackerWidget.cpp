// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectiveTrackerWidget.h"
#include "PhotoSubjectComponent.h"
#include "WidgetHelpers.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

void UObjectiveTrackerWidget::SetSubjects(const TArray<TObjectPtr<UPhotoSubjectComponent>>& InSubjects)
{
	Subjects.Reset(InSubjects.Num());
	for (UPhotoSubjectComponent* Subject : InSubjects)
	{
		Subjects.Add(Subject);
	}
}

TSharedRef<SWidget> UObjectiveTrackerWidget::MakeObjectiveRow(TWeakObjectPtr<UPhotoSubjectComponent> Subject) const
{
	return SNew(STextBlock)
		.Text_UObject(this, &UObjectiveTrackerWidget::GetObjectiveRowText, Subject)
		.ColorAndOpacity_UObject(this, &UObjectiveTrackerWidget::GetObjectiveRowColor, Subject)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14));
}

FText UObjectiveTrackerWidget::GetObjectiveRowText(TWeakObjectPtr<UPhotoSubjectComponent> Subject) const
{
	if (!Subject.IsValid())
	{
		return FText::GetEmpty();
	}

	const TCHAR* Bullet = Subject->bIsCaptured ? TEXT("✓") : TEXT("○"); // checkmark / open circle
	return FText::FromString(FString::Printf(TEXT("%s  %s"), Bullet, *Subject->DisplayName.ToString()));
}

FSlateColor UObjectiveTrackerWidget::GetObjectiveRowColor(TWeakObjectPtr<UPhotoSubjectComponent> Subject) const
{
	if (Subject.IsValid() && Subject->bIsCaptured)
	{
		return FSlateColor(FLinearColor(0.4f, 0.9f, 0.5f));
	}
	return FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.85f));
}

TSharedRef<SWidget> UObjectiveTrackerWidget::RebuildWidget()
{
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);

	Rows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("OBJECTIVES")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.5f))
		];

	for (const TWeakObjectPtr<UPhotoSubjectComponent>& Subject : Subjects)
	{
		Rows->AddSlot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				MakeObjectiveRow(Subject)
			];
	}

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f))
		.Padding(FMargin(16.f, 12.f))
		[
			Rows
		];
}
