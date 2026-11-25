// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateStructs.h"
#include "LensGridInventoryWidget.generated.h"

class ULensGridInventoryComponent;
class APhotoSimCharacter;
struct FLensDefinition;

/**
 * Lens grid + hotbar, built entirely in C++ via Slate (same house style as the other HUD
 * widgets). Click-to-pick-up / click-to-place rather than mouse-hold-drag: click a lens to carry
 * it (it follows the cursor via a floating preview), click a cell or hotbar slot to place it
 * there, click outside the panel to drop it in the world. This is deliberately not Slate's
 * UDragDropOperation machinery - that needs a widget-level drag-visual object to show anything
 * while dragging, whereas this just reads CarriedLensId/CursorPosition as plain attributes, the
 * same live-binding idiom every other widget in this codebase already uses, so the preview is
 * both simpler to get right without being able to see it render and closer to how reference grid
 * inventories (e.g. the Tarkov/Diablo-style "pick up, follow, place" pattern) actually behave.
 *
 * Each grid cell and hotbar slot is its own SBorder with a bound OnMouseButtonDown that identifies
 * exactly which cell was pressed via a captured payload (X, Y or slot index) - see
 * HandleCellClicked - rather than reconstructing cell coordinates from raw pixel math.
 */
UCLASS()
class PHOTOSIM_API ULensGridInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningCharacter(APhotoSimCharacter* InCharacter);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	static constexpr float CellSize = 30.f;
	static constexpr float HotbarSlotSize = 60.f;
	static constexpr float HotbarSlotPadding = 6.f;

	TSharedRef<SWidget> MakeGrid();
	TSharedRef<SWidget> MakeHotbar();
	TSharedRef<SWidget> MakeGridCell(int32 X, int32 Y);
	TSharedRef<SWidget> MakeHotbarSlot(int32 SlotIndex);
	TSharedRef<SWidget> MakeDraggedPreview();

	/** Picks up the lens at (X,Y) if nothing's carried yet; places whatever's carried there if something is. */
	FReply HandleCellClicked(const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 X, int32 Y);
	/** Picks up the lens in HotbarSlotIndex if nothing's carried yet; places whatever's carried into that slot if something is. */
	FReply HandleHotbarSlotClicked(const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 SlotIndex);
	/** Drops whatever's carried into the world. Bound to the scrim behind the panel, so clicking outside the panel while carrying something is "drop it". */
	FReply HandleScrimClicked(const FGeometry& Geometry, const FPointerEvent& PointerEvent);

	/** The lens anchored at (or covering) (X,Y) - excludes whatever's currently carried, since that's visually "picked up" off the grid. Invalid Guid if empty. */
	FGuid GetLensAt(int32 X, int32 Y) const;
	/** True (and sets OutLensId) only if (X,Y) is specifically a lens's top-left anchor cell, not just any cell it covers - used to only draw the label once per tile */
	bool IsAnchorCell(int32 X, int32 Y, FGuid& OutLensId) const;

	FSlateColor GetCellColor(int32 X, int32 Y) const;
	FText GetCellLabel(int32 X, int32 Y) const;
	/** Full lens name, for a tooltip - the on-cell label itself is too compact to be legible as more than a number */
	FText GetCellTooltip(int32 X, int32 Y) const;

	FSlateColor GetHotbarSlotColor(int32 SlotIndex) const;
	FText GetHotbarSlotLabel(int32 SlotIndex) const;

	FText MakeShortLabel(const FLensDefinition& Lens) const;
	/** Even more compact than MakeShortLabel, for the tiny grid cells specifically - see its own comment */
	FText MakeGridCellLabel(const FLensDefinition& Lens) const;

	EVisibility GetDraggedPreviewVisibility() const;
	FText GetDraggedPreviewText() const;
	FMargin GetDraggedPreviewOffset() const;
	FOptionalSize GetDraggedPreviewWidth() const;
	FOptionalSize GetDraggedPreviewHeight() const;

	/** Carries LensId (toggling it off the grid/hotbar visually) if nothing's carried yet. No-op if something's already carried. */
	void TryPickUp(const FGuid& LensId);
	/** Removes CarriedLensId from the inventory and spawns an ALensPickup at the owning character's feet, then clears the carry. */
	void DropCarriedLensIntoWorld();

	TWeakObjectPtr<APhotoSimCharacter> OwningCharacter;
	TWeakObjectPtr<ULensGridInventoryComponent> LensInventory;

	FGuid CarriedLensId;
	/** Local-space position within this widget's own root geometry, updated every NativeOnMouseMove */
	FVector2D CursorPosition = FVector2D::ZeroVector;
};
