// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensGridInventoryWidget.h"
#include "LensGridInventoryComponent.h"
#include "LensPickup.h"
#include "PhotoSimCharacter.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"

void ULensGridInventoryWidget::SetOwningCharacter(APhotoSimCharacter* InCharacter)
{
	OwningCharacter = InCharacter;
	LensInventory = InCharacter != nullptr ? InCharacter->GetLensInventoryComponent() : nullptr;
}

FGuid ULensGridInventoryWidget::GetLensAt(int32 X, int32 Y) const
{
	if (!LensInventory.IsValid())
	{
		return FGuid();
	}

	TArray<FGuid> Ids;
	LensInventory->GetAllLensIds(Ids);
	for (const FGuid& Id : Ids)
	{
		if (CarriedLensId.IsValid() && Id == CarriedLensId)
		{
			continue; // visually picked up - doesn't occupy its old cell while carried
		}

		int32 LensX, LensY;
		const FLensDefinition* Definition = LensInventory->FindDefinition(Id);
		if (Definition == nullptr || !LensInventory->GetGridPosition(Id, LensX, LensY))
		{
			continue;
		}

		if (X >= LensX && X < LensX + Definition->GridWidth && Y >= LensY && Y < LensY + Definition->GridHeight)
		{
			return Id;
		}
	}

	return FGuid();
}

bool ULensGridInventoryWidget::IsAnchorCell(int32 X, int32 Y, FGuid& OutLensId) const
{
	const FGuid LensId = GetLensAt(X, Y);
	int32 LensX, LensY;
	if (!LensId.IsValid() || !LensInventory.IsValid() || !LensInventory->GetGridPosition(LensId, LensX, LensY))
	{
		return false;
	}

	if (LensX == X && LensY == Y)
	{
		OutLensId = LensId;
		return true;
	}

	return false;
}

FSlateColor ULensGridInventoryWidget::GetCellColor(int32 X, int32 Y) const
{
	FGuid AnchorId;
	if (IsAnchorCell(X, Y, AnchorId))
	{
		return FSlateColor(FLinearColor(0.45f, 0.32f, 0.16f, 0.75f));
	}
	if (GetLensAt(X, Y).IsValid())
	{
		return FSlateColor(FLinearColor(0.45f, 0.32f, 0.16f, 0.5f));
	}
	return FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.05f));
}

FText ULensGridInventoryWidget::GetCellLabel(int32 X, int32 Y) const
{
	FGuid AnchorId;
	if (!IsAnchorCell(X, Y, AnchorId) || !LensInventory.IsValid())
	{
		return FText::GetEmpty();
	}

	const FLensDefinition* Definition = LensInventory->FindDefinition(AnchorId);
	return Definition != nullptr ? MakeGridCellLabel(*Definition) : FText::GetEmpty();
}

FSlateColor ULensGridInventoryWidget::GetHotbarSlotColor(int32 SlotIndex) const
{
	if (!LensInventory.IsValid())
	{
		return FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.08f));
	}

	const FGuid LensId = LensInventory->GetHotbarSlot(SlotIndex);
	const bool bIsEmptyOrCarried = !LensId.IsValid() || (CarriedLensId.IsValid() && LensId == CarriedLensId);
	const bool bIsActive = LensInventory->GetActiveHotbarSlotIndex() == SlotIndex;

	if (!bIsEmptyOrCarried)
	{
		return FSlateColor(bIsActive ? FLinearColor(0.35f, 0.75f, 0.45f, 0.65f) : FLinearColor(0.45f, 0.32f, 0.16f, 0.6f));
	}
	return FSlateColor(bIsActive ? FLinearColor(0.35f, 0.75f, 0.45f, 0.25f) : FLinearColor(1.f, 1.f, 1.f, 0.08f));
}

FText ULensGridInventoryWidget::GetHotbarSlotLabel(int32 SlotIndex) const
{
	if (!LensInventory.IsValid())
	{
		return FText::GetEmpty();
	}

	const FGuid LensId = LensInventory->GetHotbarSlot(SlotIndex);
	const FLensDefinition* Definition = (CarriedLensId.IsValid() && LensId == CarriedLensId) ? nullptr : LensInventory->FindDefinition(LensId);
	return Definition != nullptr ? MakeShortLabel(*Definition) : FText::FromString(FString::Printf(TEXT("%d"), SlotIndex + 1));
}

FText ULensGridInventoryWidget::MakeShortLabel(const FLensDefinition& Lens) const
{
	if (Lens.IsPrime())
	{
		return FText::FromString(FString::Printf(TEXT("%dmm"), FMath::RoundToInt(Lens.MaxFocalLengthMM)));
	}
	return FText::FromString(FString::Printf(TEXT("%d-%d"), FMath::RoundToInt(Lens.MinFocalLengthMM), FMath::RoundToInt(Lens.MaxFocalLengthMM)));
}

FText ULensGridInventoryWidget::MakeGridCellLabel(const FLensDefinition& Lens) const
{
	// Grid cells are too small (CellSize px) for "mm"/dashes to fit legibly even at the smallest
	// readable font - just the number(s), full name is in the tooltip instead.
	if (Lens.IsPrime())
	{
		return FText::AsNumber(FMath::RoundToInt(Lens.MaxFocalLengthMM));
	}
	return FText::FromString(FString::Printf(TEXT("%d/%d"), FMath::RoundToInt(Lens.MinFocalLengthMM), FMath::RoundToInt(Lens.MaxFocalLengthMM)));
}

FText ULensGridInventoryWidget::GetCellTooltip(int32 X, int32 Y) const
{
	FGuid AnchorId;
	if (!IsAnchorCell(X, Y, AnchorId) || !LensInventory.IsValid())
	{
		return FText::GetEmpty();
	}
	const FLensDefinition* Definition = LensInventory->FindDefinition(AnchorId);
	return Definition != nullptr ? Definition->Name : FText::GetEmpty();
}

void ULensGridInventoryWidget::TryPickUp(const FGuid& LensId)
{
	if (!CarriedLensId.IsValid() && LensId.IsValid())
	{
		CarriedLensId = LensId;
	}
}

void ULensGridInventoryWidget::DropCarriedLensIntoWorld()
{
	if (!LensInventory.IsValid() || !CarriedLensId.IsValid())
	{
		return;
	}

	FLensDefinition RemovedDefinition;
	const bool bRemoved = LensInventory->RemoveLens(CarriedLensId, RemovedDefinition);
	CarriedLensId = FGuid();

	if (!bRemoved || !OwningCharacter.IsValid())
	{
		return;
	}

	UWorld* World = OwningCharacter->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FVector SpawnLocation = OwningCharacter->GetActorLocation() + OwningCharacter->GetActorForwardVector() * 100.f;
	if (ALensPickup* Pickup = World->SpawnActor<ALensPickup>(SpawnLocation, FRotator::ZeroRotator))
	{
		Pickup->LensDefinition = RemovedDefinition;
	}
}

FReply ULensGridInventoryWidget::HandleCellClicked(const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 X, int32 Y)
{
	if (!PointerEvent.IsMouseButtonDown(EKeys::LeftMouseButton) || !LensInventory.IsValid())
	{
		return FReply::Unhandled();
	}

	if (CarriedLensId.IsValid())
	{
		if (LensInventory->MoveLensToGrid(CarriedLensId, X, Y))
		{
			CarriedLensId = FGuid();
		}
		// Failed placement (e.g. overlaps another lens) keeps carrying so the player can try elsewhere.
		return FReply::Handled();
	}

	TryPickUp(GetLensAt(X, Y));
	return FReply::Handled();
}

FReply ULensGridInventoryWidget::HandleHotbarSlotClicked(const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 SlotIndex)
{
	if (!PointerEvent.IsMouseButtonDown(EKeys::LeftMouseButton) || !LensInventory.IsValid())
	{
		return FReply::Unhandled();
	}

	if (CarriedLensId.IsValid())
	{
		if (LensInventory->MoveLensToHotbar(CarriedLensId, SlotIndex))
		{
			CarriedLensId = FGuid();
		}
		return FReply::Handled();
	}

	TryPickUp(LensInventory->GetHotbarSlot(SlotIndex));
	return FReply::Handled();
}

FReply ULensGridInventoryWidget::HandleScrimClicked(const FGeometry& Geometry, const FPointerEvent& PointerEvent)
{
	if (!PointerEvent.IsMouseButtonDown(EKeys::LeftMouseButton) || !CarriedLensId.IsValid())
	{
		return FReply::Unhandled();
	}

	DropCarriedLensIntoWorld();
	return FReply::Handled();
}

FReply ULensGridInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	CursorPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

EVisibility ULensGridInventoryWidget::GetDraggedPreviewVisibility() const
{
	return CarriedLensId.IsValid() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FText ULensGridInventoryWidget::GetDraggedPreviewText() const
{
	if (!LensInventory.IsValid())
	{
		return FText::GetEmpty();
	}
	const FLensDefinition* Definition = LensInventory->FindDefinition(CarriedLensId);
	return Definition != nullptr ? MakeShortLabel(*Definition) : FText::GetEmpty();
}

FMargin ULensGridInventoryWidget::GetDraggedPreviewOffset() const
{
	// The slot's default Alignment (0.5, 0.5) centres its content on this offset point for us -
	// no manual half-width/half-height math needed here.
	return FMargin(CursorPosition.X, CursorPosition.Y, 0.f, 0.f);
}

FOptionalSize ULensGridInventoryWidget::GetDraggedPreviewWidth() const
{
	const FLensDefinition* Definition = LensInventory.IsValid() ? LensInventory->FindDefinition(CarriedLensId) : nullptr;
	return Definition != nullptr ? CellSize * Definition->GridWidth : CellSize;
}

FOptionalSize ULensGridInventoryWidget::GetDraggedPreviewHeight() const
{
	const FLensDefinition* Definition = LensInventory.IsValid() ? LensInventory->FindDefinition(CarriedLensId) : nullptr;
	return Definition != nullptr ? CellSize * Definition->GridHeight : CellSize;
}

TSharedRef<SWidget> ULensGridInventoryWidget::MakeDraggedPreview()
{
	return SNew(SBox)
		.WidthOverride_UObject(this, &ULensGridInventoryWidget::GetDraggedPreviewWidth)
		.HeightOverride_UObject(this, &ULensGridInventoryWidget::GetDraggedPreviewHeight)
		.Visibility_UObject(this, &ULensGridInventoryWidget::GetDraggedPreviewVisibility)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.9f, 0.7f, 0.2f, 0.7f))
			.Padding(FMargin(1.f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_UObject(this, &ULensGridInventoryWidget::GetDraggedPreviewText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FLinearColor::White)
				.Justification(ETextJustify::Center)
			]
		];
}

TSharedRef<SWidget> ULensGridInventoryWidget::MakeGridCell(int32 X, int32 Y)
{
	return SNew(SBox)
		.WidthOverride(CellSize)
		.HeightOverride(CellSize)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor_UObject(this, &ULensGridInventoryWidget::GetCellColor, X, Y)
			.OnMouseButtonDown(FPointerEventHandler::CreateUObject(this, &ULensGridInventoryWidget::HandleCellClicked, X, Y))
			.ToolTipText_UObject(this, &ULensGridInventoryWidget::GetCellTooltip, X, Y)
			// Without this, a label wider than the cell (a 2-3 digit focal length still doesn't
			// always fit a CellSize-px cell) paints past its own border into neighbouring cells
			// instead of being cut off - exactly what showed up as one unbroken strip of text
			// across the top of the grid.
			.Clipping(EWidgetClipping::ClipToBounds)
			.Padding(FMargin(1.f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_UObject(this, &ULensGridInventoryWidget::GetCellLabel, X, Y)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
				.ColorAndOpacity(FLinearColor::White)
				.Justification(ETextJustify::Center)
			]
		];
}

TSharedRef<SWidget> ULensGridInventoryWidget::MakeGrid()
{
	TSharedRef<SGridPanel> Grid = SNew(SGridPanel);
	for (int32 Y = 0; Y < ULensGridInventoryComponent::GridHeight; ++Y)
	{
		for (int32 X = 0; X < ULensGridInventoryComponent::GridWidth; ++X)
		{
			Grid->AddSlot(X, Y)
			[
				MakeGridCell(X, Y)
			];
		}
	}

	return SNew(SBox)
		.WidthOverride(CellSize * ULensGridInventoryComponent::GridWidth)
		.HeightOverride(CellSize * ULensGridInventoryComponent::GridHeight)
		[
			Grid
		];
}

TSharedRef<SWidget> ULensGridInventoryWidget::MakeHotbarSlot(int32 SlotIndex)
{
	return SNew(SBox)
		.WidthOverride(HotbarSlotSize)
		.HeightOverride(HotbarSlotSize)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor_UObject(this, &ULensGridInventoryWidget::GetHotbarSlotColor, SlotIndex)
			.OnMouseButtonDown(FPointerEventHandler::CreateUObject(this, &ULensGridInventoryWidget::HandleHotbarSlotClicked, SlotIndex))
			.Padding(FMargin(4.f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_UObject(this, &ULensGridInventoryWidget::GetHotbarSlotLabel, SlotIndex)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				.ColorAndOpacity(FLinearColor::White)
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
			]
		];
}

TSharedRef<SWidget> ULensGridInventoryWidget::MakeHotbar()
{
	TSharedRef<SHorizontalBox> Hotbar = SNew(SHorizontalBox);
	for (int32 SlotIndex = 0; SlotIndex < ULensGridInventoryComponent::NumHotbarSlots; ++SlotIndex)
	{
		Hotbar->AddSlot()
			.AutoWidth()
			.Padding(HotbarSlotPadding * 0.5f)
			[
				MakeHotbarSlot(SlotIndex)
			];
	}
	return Hotbar;
}

TSharedRef<SWidget> ULensGridInventoryWidget::RebuildWidget()
{
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 24);

	TSharedRef<SWidget> Content = SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.6f))
			.OnMouseButtonDown(FPointerEventHandler::CreateUObject(this, &ULensGridInventoryWidget::HandleScrimClicked))
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			// Warm dark brown rather than flat black - closer to a game-inventory panel.
			.BorderBackgroundColor(FLinearColor(0.08f, 0.06f, 0.04f, 0.97f))
			.Padding(FMargin(28.f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("LENSES")))
					.Font(TitleFont)
					.ColorAndOpacity(FLinearColor::White)
				]

				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 16.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Click to pick up, click again to place. Click outside the panel to drop it in the world.")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
					.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.55f))
				]

				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 16.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.08f))
					.Padding(FMargin(6.f))
					[
						MakeGrid()
					]
				]

				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					MakeHotbar()
				]
			]
		];

	return SNew(SConstraintCanvas)

		+ SConstraintCanvas::Slot()
		.Anchors(FAnchors(0.f, 0.f, 1.f, 1.f))
		.Offset(FMargin(0.f))
		[
			Content
		]

		+ SConstraintCanvas::Slot()
		.AutoSize(true)
		.Anchors(FAnchors(0.f, 0.f))
		.Offset_UObject(this, &ULensGridInventoryWidget::GetDraggedPreviewOffset)
		[
			MakeDraggedPreview()
		];
}
