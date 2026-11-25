// Copyright Epic Games, Inc. All Rights Reserved.

#include "PatrolNPC.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"

APatrolNPC::APatrolNPC()
{
	PrimaryActorTick.bCanEverTick = true;

	// Gives a spawned NPC a visible, animated body out of the box instead of an invisible capsule -
	// the mannequin + locomotion AnimBP that already ship with the spruce forest pack. Override per
	// instance in the Details panel if a different mesh is wanted.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMeshAsset(TEXT("/Game/PN_interactiveSpruceForest/UE4_Mannequin/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
	if (MannequinMeshAsset.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannequinMeshAsset.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPClassFinder(TEXT("/Game/PN_interactiveSpruceForest/UE4_Mannequin/Mannequin/Animations/ThirdPerson_AnimBP"));
	if (AnimBPClassFinder.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimBPClassFinder.Class);
	}
}

void APatrolNPC::BeginPlay()
{
	Super::BeginPlay();

	OriginLocation = GetActorLocation();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = PatrolSpeed;
	}
}

void APatrolNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (PatrolPoints.Num() == 0)
	{
		return;
	}

	const FVector TargetLocation = OriginLocation + PatrolPoints[CurrentWaypointIndex];
	const FVector ToTarget = TargetLocation - GetActorLocation();
	const FVector FlatToTarget(ToTarget.X, ToTarget.Y, 0.f);

	if (FlatToTarget.SizeSquared() <= FMath::Square(WaypointReachedTolerance))
	{
		CurrentWaypointIndex = ComputeNextWaypointIndex(CurrentWaypointIndex, PatrolPoints.Num(), bMovingForward);
		return;
	}

	const FVector Direction = FlatToTarget.GetSafeNormal();
	AddMovementInput(Direction, 1.f);
	SetActorRotation(Direction.ToOrientationQuat());
}

int32 APatrolNPC::ComputeNextWaypointIndex(int32 CurrentIndex, int32 NumWaypoints, bool& bForwardDirection)
{
	if (NumWaypoints <= 1)
	{
		return 0;
	}

	int32 NextIndex = CurrentIndex + (bForwardDirection ? 1 : -1);

	if (NextIndex >= NumWaypoints)
	{
		bForwardDirection = false;
		NextIndex = NumWaypoints - 2;
	}
	else if (NextIndex < 0)
	{
		bForwardDirection = true;
		NextIndex = FMath::Min(1, NumWaypoints - 1);
	}

	return NextIndex;
}
