// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensPickup.h"
#include "LensGridInventoryComponent.h"
#include "TP_PickUpComponent.h"
#include "PhotoSimCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ALensPickup::ALensPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Every lens uses this same imported model - no per-lens meshes exist yet - distinguished
	// visually by ApplyVisuals()'s focal-length-driven scale instead. Rotation is left at identity
	// since the model's native orientation from Blender/OBJ isn't known here; nudge it in the
	// editor if it doesn't read as lying along the forward axis, the same way ACameraPickup's own
	// Body/Screen placement is flagged as a starting guess.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LensMeshAsset(TEXT("/Game/PhotoSim/Meshes/Lens.Lens"));

	LensMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LensMesh"));
	LensMesh->SetupAttachment(RootComponent);
	LensMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LensMesh->SetCastShadow(true);
	LensMesh->SetMobility(EComponentMobility::Movable);
	if (LensMeshAsset.Object != nullptr)
	{
		LensMesh->SetStaticMesh(LensMeshAsset.Object);
	}

	PickupTrigger = CreateDefaultSubobject<UTP_PickUpComponent>(TEXT("PickupTrigger"));
	PickupTrigger->SetupAttachment(RootComponent);
	PickupTrigger->SetRelativeLocation(FVector(0.f, 0.f, 5.f));

	PickupTrigger->OnPickUp.AddDynamic(this, &ALensPickup::HandlePickUp);
}

float ALensPickup::ComputeLengthScaleForFocalLength(float FocalLengthMM)
{
	return FMath::GetMappedRangeValueClamped(
		FVector2D(MinCatalogFocalLengthMM, MaxCatalogFocalLengthMM),
		FVector2D(MinLengthScale, MaxLengthScale),
		FocalLengthMM);
}

void ALensPickup::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisuals();
}

void ALensPickup::ApplyVisuals()
{
	if (LensMesh == nullptr)
	{
		return;
	}

	if (!LensDefinition.OverrideMesh.IsNull())
	{
		if (UStaticMesh* const OverrideMesh = LensDefinition.OverrideMesh.LoadSynchronous())
		{
			LensMesh->SetStaticMesh(OverrideMesh);
		}
	}

	// Driven by the long end of the zoom range (equal to the short end for a prime) - that's what
	// dominates a real lens's physical length, telephoto zooms included.
	const float LengthScale = ComputeLengthScaleForFocalLength(LensDefinition.MaxFocalLengthMM) * NativeMeshScaleCorrection;
	LensMesh->SetRelativeScale3D(FVector(LengthScale));
}

void ALensPickup::HandlePickUp(APhotoSimCharacter* PickUpCharacter)
{
	if (PickUpCharacter == nullptr)
	{
		return;
	}

	ULensGridInventoryComponent* LensInventory = PickUpCharacter->GetLensInventoryComponent();
	if (LensInventory == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lens] %s picked up but the character has no lens inventory component - leaving it in the world."), *GetName());
		return;
	}

	// Lenses belong to the player, not any specific camera body, so this works with no camera
	// equipped yet - picked-up lenses just sit in the bag until a camera's held to equip them onto.
	if (LensInventory->AddLens(LensDefinition).IsValid())
	{
		PickupTrigger->ConsumePickup();
		Destroy();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lens] %s picked up but the lens inventory is full - leaving it in the world."), *GetName());
	}
}
