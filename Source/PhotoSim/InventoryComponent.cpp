// Copyright Epic Games, Inc. All Rights Reserved.

#include "InventoryComponent.h"

void UInventoryComponent::RegisterWeight(UObject* Source, float WeightKG)
{
	if (Source == nullptr)
	{
		return;
	}

	const float OldTotal = GetTotalWeightKG();

	WeightContributions.Add(Source, WeightKG);

	const float NewTotal = GetTotalWeightKG();
	if (!FMath::IsNearlyEqual(OldTotal, NewTotal))
	{
		OnWeightChanged.Broadcast(NewTotal);
	}
}

void UInventoryComponent::UnregisterWeight(UObject* Source)
{
	if (Source == nullptr)
	{
		return;
	}

	const float OldTotal = GetTotalWeightKG();

	if (WeightContributions.Remove(Source) == 0)
	{
		return;
	}

	const float NewTotal = GetTotalWeightKG();
	if (!FMath::IsNearlyEqual(OldTotal, NewTotal))
	{
		OnWeightChanged.Broadcast(NewTotal);
	}
}

float UInventoryComponent::GetTotalWeightKG() const
{
	float Total = 0.f;
	for (const TPair<TWeakObjectPtr<UObject>, float>& Pair : WeightContributions)
	{
		if (Pair.Key.IsValid())
		{
			Total += Pair.Value;
		}
	}
	return Total;
}
