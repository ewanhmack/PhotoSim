// Copyright Epic Games, Inc. All Rights Reserved.

#include "TP_PickUpComponent.h"

UTP_PickUpComponent::UTP_PickUpComponent()
{
	// Setup the Sphere Collision
	SphereRadius = 32.f;
}

void UTP_PickUpComponent::BeginPlay()
{
	Super::BeginPlay();

	// Register our Overlap Event
	OnComponentBeginOverlap.AddDynamic(this, &UTP_PickUpComponent::OnSphereBeginOverlap);
}

void UTP_PickUpComponent::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Checking if it is a First Person Character overlapping
	APhotoSimCharacter* Character = Cast<APhotoSimCharacter>(OtherActor);
	if(Character != nullptr)
	{
		// Notify that the actor is being picked up. Does NOT unregister itself here - a handler
		// that declines the pickup (see ConsumePickup's own comment) needs overlapping again to
		// still re-trigger this.
		OnPickUp.Broadcast(Character);
	}
}

void UTP_PickUpComponent::ConsumePickup()
{
	OnComponentBeginOverlap.RemoveAll(this);
}
