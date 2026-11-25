// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LensDefinition.h"
#include "LensGridInventoryComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnLensInventoryChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActiveLensChanged, FGuid /*LensId, invalid if the active hotbar slot is empty*/);

/**
 * Every lens the player owns lives here, not on any specific camera - picking up a different
 * camera body doesn't lose your lenses. Each owned lens sits in exactly one place: a cell anchor
 * in the GridWidth x GridHeight grid, or one of NumHotbarSlots hotbar slots. The hotbar is the
 * equip point - UTP_CameraComponent listens for OnActiveLensChanged and applies whichever lens is
 * in the currently active hotbar slot to its live dials.
 *
 * Lenses are identified by FGuid rather than array index, since index would silently go stale the
 * moment anything else moves - a drag-and-drop UI is constantly moving things.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PHOTOSIM_API ULensGridInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 GridWidth = 16;
	static constexpr int32 GridHeight = 16;
	static constexpr int32 NumHotbarSlots = 3;

	/** Adds a new owned lens, auto-placed in the first free grid slot (row-major). Returns its new Id, or an invalid Guid if it doesn't fit anywhere. */
	FGuid AddLens(const FLensDefinition& Definition);

	/** Removes a lens entirely, from wherever it currently is. Returns false (OutDefinition untouched) if Id isn't owned. */
	bool RemoveLens(const FGuid& LensId, FLensDefinition& OutDefinition);

	/** True if a Width x Height footprint at (X,Y) is in bounds and doesn't overlap any owned lens other than IgnoreId. */
	bool CanPlaceAt(int32 X, int32 Y, int32 Width, int32 Height, const FGuid& IgnoreId) const;

	/** Moves an owned lens (from the grid or a hotbar slot) to a specific grid anchor. False (nothing moved) if it doesn't fit there. */
	bool MoveLensToGrid(const FGuid& LensId, int32 X, int32 Y);

	/**
	 * Moves an owned lens (from the grid or another hotbar slot) into HotbarSlotIndex. Whatever was
	 * already in that slot is displaced to the mover's old grid position if it fits there, else the
	 * first free grid slot, else the swap is rejected outright and nothing moves.
	 */
	bool MoveLensToHotbar(const FGuid& LensId, int32 HotbarSlotIndex);

	/** Lens Id in HotbarSlotIndex, or an invalid Guid if that slot is empty or the index is out of range. */
	FGuid GetHotbarSlot(int32 HotbarSlotIndex) const;

	/** Grid anchor of LensId. False if LensId isn't currently in the grid (e.g. it's in the hotbar, or unowned). */
	bool GetGridPosition(const FGuid& LensId, int32& OutX, int32& OutY) const;

	const FLensDefinition* FindDefinition(const FGuid& LensId) const;

	/** Every owned lens Id, grid and hotbar both. */
	void GetAllLensIds(TArray<FGuid>& OutIds) const;

	/** Switches which hotbar slot is treated as equipped and broadcasts OnActiveLensChanged - an empty slot broadcasts an invalid Guid. */
	UFUNCTION(BlueprintCallable, Category = "Lens Inventory")
	void SetActiveHotbarSlot(int32 SlotIndex);

	int32 GetActiveHotbarSlotIndex() const { return ActiveHotbarSlotIndex; }

	/** Fires after any add/remove/move - UI refresh hook. */
	FOnLensInventoryChanged OnInventoryChanged;

	/** Fires when the active hotbar slot's occupant changes, including via SetActiveHotbarSlot itself landing on a different lens. */
	FOnActiveLensChanged OnActiveLensChanged;

	/** True if LocalPoint, a 1x1 grid cell occupancy row-major array, has every cell of a Width x Height rectangle anchored at (X,Y) clear. Pure so the packing logic is unit-testable without a live component. */
	static bool CanPlaceFootprint(int32 X, int32 Y, int32 Width, int32 Height, int32 InGridWidth, int32 InGridHeight, const TArray<bool>& OccupiedCells);

	/** Scans row-major for the first (X,Y) a Width x Height footprint fits per CanPlaceFootprint. False if nothing fits anywhere. */
	static bool FindFirstFreeSlot(int32 Width, int32 Height, int32 InGridWidth, int32 InGridHeight, const TArray<bool>& OccupiedCells, int32& OutX, int32& OutY);

protected:
	virtual void BeginPlay() override;

private:
	/** Sums every owned lens's weight and registers it with the owner's UInventoryComponent under this component - a separate weight source from the camera body's own, so the bag's weight persists independent of whichever camera (if any) is held. */
	void RefreshCarriedWeight();

	TMap<FGuid, FLensDefinition> OwnedLenses;
	TMap<FGuid, FIntPoint> GridPositions;
	TArray<FGuid> HotbarSlots{FGuid(), FGuid(), FGuid()};
	int32 ActiveHotbarSlotIndex = 0;

	TArray<bool> BuildOccupancyGrid(const FGuid& ExcludeId) const;

	/** Removes LensId from wherever it currently sits (grid or hotbar) without removing it from OwnedLenses - an internal step shared by the public move/remove functions. */
	void ClearPlacement(const FGuid& LensId);

	/** Finds a lens's first free grid slot and places it there if one exists; otherwise it's left with no placement at all (still owned, just nowhere visible - callers are expected to only hit this on a genuinely full inventory). Returns true if it found a spot. */
	bool PlaceInFirstFreeGridSlot(const FGuid& LensId);

	void BroadcastActiveLensIfSlotAffected(int32 AffectedSlotIndex);
};
