// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LensDefinition.h"
#include "LensPickup.generated.h"

class UTP_PickUpComponent;
class UStaticMeshComponent;
class USceneComponent;
class APhotoSimCharacter;

/**
 * A lens sitting in the world that the player can walk into to pick up, mirroring the
 * ACameraPickup pattern. On pickup, the lens is added to the character's already-equipped
 * camera (see UTP_CameraComponent::AddLens) and this actor is destroyed.
 */
UCLASS(Blueprintable, BlueprintType)
class PHOTOSIM_API ALensPickup : public AActor
{
	GENERATED_BODY()

public:
	ALensPickup();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	FLensDefinition LensDefinition;

	/**
	 * Maps a focal length onto a uniform mesh scale, so a 400mm lens reads as visibly bigger than
	 * a 14mm one even though every lens in the catalog shares the same placeholder barrel mesh.
	 * Linear between MinCatalogFocalLengthMM/MaxCatalogFocalLengthMM, clamped outside that range.
	 */
	UFUNCTION(BlueprintPure, Category = "Lens")
	static float ComputeLengthScaleForFocalLength(float FocalLengthMM);

	static constexpr float MinCatalogFocalLengthMM = 14.f;
	static constexpr float MaxCatalogFocalLengthMM = 400.f;
	static constexpr float MinLengthScale = 0.6f;
	static constexpr float MaxLengthScale = 2.4f;

	/**
	 * SM_Lens was modelled in metres (Blender's default unit) and imported from OBJ, a format
	 * with no unit metadata - Unreal reads its numbers as centimetres, so without this it renders
	 * about 100x too small to see. EditAnywhere so a future, correctly-scaled mesh can set this
	 * back to 1 without a code change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float NativeMeshScaleCorrection = 100.f;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> LensMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTP_PickUpComponent> PickupTrigger;

	virtual void BeginPlay() override;

	/** Applies LensDefinition.OverrideMesh (if set) and the focal-length-driven scale to LensMesh. Called from BeginPlay, and safe to call again if LensDefinition changes. */
	void ApplyVisuals();

	UFUNCTION()
	void HandlePickUp(APhotoSimCharacter* PickUpCharacter);
};
