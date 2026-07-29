// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraPickup.h"
#include "TP_CameraComponent.h"
#include "TP_PickUpComponent.h"
#include "PhotoSimCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ACameraPickup::ACameraPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;

	CameraComponent = CreateDefaultSubobject<UTP_CameraComponent>(TEXT("CameraModel"));
	CameraComponent->SetupAttachment(RootComponent);

	PickupTrigger = CreateDefaultSubobject<UTP_PickUpComponent>(TEXT("PickupTrigger"));
	PickupTrigger->SetupAttachment(RootComponent);
	PickupTrigger->SetRelativeLocation(FVector(0.f, 0.f, 5.f));

	PickupTrigger->OnPickUp.AddDynamic(this, &ACameraPickup::HandlePickUp);

	// The visual model: an imported mesh (its own material/texture come along with it) plus a
	// small overlay quad for the live-view screen. Both are direct components of this actor
	// (attached to CameraComponent so they move together with it) rather than nested inside
	// CameraComponent, so a Blueprint made from this class inherits them reliably.

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CameraMeshAsset(TEXT("/Game/PhotoSim/Meshes/SM_Camera.SM_Camera"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ScreenMaterialAsset(TEXT("/Game/PhotoSim/Materials/M_CameraScreen.M_CameraScreen"));
	ScreenMaterialBase = ScreenMaterialAsset.Object;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(CameraComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(true);
	Body->SetMobility(EComponentMobility::Movable);
	if (CameraMeshAsset.Object != nullptr)
	{
		Body->SetStaticMesh(CameraMeshAsset.Object);
	}

	Screen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Screen"));
	Screen->SetupAttachment(CameraComponent);
	Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Screen->SetCastShadow(false);
	Screen->SetMobility(EComponentMobility::Movable);
	if (CubeMeshAsset.Object != nullptr)
	{
		Screen->SetStaticMesh(CubeMeshAsset.Object);
	}
	if (ScreenMaterialBase != nullptr)
	{
		Screen->SetMaterial(0, ScreenMaterialBase);
	}

	// Starting guesses only - the imported mesh's real dimensions/pivot aren't known here, so
	// both of these will need a visual nudge in the editor once you can see the actual model.
	// Body is left at identity (adjust its relative Location/Rotation/Scale in the Details panel
	// to fit HipPose/ViewfinderPose properly). Screen is a small quad placeholder positioned
	// roughly where a rear LCD would sit - drag it to match the real model's screen by eye.
	Screen->SetRelativeLocation(FVector(-3.5f, 0.f, 0.f));
	Screen->SetRelativeScale3D(FVector(0.02f, 0.05f, 0.04f));
}

void ACameraPickup::HandlePickUp(APhotoSimCharacter* PickUpCharacter)
{
	if (CameraComponent == nullptr || PickUpCharacter == nullptr || !CameraComponent->AttachCamera(PickUpCharacter))
	{
		return;
	}

	// The visual mesh parts are only scene-attached to CameraComponent, not owned by it - they
	// still belong to this actor. Rename() moves actual ownership (Outer) to the character, the
	// same way AttachCamera just did for CameraComponent itself; without it, Destroy() below
	// would garbage-collect these meshes along with this actor.
	for (UStaticMeshComponent* const Part : { Body, Screen })
	{
		if (Part != nullptr)
		{
			Part->Rename(nullptr, PickUpCharacter);
			PickUpCharacter->AddInstanceComponent(Part);
		}
	}

	CameraComponent->SetupLiveViewScreen(Screen, ScreenMaterialBase);

	Destroy();
}
