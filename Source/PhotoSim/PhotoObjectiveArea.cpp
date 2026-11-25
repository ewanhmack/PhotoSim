// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhotoObjectiveArea.h"
#include "PhotoSubjectComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"

APhotoObjectiveArea::APhotoObjectiveArea()
{
	PrimaryActorTick.bCanEverTick = false;

	AreaVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaVolume"));
	RootComponent = AreaVolume;
	AreaVolume->SetBoxExtent(FVector(150.f, 150.f, 100.f));
	// TryCaptureFrom tests the volume by maths against a reported location, not by overlap events,
	// so this never needs to physically interact with anything.
	AreaVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PhotoSubject = CreateDefaultSubobject<UPhotoSubjectComponent>(TEXT("PhotoSubject"));
	PhotoSubject->DisplayName = FText::FromString(TEXT("Objective"));

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(32.f);
	Label->SetText(PhotoSubject->DisplayName);
	Label->SetHiddenInGame(true);
}

void APhotoObjectiveArea::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (Label != nullptr && PhotoSubject != nullptr)
	{
		Label->SetText(PhotoSubject->DisplayName);
		Label->SetHiddenInGame(!bShowLabelInGame);
	}
}

bool APhotoObjectiveArea::TryCaptureFrom(const FVector& CandidateLocation, const FVector& CandidateForward)
{
	if (PhotoSubject == nullptr || PhotoSubject->bIsCaptured)
	{
		return false;
	}

	if (!IsLocationInsideArea(CandidateLocation) || !IsLookingInRightDirection(CandidateForward))
	{
		return false;
	}

	return PhotoSubject->TryCapture();
}

bool APhotoObjectiveArea::IsLocationInsideArea(const FVector& CandidateLocation) const
{
	if (AreaVolume == nullptr)
	{
		return false;
	}

	const FVector LocalPoint = AreaVolume->GetComponentTransform().InverseTransformPosition(CandidateLocation);
	return IsPointInsideBoxExtent(LocalPoint, AreaVolume->GetUnscaledBoxExtent());
}

bool APhotoObjectiveArea::IsLookingInRightDirection(const FVector& CandidateForward) const
{
	return IsWithinFacingTolerance(CandidateForward, GetActorForwardVector(), RequiredFacingToleranceDegrees);
}

bool APhotoObjectiveArea::IsPointInsideBoxExtent(const FVector& LocalPoint, const FVector& Extent)
{
	return FMath::Abs(LocalPoint.X) <= Extent.X
		&& FMath::Abs(LocalPoint.Y) <= Extent.Y
		&& FMath::Abs(LocalPoint.Z) <= Extent.Z;
}

bool APhotoObjectiveArea::IsWithinFacingTolerance(const FVector& CandidateForward, const FVector& RequiredForward, float ToleranceDegrees)
{
	const float CosTolerance = FMath::Cos(FMath::DegreesToRadians(ToleranceDegrees));
	return FVector::DotProduct(CandidateForward.GetSafeNormal(), RequiredForward.GetSafeNormal()) >= CosTolerance;
}
