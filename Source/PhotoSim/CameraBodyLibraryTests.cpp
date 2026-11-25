// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CameraBodyDefinition.h"
#include "PhotoLibrarySubsystem.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"

namespace
{
	UTexture2D* MakeTestPhoto()
	{
		return NewObject<UTexture2D>();
	}

	FPhotoMetadata MakeTestMetadata()
	{
		FPhotoMetadata Metadata;
		Metadata.Aperture = 2.8f;
		Metadata.ShutterSpeed = 125.f;
		Metadata.ISO = 100.f;
		Metadata.FocalLengthMM = 50.f;
		Metadata.CameraBodyName = TEXT("Test Body");
		Metadata.Timestamp = FDateTime(2026, 1, 1, 12, 0, 0);
		return Metadata;
	}

	// UPhotoLibrarySubsystem is ClassWithin=GameInstance - constructing it with no Outer (or a
	// non-GameInstance Outer) trips an ensure in UObjectGlobals and leaves the object invalid.
	// A bare UGameInstance satisfies that constraint without needing the heavier Init() path,
	// since these tests call the subsystem's own methods directly rather than going through
	// GameInstance::GetSubsystem<T>() lookup.
	UPhotoLibrarySubsystem* CreateTestPhotoLibrary()
	{
		UGameInstance* DummyGameInstance = NewObject<UGameInstance>();
		return NewObject<UPhotoLibrarySubsystem>(DummyGameInstance);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhotoLibraryPendingShotTest, "PhotoSim.PhotoLibrary.AddPendingFilmShotDoesNotAffectPhotosOrBroadcast", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPhotoLibraryPendingShotTest::RunTest(const FString& Parameters)
{
	UPhotoLibrarySubsystem* const Library = CreateTestPhotoLibrary();

	int32 BroadcastCount = 0;
	Library->OnPhotoLibraryChanged.AddLambda([&BroadcastCount]() { ++BroadcastCount; });

	const FGuid RollId = FGuid::NewGuid();
	Library->AddPendingFilmShot(RollId, MakeTestPhoto(), MakeTestMetadata());
	Library->AddPendingFilmShot(RollId, MakeTestPhoto(), MakeTestMetadata());

	TestEqual(TEXT("GetPhotos should stay empty while a roll is pending"), Library->GetPhotos().Num(), 0);
	TestEqual(TEXT("OnPhotoLibraryChanged should not fire for pending shots"), BroadcastCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhotoLibraryDevelopRollOrderTest, "PhotoSim.PhotoLibrary.DevelopRollMovesAllShotsInOrder", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPhotoLibraryDevelopRollOrderTest::RunTest(const FString& Parameters)
{
	UPhotoLibrarySubsystem* const Library = CreateTestPhotoLibrary();

	int32 BroadcastCount = 0;
	Library->OnPhotoLibraryChanged.AddLambda([&BroadcastCount]() { ++BroadcastCount; });

	const FGuid RollId = FGuid::NewGuid();
	UTexture2D* const First = MakeTestPhoto();
	UTexture2D* const Second = MakeTestPhoto();
	UTexture2D* const Third = MakeTestPhoto();

	Library->AddPendingFilmShot(RollId, First, MakeTestMetadata());
	Library->AddPendingFilmShot(RollId, Second, MakeTestMetadata());
	Library->AddPendingFilmShot(RollId, Third, MakeTestMetadata());

	Library->DevelopRoll(RollId);

	const TArray<FPhotoRecord>& Photos = Library->GetPhotos();
	TestEqual(TEXT("All three pending shots should land in Photos"), Photos.Num(), 3);
	if (Photos.Num() == 3)
	{
		TestTrue(TEXT("First shot should keep its position"), Photos[0].Texture == First);
		TestTrue(TEXT("Second shot should keep its position"), Photos[1].Texture == Second);
		TestTrue(TEXT("Third shot should keep its position"), Photos[2].Texture == Third);
	}
	TestEqual(TEXT("OnPhotoLibraryChanged should fire exactly once"), BroadcastCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhotoLibraryDevelopRollClearsPendingTest, "PhotoSim.PhotoLibrary.DevelopRollClearsPendingEntry", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPhotoLibraryDevelopRollClearsPendingTest::RunTest(const FString& Parameters)
{
	UPhotoLibrarySubsystem* const Library = CreateTestPhotoLibrary();

	const FGuid RollId = FGuid::NewGuid();
	Library->AddPendingFilmShot(RollId, MakeTestPhoto(), MakeTestMetadata());
	Library->DevelopRoll(RollId);

	int32 BroadcastCount = 0;
	Library->OnPhotoLibraryChanged.AddLambda([&BroadcastCount]() { ++BroadcastCount; });

	// Developing the same RollId again should now hit the unknown-roll path, not re-develop it.
	Library->DevelopRoll(RollId);

	TestEqual(TEXT("Photos should still only contain the one shot from the first develop"), Library->GetPhotos().Num(), 1);
	TestEqual(TEXT("Re-developing a cleared roll should not broadcast"), BroadcastCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhotoLibraryDevelopUnknownRollTest, "PhotoSim.PhotoLibrary.DevelopRollUnknownRollIdDoesNotCrashOrBroadcast", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPhotoLibraryDevelopUnknownRollTest::RunTest(const FString& Parameters)
{
	UPhotoLibrarySubsystem* const Library = CreateTestPhotoLibrary();

	int32 BroadcastCount = 0;
	Library->OnPhotoLibraryChanged.AddLambda([&BroadcastCount]() { ++BroadcastCount; });

	Library->DevelopRoll(FGuid::NewGuid());

	TestEqual(TEXT("Photos should stay empty when the RollId was never staged"), Library->GetPhotos().Num(), 0);
	TestEqual(TEXT("An unknown RollId should not broadcast"), BroadcastCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhotoLibraryMetadataRoundTripTest, "PhotoSim.PhotoLibrary.AddPhotoKeepsMetadataAndAssignsStableId", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPhotoLibraryMetadataRoundTripTest::RunTest(const FString& Parameters)
{
	UPhotoLibrarySubsystem* const Library = CreateTestPhotoLibrary();

	const FPhotoMetadata Metadata = MakeTestMetadata();
	Library->AddPhoto(MakeTestPhoto(), Metadata);

	const TArray<FPhotoRecord>& Photos = Library->GetPhotos();
	TestEqual(TEXT("One photo should have been added"), Photos.Num(), 1);
	if (Photos.Num() == 1)
	{
		TestTrue(TEXT("The stable Id should be valid"), Photos[0].Id.IsValid());
		TestEqual(TEXT("Aperture should round-trip"), Photos[0].Metadata.Aperture, Metadata.Aperture);
		TestEqual(TEXT("ShutterSpeed should round-trip"), Photos[0].Metadata.ShutterSpeed, Metadata.ShutterSpeed);
		TestEqual(TEXT("ISO should round-trip"), Photos[0].Metadata.ISO, Metadata.ISO);
		TestEqual(TEXT("FocalLengthMM should round-trip"), Photos[0].Metadata.FocalLengthMM, Metadata.FocalLengthMM);
		TestEqual(TEXT("CameraBodyName should round-trip"), Photos[0].Metadata.CameraBodyName, Metadata.CameraBodyName);
		TestFalse(TEXT("A freshly added photo should not start favorited"), Photos[0].bFavorite);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhotoLibraryToggleFavoriteTest, "PhotoSim.PhotoLibrary.ToggleFavoriteFlipsOnlyTheMatchingPhoto", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPhotoLibraryToggleFavoriteTest::RunTest(const FString& Parameters)
{
	UPhotoLibrarySubsystem* const Library = CreateTestPhotoLibrary();

	Library->AddPhoto(MakeTestPhoto(), MakeTestMetadata());
	Library->AddPhoto(MakeTestPhoto(), MakeTestMetadata());

	const FGuid FirstId = Library->GetPhotos()[0].Id;

	int32 BroadcastCount = 0;
	Library->OnPhotoLibraryChanged.AddLambda([&BroadcastCount]() { ++BroadcastCount; });

	Library->ToggleFavorite(FirstId);

	TestTrue(TEXT("The targeted photo should now be favorited"), Library->GetPhotos()[0].bFavorite);
	TestFalse(TEXT("The other photo should be untouched"), Library->GetPhotos()[1].bFavorite);
	TestEqual(TEXT("Toggling favorite should broadcast"), BroadcastCount, 1);

	Library->ToggleFavorite(FirstId);
	TestFalse(TEXT("Toggling again should clear it"), Library->GetPhotos()[0].bFavorite);

	BroadcastCount = 0;
	Library->ToggleFavorite(FGuid::NewGuid());
	TestEqual(TEXT("An unknown Id should not broadcast"), BroadcastCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhotoLibraryDeletePhotoTest, "PhotoSim.PhotoLibrary.DeletePhotoRemovesOnlyThatPhoto", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FPhotoLibraryDeletePhotoTest::RunTest(const FString& Parameters)
{
	UPhotoLibrarySubsystem* const Library = CreateTestPhotoLibrary();

	Library->AddPhoto(MakeTestPhoto(), MakeTestMetadata());
	Library->AddPhoto(MakeTestPhoto(), MakeTestMetadata());

	const FGuid FirstId = Library->GetPhotos()[0].Id;
	const FGuid SecondId = Library->GetPhotos()[1].Id;

	int32 BroadcastCount = 0;
	Library->OnPhotoLibraryChanged.AddLambda([&BroadcastCount]() { ++BroadcastCount; });

	Library->DeletePhoto(FirstId);

	TestEqual(TEXT("One photo should remain"), Library->GetPhotos().Num(), 1);
	if (Library->GetPhotos().Num() == 1)
	{
		TestTrue(TEXT("The remaining photo should be the second one"), Library->GetPhotos()[0].Id == SecondId);
	}
	TestEqual(TEXT("Deleting should broadcast"), BroadcastCount, 1);

	BroadcastCount = 0;
	Library->DeletePhoto(FirstId);
	TestEqual(TEXT("Photos count should be unchanged"), Library->GetPhotos().Num(), 1);
	TestEqual(TEXT("Deleting an already-removed Id should not broadcast"), BroadcastCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraBodyDefinitionDefaultMeshTest, "PhotoSim.CameraBody.OverrideMeshUnsetLeavesDefaultMeshUnchanged", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCameraBodyDefinitionDefaultMeshTest::RunTest(const FString& Parameters)
{
	FCameraBodyDefinition BodyDefinition;

	TestTrue(TEXT("A freshly constructed FCameraBodyDefinition should have no OverrideMesh set"), BodyDefinition.OverrideMesh.IsNull());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
