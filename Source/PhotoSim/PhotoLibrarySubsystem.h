// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PhotoLibrarySubsystem.generated.h"

class UTexture2D;

DECLARE_MULTICAST_DELEGATE(FOnPhotoLibraryChanged);

/** The camera settings a shot was taken with, read back for the gallery's playback view. */
USTRUCT()
struct FPhotoMetadata
{
	GENERATED_BODY()

	UPROPERTY()
	float Aperture = 0.f;

	UPROPERTY()
	float ShutterSpeed = 0.f;

	UPROPERTY()
	float ISO = 0.f;

	UPROPERTY()
	float FocalLengthMM = 0.f;

	UPROPERTY()
	FString CameraBodyName;

	UPROPERTY()
	FDateTime Timestamp = FDateTime::MinValue();
};

/** A single photo plus the settings it was taken with. Id is stable across deletes, so the gallery can track "the currently shown photo" without relying on array position. */
USTRUCT()
struct FPhotoRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Id;

	UPROPERTY()
	TObjectPtr<UTexture2D> Texture = nullptr;

	UPROPERTY()
	FPhotoMetadata Metadata;

	UPROPERTY()
	bool bFavorite = false;
};

// A bare TArray<...> can't be used as a TMap value under UHT's reflection rules - wrapping it in
// a struct is the standard way around that.
USTRUCT()
struct FPendingFilmRoll
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FPhotoRecord> Shots;
};

/**
 * Holds every photo taken this session, in the order they were taken. Lives on the GameInstance
 * so it survives level transitions for the length of a play session.
 *
 * Photos are plain UTexture2D (baked from the capture's rotated pixels, see
 * UTP_CameraComponent::TakePhoto), not the live UTextureRenderTarget2D the scene was captured
 * into - a render target is a render surface meant to be redrawn, not a frozen CPU-rotated image.
 */
UCLASS()
class PHOTOSIM_API UPhotoLibrarySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void AddPhoto(UTexture2D* Photo, const FPhotoMetadata& Metadata);

	const TArray<FPhotoRecord>& GetPhotos() const { return Photos; }

	/** Stages a film shot under RollId without adding it to Photos yet - nothing appears in the gallery until DevelopRoll */
	void AddPendingFilmShot(const FGuid& RollId, UTexture2D* Photo, const FPhotoMetadata& Metadata);

	/** Moves every shot staged under RollId into Photos, in the order they were added, then clears the pending entry */
	void DevelopRoll(const FGuid& RollId);

	/** Removes a photo by its stable Id. No-op (and no broadcast) if PhotoId isn't found. */
	void DeletePhoto(const FGuid& PhotoId);

	/** Flips the favorite flag on a photo by its stable Id. No-op (and no broadcast) if PhotoId isn't found. */
	void ToggleFavorite(const FGuid& PhotoId);

	/** Broadcast whenever the Photos array changes (add, delete, develop, favorite toggle), so an open gallery can refresh live */
	FOnPhotoLibraryChanged OnPhotoLibraryChanged;

private:
	UPROPERTY(Transient)
	TArray<FPhotoRecord> Photos;

	UPROPERTY(Transient)
	TMap<FGuid, FPendingFilmRoll> PendingRolls;
};
