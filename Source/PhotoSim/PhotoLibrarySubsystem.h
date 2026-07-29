// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PhotoLibrarySubsystem.generated.h"

class UTextureRenderTarget2D;

DECLARE_MULTICAST_DELEGATE(FOnPhotoLibraryChanged);

/**
 * Holds every photo taken this session, in the order they were taken. Lives on the GameInstance
 * so it survives level transitions for the length of a play session.
 */
UCLASS()
class PHOTOSIM_API UPhotoLibrarySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void AddPhoto(UTextureRenderTarget2D* Photo);

	const TArray<TObjectPtr<UTextureRenderTarget2D>>& GetPhotos() const { return Photos; }

	/** Broadcast whenever a new photo is added, so an open gallery can refresh live */
	FOnPhotoLibraryChanged OnPhotoLibraryChanged;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> Photos;
};
