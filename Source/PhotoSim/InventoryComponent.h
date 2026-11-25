// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWeightChanged, float /*NewTotalWeightKG*/);

/**
 * Tracks carried weight as a sum of independently-owned contributions (camera body, lenses,
 * whatever else gets added later). Each contributor registers under itself as the key so it can
 * update its own share without knowing about the others.
 */
UCLASS()
class PHOTOSIM_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void RegisterWeight(UObject* Source, float WeightKG);
	void UnregisterWeight(UObject* Source);

	float GetTotalWeightKG() const;

	FOnWeightChanged OnWeightChanged;

private:
	TMap<TWeakObjectPtr<UObject>, float> WeightContributions;
};
