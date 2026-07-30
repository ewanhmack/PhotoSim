// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CameraPickup.generated.h"

class UTP_CameraComponent;
class UTP_PickUpComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class APhotoSimCharacter;

/**
 * A camera sitting in the world that the player can walk into to pick up, mirroring the
 * BP_PickUp_Rifle pattern. On pickup, the UTP_CameraComponent rig is handed off to the
 * character (see UTP_CameraComponent::AttachCamera) and this actor is destroyed.
 *
 * The visual mesh parts are direct components of this actor (attached to CameraComponent so
 * they move together with it) rather than nested inside CameraComponent, so that a Blueprint
 * made from this class reliably inherits them.
 */
UCLASS(Blueprintable, BlueprintType)
class PHOTOSIM_API ACameraPickup : public AActor
{
	GENERATED_BODY()

public:
	ACameraPickup();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UTP_CameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UTP_PickUpComponent> PickupTrigger;

	/** The imported camera model. Loaded from /Game/PhotoSim/Meshes/SM_Camera. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera|Model")
	TObjectPtr<UStaticMeshComponent> Body;

	/** LCD screen overlay - fed a live capture from the lens once equipped, see UTP_CameraComponent::SetupLiveViewScreen */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera|Model")
	TObjectPtr<UStaticMeshComponent> Screen;

protected:
	/**
	 * Base material for the rear screen. Must expose a Texture Parameter named "ScreenTexture"
	 * driving Emissive Color (Unlit, Opaque). Loaded from /Game/PhotoSim/Materials/M_CameraScreen.
	 * Transient, not EditDefaultsOnly - re-resolved by the constructor via ConstructorHelpers
	 * every time, so a Blueprint can never freeze a stale (e.g. None) override onto it.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ScreenMaterialBase;

	UFUNCTION()
	void HandlePickUp(APhotoSimCharacter* PickUpCharacter);
};
