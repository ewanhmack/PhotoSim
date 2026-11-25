// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensGridInventoryComponent.h"
#include "InventoryComponent.h"

void ULensGridInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	OnInventoryChanged.AddUObject(this, &ULensGridInventoryComponent::RefreshCarriedWeight);
	RefreshCarriedWeight();
}

void ULensGridInventoryComponent::RefreshCarriedWeight()
{
	AActor* Owner = GetOwner();
	UInventoryComponent* Inventory = Owner != nullptr ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (Inventory == nullptr)
	{
		return;
	}

	float TotalWeightKG = 0.f;
	for (const TPair<FGuid, FLensDefinition>& Entry : OwnedLenses)
	{
		TotalWeightKG += Entry.Value.WeightKG;
	}

	Inventory->RegisterWeight(this, TotalWeightKG);
}

bool ULensGridInventoryComponent::CanPlaceFootprint(int32 X, int32 Y, int32 Width, int32 Height, int32 InGridWidth, int32 InGridHeight, const TArray<bool>& OccupiedCells)
{
	if (X < 0 || Y < 0 || Width <= 0 || Height <= 0 || X + Width > InGridWidth || Y + Height > InGridHeight)
	{
		return false;
	}

	for (int32 CellY = Y; CellY < Y + Height; ++CellY)
	{
		for (int32 CellX = X; CellX < X + Width; ++CellX)
		{
			if (OccupiedCells[CellY * InGridWidth + CellX])
			{
				return false;
			}
		}
	}

	return true;
}

bool ULensGridInventoryComponent::FindFirstFreeSlot(int32 Width, int32 Height, int32 InGridWidth, int32 InGridHeight, const TArray<bool>& OccupiedCells, int32& OutX, int32& OutY)
{
	for (int32 Y = 0; Y <= InGridHeight - Height; ++Y)
	{
		for (int32 X = 0; X <= InGridWidth - Width; ++X)
		{
			if (CanPlaceFootprint(X, Y, Width, Height, InGridWidth, InGridHeight, OccupiedCells))
			{
				OutX = X;
				OutY = Y;
				return true;
			}
		}
	}

	return false;
}

TArray<bool> ULensGridInventoryComponent::BuildOccupancyGrid(const FGuid& ExcludeId) const
{
	TArray<bool> Occupied;
	Occupied.Init(false, GridWidth * GridHeight);

	for (const TPair<FGuid, FIntPoint>& Entry : GridPositions)
	{
		if (Entry.Key == ExcludeId)
		{
			continue;
		}

		const FLensDefinition* Definition = OwnedLenses.Find(Entry.Key);
		if (Definition == nullptr)
		{
			continue;
		}

		for (int32 CellY = Entry.Value.Y; CellY < Entry.Value.Y + Definition->GridHeight; ++CellY)
		{
			for (int32 CellX = Entry.Value.X; CellX < Entry.Value.X + Definition->GridWidth; ++CellX)
			{
				Occupied[CellY * GridWidth + CellX] = true;
			}
		}
	}

	return Occupied;
}

FGuid ULensGridInventoryComponent::AddLens(const FLensDefinition& Definition)
{
	TArray<bool> Occupied = BuildOccupancyGrid(FGuid());

	int32 X, Y;
	if (!FindFirstFreeSlot(Definition.GridWidth, Definition.GridHeight, GridWidth, GridHeight, Occupied, X, Y))
	{
		return FGuid();
	}

	const FGuid NewId = FGuid::NewGuid();
	OwnedLenses.Add(NewId, Definition);
	GridPositions.Add(NewId, FIntPoint(X, Y));

	OnInventoryChanged.Broadcast();
	return NewId;
}

bool ULensGridInventoryComponent::RemoveLens(const FGuid& LensId, FLensDefinition& OutDefinition)
{
	const FLensDefinition* Definition = OwnedLenses.Find(LensId);
	if (Definition == nullptr)
	{
		return false;
	}

	OutDefinition = *Definition;
	ClearPlacement(LensId);
	OwnedLenses.Remove(LensId);

	OnInventoryChanged.Broadcast();
	return true;
}

bool ULensGridInventoryComponent::CanPlaceAt(int32 X, int32 Y, int32 Width, int32 Height, const FGuid& IgnoreId) const
{
	return CanPlaceFootprint(X, Y, Width, Height, GridWidth, GridHeight, BuildOccupancyGrid(IgnoreId));
}

bool ULensGridInventoryComponent::MoveLensToGrid(const FGuid& LensId, int32 X, int32 Y)
{
	const FLensDefinition* Definition = OwnedLenses.Find(LensId);
	if (Definition == nullptr || !CanPlaceAt(X, Y, Definition->GridWidth, Definition->GridHeight, LensId))
	{
		return false;
	}

	const int32 PreviousHotbarSlot = HotbarSlots.IndexOfByKey(LensId);

	ClearPlacement(LensId);
	GridPositions.Add(LensId, FIntPoint(X, Y));

	if (PreviousHotbarSlot != INDEX_NONE)
	{
		BroadcastActiveLensIfSlotAffected(PreviousHotbarSlot);
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool ULensGridInventoryComponent::MoveLensToHotbar(const FGuid& LensId, int32 HotbarSlotIndex)
{
	if (!OwnedLenses.Contains(LensId) || !HotbarSlots.IsValidIndex(HotbarSlotIndex))
	{
		return false;
	}

	if (HotbarSlots[HotbarSlotIndex] == LensId)
	{
		return true;
	}

	const FGuid DisplacedLensId = HotbarSlots[HotbarSlotIndex];
	FIntPoint MoverOldGridPosition(0, 0);
	const bool bMoverWasInGrid = GridPositions.RemoveAndCopyValue(LensId, MoverOldGridPosition);
	const int32 MoverOldHotbarSlot = HotbarSlots.IndexOfByKey(LensId);
	if (MoverOldHotbarSlot != INDEX_NONE)
	{
		HotbarSlots[MoverOldHotbarSlot] = FGuid();
	}

	if (DisplacedLensId.IsValid())
	{
		// Try to put the displaced lens back exactly where the mover came from, if the mover came
		// from the grid and the displaced lens's footprint fits there too; otherwise the first free
		// slot; if neither works the displaced lens is left with no placement (still owned).
		const FLensDefinition* DisplacedDefinition = OwnedLenses.Find(DisplacedLensId);
		bool bPlaced = false;
		if (bMoverWasInGrid && DisplacedDefinition != nullptr
			&& CanPlaceFootprint(MoverOldGridPosition.X, MoverOldGridPosition.Y, DisplacedDefinition->GridWidth, DisplacedDefinition->GridHeight, GridWidth, GridHeight, BuildOccupancyGrid(DisplacedLensId)))
		{
			GridPositions.Add(DisplacedLensId, MoverOldGridPosition);
			bPlaced = true;
		}
		if (!bPlaced)
		{
			PlaceInFirstFreeGridSlot(DisplacedLensId);
		}
	}

	HotbarSlots[HotbarSlotIndex] = LensId;

	BroadcastActiveLensIfSlotAffected(HotbarSlotIndex);
	if (MoverOldHotbarSlot != INDEX_NONE)
	{
		BroadcastActiveLensIfSlotAffected(MoverOldHotbarSlot);
	}

	OnInventoryChanged.Broadcast();
	return true;
}

FGuid ULensGridInventoryComponent::GetHotbarSlot(int32 HotbarSlotIndex) const
{
	return HotbarSlots.IsValidIndex(HotbarSlotIndex) ? HotbarSlots[HotbarSlotIndex] : FGuid();
}

bool ULensGridInventoryComponent::GetGridPosition(const FGuid& LensId, int32& OutX, int32& OutY) const
{
	if (const FIntPoint* Position = GridPositions.Find(LensId))
	{
		OutX = Position->X;
		OutY = Position->Y;
		return true;
	}
	return false;
}

const FLensDefinition* ULensGridInventoryComponent::FindDefinition(const FGuid& LensId) const
{
	return OwnedLenses.Find(LensId);
}

void ULensGridInventoryComponent::GetAllLensIds(TArray<FGuid>& OutIds) const
{
	OwnedLenses.GenerateKeyArray(OutIds);
}

void ULensGridInventoryComponent::SetActiveHotbarSlot(int32 SlotIndex)
{
	if (!HotbarSlots.IsValidIndex(SlotIndex) || SlotIndex == ActiveHotbarSlotIndex)
	{
		return;
	}

	ActiveHotbarSlotIndex = SlotIndex;
	OnActiveLensChanged.Broadcast(HotbarSlots[SlotIndex]);
}

void ULensGridInventoryComponent::ClearPlacement(const FGuid& LensId)
{
	GridPositions.Remove(LensId);
	const int32 HotbarSlotIndex = HotbarSlots.IndexOfByKey(LensId);
	if (HotbarSlotIndex != INDEX_NONE)
	{
		HotbarSlots[HotbarSlotIndex] = FGuid();
		BroadcastActiveLensIfSlotAffected(HotbarSlotIndex);
	}
}

bool ULensGridInventoryComponent::PlaceInFirstFreeGridSlot(const FGuid& LensId)
{
	const FLensDefinition* Definition = OwnedLenses.Find(LensId);
	if (Definition == nullptr)
	{
		return false;
	}

	int32 X, Y;
	if (!FindFirstFreeSlot(Definition->GridWidth, Definition->GridHeight, GridWidth, GridHeight, BuildOccupancyGrid(LensId), X, Y))
	{
		return false;
	}

	GridPositions.Add(LensId, FIntPoint(X, Y));
	return true;
}

void ULensGridInventoryComponent::BroadcastActiveLensIfSlotAffected(int32 AffectedSlotIndex)
{
	if (AffectedSlotIndex == ActiveHotbarSlotIndex)
	{
		OnActiveLensChanged.Broadcast(HotbarSlots[ActiveHotbarSlotIndex]);
	}
}
