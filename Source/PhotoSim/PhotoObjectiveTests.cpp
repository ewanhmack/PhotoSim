// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "PhotoSubjectComponent.h"
#include "PhotoSimGameState.h"
#include "PhotoObjectiveArea.h"
#include "PatrolNPC.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

BEGIN_DEFINE_SPEC(FPhotoObjectiveSpec, "PhotoSim", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UWorld* TestWorld = nullptr;
	AActor* TestActor = nullptr;
	UPhotoSubjectComponent* Subject = nullptr;
	APhotoSimGameState* GameState = nullptr;

	// See InventoryStaminaTests.cpp's own comment on this guard for why it's needed at all.
	TUniquePtr<FScopedAllowAbstractClassAllocation> AbstractAllocationGuard;
END_DEFINE_SPEC(FPhotoObjectiveSpec)

void FPhotoObjectiveSpec::Define()
{
	BeforeEach([this]()
	{
		AbstractAllocationGuard = MakeUnique<FScopedAllowAbstractClassAllocation>();

		TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
		TestActor = TestWorld->SpawnActor<AActor>();
		Subject = NewObject<UPhotoSubjectComponent>(TestActor);
		Subject->RegisterComponent();
		GameState = TestWorld->SpawnActor<APhotoSimGameState>();
	});

	AfterEach([this]()
	{
		if (TestWorld != nullptr)
		{
			TestWorld->DestroyWorld(false);
			TestWorld = nullptr;
		}
		TestActor = nullptr;
		Subject = nullptr;
		GameState = nullptr;
		AbstractAllocationGuard.Reset();
	});

	Describe("PhotoSubjectComponent", [this]()
	{
		It("FirstCaptureSucceedsAndBroadcasts", [this]()
		{
			int32 BroadcastCount = 0;
			Subject->OnCaptured.AddLambda([&BroadcastCount]() { ++BroadcastCount; });

			const bool bResult = Subject->TryCapture();

			TestTrue(TEXT("First capture returns true"), bResult);
			TestTrue(TEXT("bIsCaptured is now set"), Subject->bIsCaptured);
			TestEqual(TEXT("Broadcasts exactly once"), BroadcastCount, 1);
		});

		It("SecondCaptureIsANoOp", [this]()
		{
			int32 BroadcastCount = 0;
			Subject->OnCaptured.AddLambda([&BroadcastCount]() { ++BroadcastCount; });

			Subject->TryCapture();
			const bool bSecondResult = Subject->TryCapture();

			TestFalse(TEXT("Second capture returns false"), bSecondResult);
			TestEqual(TEXT("Does not broadcast again"), BroadcastCount, 1);
		});
	});

	Describe("PhotoSimGameState", [this]()
	{
		It("WinsOnceAllObjectivesCaptured", [this]()
		{
			GameState->InitRound(2, 100.f);

			GameState->NotifySubjectCaptured();
			TestEqual(TEXT("Round still in progress after 1 of 2"), GameState->RoundState, EPhotoSimRoundState::InProgress);

			GameState->NotifySubjectCaptured();
			TestEqual(TEXT("Round won once all objectives are captured"), GameState->RoundState, EPhotoSimRoundState::Won);
		});

		It("LosesWhenTimerReachesZero", [this]()
		{
			GameState->InitRound(4, 1.f);

			GameState->TickTimer(0.6f);
			TestEqual(TEXT("Still in progress with time left"), GameState->RoundState, EPhotoSimRoundState::InProgress);

			GameState->TickTimer(0.6f);
			TestEqual(TEXT("Lost once the clock reaches zero"), GameState->RoundState, EPhotoSimRoundState::Lost);
			TestEqual(TEXT("Clamped at zero, not negative"), GameState->TimeRemainingSeconds, 0.f);
		});

		It("DecidedRoundIgnoresFurtherEvents", [this]()
		{
			GameState->InitRound(1, 10.f);
			GameState->NotifySubjectCaptured();
			TestEqual(TEXT("Won after the only objective"), GameState->RoundState, EPhotoSimRoundState::Won);

			int32 BroadcastCount = 0;
			GameState->OnRoundStateChanged.AddLambda([&BroadcastCount](EPhotoSimRoundState) { ++BroadcastCount; });

			GameState->TickTimer(100.f);
			TestEqual(TEXT("Still Won, a late timeout does not override a win"), GameState->RoundState, EPhotoSimRoundState::Won);
			TestEqual(TEXT("No further broadcast once decided"), BroadcastCount, 0);
		});

		It("BroadcastsOnlyOnceOnTheDecidingTransition", [this]()
		{
			GameState->InitRound(1, 10.f);

			int32 BroadcastCount = 0;
			EPhotoSimRoundState LastBroadcastState = EPhotoSimRoundState::InProgress;
			GameState->OnRoundStateChanged.AddLambda([&BroadcastCount, &LastBroadcastState](EPhotoSimRoundState NewState)
			{
				++BroadcastCount;
				LastBroadcastState = NewState;
			});

			GameState->NotifySubjectCaptured();

			TestEqual(TEXT("Broadcasts exactly once"), BroadcastCount, 1);
			TestEqual(TEXT("Broadcasts the Won state"), LastBroadcastState, EPhotoSimRoundState::Won);
		});

		It("ZeroObjectivesNeverAutoWins", [this]()
		{
			GameState->InitRound(0, 10.f);

			// Nothing to capture - should sit in InProgress until the timer runs out, not win for free.
			TestEqual(TEXT("Round state unaffected by InitRound alone"), GameState->RoundState, EPhotoSimRoundState::InProgress);

			GameState->TickTimer(10.f);
			TestEqual(TEXT("Loses on timeout rather than winning with zero objectives"), GameState->RoundState, EPhotoSimRoundState::Lost);
		});
	});

	Describe("PhotoObjectiveArea", [this]()
	{
		It("CapturesWhenInsideAreaAndFacingTheRightWay", [this]()
		{
			APhotoObjectiveArea* Area = TestWorld->SpawnActor<APhotoObjectiveArea>();
			Area->AreaVolume->SetBoxExtent(FVector(100.f, 100.f, 100.f));

			const bool bResult = Area->TryCaptureFrom(Area->GetActorLocation(), FVector::ForwardVector);

			TestTrue(TEXT("Captures when standing in the area facing the required direction"), bResult);
			TestTrue(TEXT("PhotoSubject is marked captured"), Area->PhotoSubject->bIsCaptured);
		});

		It("DoesNotCaptureWhenOutsideTheArea", [this]()
		{
			APhotoObjectiveArea* Area = TestWorld->SpawnActor<APhotoObjectiveArea>();
			Area->AreaVolume->SetBoxExtent(FVector(100.f, 100.f, 100.f));

			const FVector FarAway = Area->GetActorLocation() + FVector(1000.f, 0.f, 0.f);
			const bool bResult = Area->TryCaptureFrom(FarAway, FVector::ForwardVector);

			TestFalse(TEXT("Does not capture from outside the area"), bResult);
		});

		It("DoesNotCaptureWhenFacingTheWrongWay", [this]()
		{
			APhotoObjectiveArea* Area = TestWorld->SpawnActor<APhotoObjectiveArea>();
			Area->AreaVolume->SetBoxExtent(FVector(100.f, 100.f, 100.f));

			const bool bResult = Area->TryCaptureFrom(Area->GetActorLocation(), -FVector::ForwardVector);

			TestFalse(TEXT("Does not capture when facing away from the required direction"), bResult);
		});

		It("SecondCaptureAttemptIsANoOp", [this]()
		{
			APhotoObjectiveArea* Area = TestWorld->SpawnActor<APhotoObjectiveArea>();
			Area->AreaVolume->SetBoxExtent(FVector(100.f, 100.f, 100.f));

			Area->TryCaptureFrom(Area->GetActorLocation(), FVector::ForwardVector);
			const bool bSecondResult = Area->TryCaptureFrom(Area->GetActorLocation(), FVector::ForwardVector);

			TestFalse(TEXT("Already captured - second attempt is a no-op"), bSecondResult);
		});
	});

	Describe("PhotoObjectiveArea pure math", [this]()
	{
		It("PointInsideBoxExtentRespectsAllThreeAxes", [this]()
		{
			const FVector Extent(100.f, 50.f, 25.f);

			TestTrue(TEXT("Origin is always inside"), APhotoObjectiveArea::IsPointInsideBoxExtent(FVector::ZeroVector, Extent));
			TestTrue(TEXT("Exactly on the boundary counts as inside"), APhotoObjectiveArea::IsPointInsideBoxExtent(FVector(100.f, 50.f, 25.f), Extent));
			TestFalse(TEXT("Just past X is outside"), APhotoObjectiveArea::IsPointInsideBoxExtent(FVector(100.1f, 0.f, 0.f), Extent));
			TestFalse(TEXT("Just past Y is outside"), APhotoObjectiveArea::IsPointInsideBoxExtent(FVector(0.f, 50.1f, 0.f), Extent));
			TestFalse(TEXT("Just past Z is outside"), APhotoObjectiveArea::IsPointInsideBoxExtent(FVector(0.f, 0.f, 25.1f), Extent));
		});

		It("FacingToleranceAcceptsWithinConeAndRejectsOutside", [this]()
		{
			TestTrue(TEXT("Exact match always passes"), APhotoObjectiveArea::IsWithinFacingTolerance(FVector::ForwardVector, FVector::ForwardVector, 45.f));

			const FVector Within30Degrees = FRotator(0.f, 30.f, 0.f).RotateVector(FVector::ForwardVector);
			TestTrue(TEXT("30 degrees off passes a 45 degree tolerance"), APhotoObjectiveArea::IsWithinFacingTolerance(Within30Degrees, FVector::ForwardVector, 45.f));

			const FVector Beyond60Degrees = FRotator(0.f, 60.f, 0.f).RotateVector(FVector::ForwardVector);
			TestFalse(TEXT("60 degrees off fails a 45 degree tolerance"), APhotoObjectiveArea::IsWithinFacingTolerance(Beyond60Degrees, FVector::ForwardVector, 45.f));

			TestFalse(TEXT("Facing directly away always fails"), APhotoObjectiveArea::IsWithinFacingTolerance(-FVector::ForwardVector, FVector::ForwardVector, 45.f));
		});
	});

	Describe("PatrolNPC waypoint bounce", [this]()
	{
		It("PingPongsBetweenTwoWaypoints", [this]()
		{
			bool bForward = true;
			int32 Index = 0;

			Index = APatrolNPC::ComputeNextWaypointIndex(Index, 2, bForward);
			TestEqual(TEXT("0 -> 1"), Index, 1);
			TestTrue(TEXT("Still moving forward"), bForward);

			Index = APatrolNPC::ComputeNextWaypointIndex(Index, 2, bForward);
			TestEqual(TEXT("1 -> 0, bounces"), Index, 0);
			TestFalse(TEXT("Now moving backward"), bForward);

			Index = APatrolNPC::ComputeNextWaypointIndex(Index, 2, bForward);
			TestEqual(TEXT("0 -> 1 again"), Index, 1);
			TestTrue(TEXT("Bounced back to forward"), bForward);
		});

		It("NeverRevisitsAnEndpointTwiceInARowWithThreeWaypoints", [this]()
		{
			bool bForward = true;
			int32 Index = 0;
			const int32 Expected[] = { 1, 2, 1, 0, 1, 2 };

			for (int32 i = 0; i < 6; ++i)
			{
				Index = APatrolNPC::ComputeNextWaypointIndex(Index, 3, bForward);
				TestEqual(FString::Printf(TEXT("Step %d"), i), Index, Expected[i]);
			}
		});

		It("SingleWaypointAlwaysReturnsZero", [this]()
		{
			bool bForward = true;
			TestEqual(TEXT("Only one waypoint - always index 0"), APatrolNPC::ComputeNextWaypointIndex(0, 1, bForward), 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
