// Copyright Epic Games, Inc. All Rights Reserved.

#include "StaminaComponent.h"
#include "InventoryComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogStamina, Log, All);

UStaminaComponent::UStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	CurrentStamina = MaxStamina;

	// Flat multiplier of 1 until Julien tunes it in the Details panel - behavior is unchanged
	// from a no-weight-coupling world until these keys are edited.
	WeightToDrainMultiplierCurve.EditorCurveData.AddKey(0.f, 1.f);
	WeightToDrainMultiplierCurve.EditorCurveData.AddKey(10.f, 1.f);
}

void UStaminaComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentStamina = MaxStamina;

	if (AActor* Owner = GetOwner())
	{
		CachedInventory = Owner->FindComponentByClass<UInventoryComponent>();
	}

	if (!CachedInventory.IsValid())
	{
		UE_LOG(LogStamina, Warning, TEXT("'%s' found no UInventoryComponent on its owner - weight multiplier will default to 1.0"), *GetNameSafe(this));
	}
}

void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsSprinting)
	{
		const bool bWasAboveZero = CurrentStamina > 0.f;

		const float WeightKG = CachedInventory.IsValid() ? CachedInventory->GetTotalWeightKG() : 0.f;
		const float WeightMultiplier = WeightToDrainMultiplierCurve.GetRichCurveConst()->Eval(WeightKG);

		CurrentStamina = FMath::Clamp(CurrentStamina - DrainPerSecondSprinting * WeightMultiplier * DeltaTime, 0.f, MaxStamina);

		if (bWasAboveZero && CurrentStamina <= 0.f)
		{
			OnExhausted.Broadcast();
		}
	}
	else
	{
		TimeSinceSprintStopped += DeltaTime;

		if (TimeSinceSprintStopped >= RegenDelayAfterSprint)
		{
			CurrentStamina = FMath::Clamp(CurrentStamina + RegenPerSecond * DeltaTime, 0.f, MaxStamina);
		}
	}
}

bool UStaminaComponent::CanStartSprinting() const
{
	return CurrentStamina >= MinStaminaToStartSprint;
}

void UStaminaComponent::NotifySprintStarted()
{
	bIsSprinting = true;
	TimeSinceSprintStopped = 0.f;
}

void UStaminaComponent::NotifySprintStopped()
{
	bIsSprinting = false;
	TimeSinceSprintStopped = 0.f;
}

float UStaminaComponent::GetStaminaFraction() const
{
	return MaxStamina > 0.f ? FMath::Clamp(CurrentStamina / MaxStamina, 0.f, 1.f) : 0.f;
}
