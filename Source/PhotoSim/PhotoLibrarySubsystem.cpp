// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhotoLibrarySubsystem.h"
#include "Engine/TextureRenderTarget2D.h"

void UPhotoLibrarySubsystem::AddPhoto(UTextureRenderTarget2D* Photo)
{
	if (Photo == nullptr)
	{
		return;
	}

	Photos.Add(Photo);
	OnPhotoLibraryChanged.Broadcast();
}
