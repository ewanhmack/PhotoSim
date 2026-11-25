// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "LensGridInventoryComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	FLensDefinition MakeLens(int32 Width, int32 Height, const TCHAR* Name = TEXT("Test Lens"))
	{
		FLensDefinition Lens;
		Lens.Name = FText::FromString(Name);
		Lens.GridWidth = Width;
		Lens.GridHeight = Height;
		return Lens;
	}
}

BEGIN_DEFINE_SPEC(FLensGridInventorySpec, "PhotoSim", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UWorld* TestWorld = nullptr;
	AActor* TestActor = nullptr;
	ULensGridInventoryComponent* Inventory = nullptr;

	TUniquePtr<FScopedAllowAbstractClassAllocation> AbstractAllocationGuard;
END_DEFINE_SPEC(FLensGridInventorySpec)

void FLensGridInventorySpec::Define()
{
	BeforeEach([this]()
	{
		AbstractAllocationGuard = MakeUnique<FScopedAllowAbstractClassAllocation>();

		TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
		TestActor = TestWorld->SpawnActor<AActor>();
		Inventory = NewObject<ULensGridInventoryComponent>(TestActor);
		Inventory->RegisterComponent();
	});

	AfterEach([this]()
	{
		if (TestWorld != nullptr)
		{
			TestWorld->DestroyWorld(false);
			TestWorld = nullptr;
		}
		TestActor = nullptr;
		Inventory = nullptr;
		AbstractAllocationGuard.Reset();
	});

	Describe("CanPlaceFootprint (pure)", [this]()
	{
		It("AcceptsAnInBoundsEmptyRegion", [this]()
		{
			TArray<bool> Occupied;
			Occupied.Init(false, 16 * 16);
			TestTrue(TEXT("2x1 fits at the origin of an empty grid"), ULensGridInventoryComponent::CanPlaceFootprint(0, 0, 2, 1, 16, 16, Occupied));
		});

		It("RejectsRunningOffTheRightOrBottomEdge", [this]()
		{
			TArray<bool> Occupied;
			Occupied.Init(false, 16 * 16);
			TestFalse(TEXT("2 wide doesn't fit with only 1 column left"), ULensGridInventoryComponent::CanPlaceFootprint(15, 0, 2, 1, 16, 16, Occupied));
			TestFalse(TEXT("2 tall doesn't fit with only 1 row left"), ULensGridInventoryComponent::CanPlaceFootprint(0, 15, 1, 2, 16, 16, Occupied));
		});

		It("RejectsNegativeOrigin", [this]()
		{
			TArray<bool> Occupied;
			Occupied.Init(false, 16 * 16);
			TestFalse(TEXT("Negative X is out of bounds"), ULensGridInventoryComponent::CanPlaceFootprint(-1, 0, 1, 1, 16, 16, Occupied));
		});

		It("RejectsOverlapWithAnOccupiedCell", [this]()
		{
			TArray<bool> Occupied;
			Occupied.Init(false, 16 * 16);
			Occupied[1 * 16 + 1] = true; // (X=1, Y=1)
			TestFalse(TEXT("A 2x2 footprint at (0,0) covers the occupied (1,1) cell"), ULensGridInventoryComponent::CanPlaceFootprint(0, 0, 2, 2, 16, 16, Occupied));
			TestTrue(TEXT("A 1x1 at (0,0) alone doesn't touch (1,1)"), ULensGridInventoryComponent::CanPlaceFootprint(0, 0, 1, 1, 16, 16, Occupied));
		});
	});

	Describe("FindFirstFreeSlot (pure)", [this]()
	{
		It("FindsTheOriginOnAnEmptyGrid", [this]()
		{
			TArray<bool> Occupied;
			Occupied.Init(false, 16 * 16);
			int32 X, Y;
			TestTrue(TEXT("Finds a slot"), ULensGridInventoryComponent::FindFirstFreeSlot(1, 1, 16, 16, Occupied, X, Y));
			TestEqual(TEXT("At the origin"), X, 0);
			TestEqual(TEXT("At the origin"), Y, 0);
		});

		It("SkipsOccupiedCellsRowMajor", [this]()
		{
			TArray<bool> Occupied;
			Occupied.Init(false, 16 * 16);
			Occupied[0] = true; // (0,0)
			int32 X, Y;
			TestTrue(TEXT("Still finds a slot"), ULensGridInventoryComponent::FindFirstFreeSlot(1, 1, 16, 16, Occupied, X, Y));
			TestEqual(TEXT("Next cell along the row"), X, 1);
			TestEqual(TEXT("Same row"), Y, 0);
		});

		It("ReturnsFalseWhenNothingFits", [this]()
		{
			TArray<bool> Occupied;
			Occupied.Init(true, 2 * 2); // every cell of a 2x2 grid occupied
			int32 X, Y;
			TestFalse(TEXT("No room anywhere"), ULensGridInventoryComponent::FindFirstFreeSlot(1, 1, 2, 2, Occupied, X, Y));
		});
	});

	Describe("ULensGridInventoryComponent", [this]()
	{
		It("AddLensPlacesItAndReturnsAValidId", [this]()
		{
			const FGuid Id = Inventory->AddLens(MakeLens(1, 1));
			TestTrue(TEXT("New lens gets a valid Id"), Id.IsValid());

			int32 X, Y;
			TestTrue(TEXT("It's placed in the grid"), Inventory->GetGridPosition(Id, X, Y));
			TestEqual(TEXT("At the origin, the grid is empty"), X, 0);
			TestEqual(TEXT("At the origin, the grid is empty"), Y, 0);
		});

		It("SecondLensDoesNotOverlapTheFirst", [this]()
		{
			const FGuid FirstId = Inventory->AddLens(MakeLens(2, 2));
			const FGuid SecondId = Inventory->AddLens(MakeLens(1, 1));

			int32 FirstX, FirstY, SecondX, SecondY;
			Inventory->GetGridPosition(FirstId, FirstX, FirstY);
			Inventory->GetGridPosition(SecondId, SecondX, SecondY);

			const bool bOverlaps = SecondX >= FirstX && SecondX < FirstX + 2 && SecondY >= FirstY && SecondY < FirstY + 2;
			TestFalse(TEXT("Second lens was placed outside the first lens's 2x2 footprint"), bOverlaps);
		});

		It("RemoveLensFreesItsCellsForReuse", [this]()
		{
			const FGuid FirstId = Inventory->AddLens(MakeLens(16, 16)); // fills the entire grid
			FLensDefinition Removed;
			TestTrue(TEXT("Removes successfully"), Inventory->RemoveLens(FirstId, Removed));

			const FGuid SecondId = Inventory->AddLens(MakeLens(1, 1));
			TestTrue(TEXT("A new lens can be placed now the grid is empty again"), SecondId.IsValid());
		});

		It("RemoveLensOnUnknownIdFails", [this]()
		{
			FLensDefinition Removed;
			TestFalse(TEXT("Unknown Id returns false"), Inventory->RemoveLens(FGuid::NewGuid(), Removed));
		});

		It("MoveLensToGridRelocatesIt", [this]()
		{
			const FGuid Id = Inventory->AddLens(MakeLens(1, 1));
			TestTrue(TEXT("Moves to a free cell"), Inventory->MoveLensToGrid(Id, 5, 5));

			int32 X, Y;
			Inventory->GetGridPosition(Id, X, Y);
			TestEqual(TEXT("New X"), X, 5);
			TestEqual(TEXT("New Y"), Y, 5);
		});

		It("MoveLensToGridFailsIntoAnOccupiedCellAndLeavesItWhereItWas", [this]()
		{
			const FGuid FirstId = Inventory->AddLens(MakeLens(1, 1));
			Inventory->MoveLensToGrid(FirstId, 3, 3);
			const FGuid SecondId = Inventory->AddLens(MakeLens(1, 1));

			TestFalse(TEXT("Can't move onto an occupied cell"), Inventory->MoveLensToGrid(SecondId, 3, 3));

			int32 X, Y;
			Inventory->GetGridPosition(SecondId, X, Y);
			const bool bStillAtOriginalSpot = !(X == 3 && Y == 3);
			TestTrue(TEXT("Failed move left the lens where it was"), bStillAtOriginalSpot);
		});

		It("MoveLensToHotbarEquipsItAndClearsItsGridCell", [this]()
		{
			const FGuid Id = Inventory->AddLens(MakeLens(1, 1));
			TestTrue(TEXT("Moves into slot 0"), Inventory->MoveLensToHotbar(Id, 0));

			TestEqual(TEXT("Slot 0 now holds it"), Inventory->GetHotbarSlot(0), Id);

			int32 X, Y;
			TestFalse(TEXT("No longer has a grid position"), Inventory->GetGridPosition(Id, X, Y));
		});

		It("MoveLensToHotbarSwapsWithWhateverWasThere", [this]()
		{
			const FGuid GridLensId = Inventory->AddLens(MakeLens(1, 1, TEXT("Incoming")));
			const FGuid HotbarLensId = Inventory->AddLens(MakeLens(1, 1, TEXT("Displaced")));
			Inventory->MoveLensToHotbar(HotbarLensId, 0);

			TestTrue(TEXT("Moving the grid lens into the occupied slot succeeds"), Inventory->MoveLensToHotbar(GridLensId, 0));

			TestEqual(TEXT("Slot 0 now holds the incoming lens"), Inventory->GetHotbarSlot(0), GridLensId);

			int32 X, Y;
			TestTrue(TEXT("The displaced lens landed back in the grid somewhere"), Inventory->GetGridPosition(HotbarLensId, X, Y));
		});

		It("MovingTheSameLensToItsOwnHotbarSlotIsANoOpSuccess", [this]()
		{
			const FGuid Id = Inventory->AddLens(MakeLens(1, 1));
			Inventory->MoveLensToHotbar(Id, 1);
			TestTrue(TEXT("Re-targeting the same slot succeeds without disturbing anything"), Inventory->MoveLensToHotbar(Id, 1));
			TestEqual(TEXT("Still in slot 1"), Inventory->GetHotbarSlot(1), Id);
		});

		It("SetActiveHotbarSlotBroadcastsTheNewSlotsOccupant", [this]()
		{
			const FGuid SlotZeroLens = Inventory->AddLens(MakeLens(1, 1, TEXT("Slot0")));
			const FGuid SlotOneLens = Inventory->AddLens(MakeLens(1, 1, TEXT("Slot1")));
			Inventory->MoveLensToHotbar(SlotZeroLens, 0);
			Inventory->MoveLensToHotbar(SlotOneLens, 1);

			FGuid BroadcastLensId;
			int32 BroadcastCount = 0;
			Inventory->OnActiveLensChanged.AddLambda([&BroadcastLensId, &BroadcastCount](FGuid LensId)
			{
				BroadcastLensId = LensId;
				++BroadcastCount;
			});

			Inventory->SetActiveHotbarSlot(1);

			TestEqual(TEXT("Broadcasts once"), BroadcastCount, 1);
			TestEqual(TEXT("With slot 1's lens"), BroadcastLensId, SlotOneLens);
		});

		It("AddLensReturnsInvalidGuidWhenTheGridIsFull", [this]()
		{
			Inventory->AddLens(MakeLens(16, 16));
			const FGuid Overflow = Inventory->AddLens(MakeLens(1, 1));
			TestFalse(TEXT("No room left anywhere"), Overflow.IsValid());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
