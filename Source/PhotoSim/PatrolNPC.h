// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PatrolNPC.generated.h"

/**
 * Scripted patrol NPC: walks back and forth between PatrolPoints, bouncing at either end. No
 * AIController or NavMesh - the brief explicitly allows "scripted/patterned transformations" for
 * NPC movement, and ACharacter's own CharacterMovementComponent (gravity, collision) already does
 * everything a patrol needs once it's fed a direction each tick via AddMovementInput.
 *
 * Purely ambient - objectives are APhotoObjectiveArea now, not actor-tagged, so this NPC isn't a
 * capturable subject itself. Place an objective area somewhere along its patrol route if you want
 * a shot that requires catching it passing through.
 */
UCLASS()
class PHOTOSIM_API APatrolNPC : public ACharacter
{
	GENERATED_BODY()

public:
	APatrolNPC();

	/** Waypoints relative to wherever this NPC is placed in the level, walked in order then back */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	TArray<FVector> PatrolPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	float PatrolSpeed = 150.f;

	/** How close (horizontally) counts as "arrived" at the current waypoint */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	float WaypointReachedTolerance = 50.f;

	/**
	 * Next waypoint index after reaching CurrentIndex, ping-ponging between 0 and NumWaypoints-1.
	 * bForwardDirection is updated in place. Pure so the bounce logic is unit-testable without
	 * spawning an actor.
	 */
	static int32 ComputeNextWaypointIndex(int32 CurrentIndex, int32 NumWaypoints, bool& bForwardDirection);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	FVector OriginLocation;
	int32 CurrentWaypointIndex = 0;
	bool bMovingForward = true;
};
