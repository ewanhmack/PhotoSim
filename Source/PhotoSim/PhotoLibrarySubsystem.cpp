// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhotoLibrarySubsystem.h"
#include "Engine/Texture2D.h"

void UPhotoLibrarySubsystem::AddPhoto(UTexture2D* Photo, const FPhotoMetadata& Metadata)
{
	if (Photo == nullptr)
	{
		return;
	}

	FPhotoRecord Record;
	Record.Id = FGuid::NewGuid();
	Record.Texture = Photo;
	Record.Metadata = Metadata;

	Photos.Add(MoveTemp(Record));
	OnPhotoLibraryChanged.Broadcast();
}

void UPhotoLibrarySubsystem::AddPendingFilmShot(const FGuid& RollId, UTexture2D* Photo, const FPhotoMetadata& Metadata)
{
	if (Photo == nullptr)
	{
		return;
	}

	FPhotoRecord Record;
	Record.Id = FGuid::NewGuid();
	Record.Texture = Photo;
	Record.Metadata = Metadata;

	PendingRolls.FindOrAdd(RollId).Shots.Add(MoveTemp(Record));
}

void UPhotoLibrarySubsystem::DevelopRoll(const FGuid& RollId)
{
	FPendingFilmRoll Roll;
	if (!PendingRolls.RemoveAndCopyValue(RollId, Roll))
	{
		UE_LOG(LogTemp, Warning, TEXT("UPhotoLibrarySubsystem::DevelopRoll - no pending roll found for RollId %s"), *RollId.ToString());
		return;
	}

	Photos.Append(MoveTemp(Roll.Shots));
	OnPhotoLibraryChanged.Broadcast();
}

void UPhotoLibrarySubsystem::DeletePhoto(const FGuid& PhotoId)
{
	const int32 RemovedCount = Photos.RemoveAll([&PhotoId](const FPhotoRecord& Record)
	{
		return Record.Id == PhotoId;
	});

	if (RemovedCount > 0)
	{
		OnPhotoLibraryChanged.Broadcast();
	}
}

void UPhotoLibrarySubsystem::ToggleFavorite(const FGuid& PhotoId)
{
	for (FPhotoRecord& Record : Photos)
	{
		if (Record.Id == PhotoId)
		{
			Record.bFavorite = !Record.bFavorite;
			OnPhotoLibraryChanged.Broadcast();
			return;
		}
	}
}
