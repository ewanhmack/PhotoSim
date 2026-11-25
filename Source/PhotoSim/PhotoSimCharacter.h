// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Curves/CurveFloat.h"
#include "PhotoSimCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UInventoryComponent;
class UStaminaComponent;
class ULensGridInventoryComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class APhotoSimCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* Mesh1P;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Tracks total carried weight across whatever's equipped */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Inventory, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> InventoryComponent;

	/** Drains while sprinting, regenerates when not */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Stamina, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaminaComponent> StaminaComponent;

	/** Every lens owned, independent of whichever camera (if any) is currently equipped */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Inventory, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULensGridInventoryComponent> LensInventoryComponent;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Crouch Input Action (toggle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* CrouchAction;

	/** Sprint Input Action (hold) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* SprintAction;

public:
	APhotoSimCharacter();

protected:
	virtual void BeginPlay();

public:

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	/** Normal walk speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement")
	float WalkSpeed = 600.f;

	/** Speed while Sprint is held */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement")
	float SprintSpeed = 1000.f;

	/** Speed while crouched */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement")
	float CrouchSpeed = 300.f;

	// Default keys (0,1.0) and (10,1.0) so behavior is unchanged until Julien tunes it.
	/** Multiplies walk/sprint/crouch speed based on total carried weight (kg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement")
	FRuntimeFloatCurve WeightToSpeedMultiplierCurve;

protected:
	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Toggles crouch on/off */
	void ToggleCrouch(const FInputActionValue& Value);

	/** Starts sprinting if stamina allows it */
	void StartSprint(const FInputActionValue& Value);

	/** Stops sprinting on release */
	void StopSprint(const FInputActionValue& Value);

	/** Same as StopSprint, minus the FInputActionValue param - for binding to UStaminaComponent::OnExhausted */
	void ForceStopSprint();

	// Shifts the first person camera to stay at a consistent eye height as the capsule
	// shrinks/grows, since the camera is a fixed relative offset from the capsule's centre.
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

private:
	// bWantsToSprint is the only source of truth for sprint intent - MaxWalkSpeed is scaled by
	// carried weight now, so it no longer exactly equals SprintSpeed/WalkSpeed and can't be used
	// to infer sprint state.
	bool bWantsToSprint = false;

	/** Reapplies WalkSpeed/SprintSpeed/CrouchSpeed scaled by the current weight multiplier */
	void RecomputeMovementSpeed();

	void OnWeightChangedHandler(float NewTotalWeightKG);

	/** Hands the player one starter lens (the catalog's 50mm prime, or a hardcoded fallback) and puts it straight into hotbar slot 0 */
	void SeedStarterLens();

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	/** Returns InventoryComponent subobject **/
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	/** Returns StaminaComponent subobject **/
	UStaminaComponent* GetStaminaComponent() const { return StaminaComponent; }
	/** Returns LensInventoryComponent subobject **/
	ULensGridInventoryComponent* GetLensInventoryComponent() const { return LensInventoryComponent; }

};
