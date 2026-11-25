// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "StaminaComponent.generated.h"

class UInventoryComponent;

DECLARE_MULTICAST_DELEGATE(FOnExhausted);

/**
 * Drains while sprinting, regenerates after a delay once sprinting stops. Drain rate scales with
 * carried weight via WeightToDrainMultiplierCurve, read from the owner's UInventoryComponent.
 */
UCLASS()
class PHOTOSIM_API UStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStaminaComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float MaxStamina = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float DrainPerSecondSprinting = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float RegenPerSecond = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float RegenDelayAfterSprint = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float MinStaminaToStartSprint = 10.f;

	// Weight coupling: heavier kit drains stamina faster while sprinting. Evaluated against
	// UInventoryComponent::GetTotalWeightKG(); X axis is kg, Y axis is a multiplier on DrainPerSecondSprinting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	FRuntimeFloatCurve WeightToDrainMultiplierCurve;

	UPROPERTY(BlueprintReadOnly, Category = "Stamina")
	float CurrentStamina;

	bool CanStartSprinting() const;
	void NotifySprintStarted();
	void NotifySprintStopped();
	float GetStaminaFraction() const;

	FOnExhausted OnExhausted;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool bIsSprinting = false;
	float TimeSinceSprintStopped = 0.f;
	TWeakObjectPtr<UInventoryComponent> CachedInventory;
};
