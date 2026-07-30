// Copyright Epic Games, Inc. All Rights Reserved.

#include "TP_CameraComponent.h"
#include "PhotoSimCharacter.h"
#include "ViewfinderWidget.h"
#include "CameraShutterWidget.h"
#include "LensInventoryWidget.h"
#include "PhotoLibrarySubsystem.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

UTP_CameraComponent::UTP_CameraComponent()
	: Character(nullptr)
	, ZoomAlpha(0.f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	bIsAiming = false;
	RaiseInterpSpeed = 12.f;
	DefaultFOV = 90.f;
	SensorWidthMM = 36.f; // full-frame equivalent, so "24mm/50mm/200mm" reads the way people expect
	bRequireViewfinderToShoot = true;
	PhotoResolution = FIntPoint(640, 360);

	// Manual exposure defaults - a bright, slightly overcast daylight scene. Tune to taste.
	Aperture = 4.f;
	MinAperture = 1.4f;
	MaxAperture = 16.f;
	ApertureStep = 0.2f;

	FocalDistance = 300.f;
	MaxFocusDistance = 50000.f; // 500m - effectively "infinity"; MinFocusDistance comes from the equipped lens
	FocusStep = 50.f;

	ShutterSpeed = 125.f;
	MinShutterSpeed = 15.f;
	MaxShutterSpeed = 2000.f;
	ShutterSpeedStep = 25.f;
	MaxMotionBlurAmount = 1.f;

	ISO = 100.f;
	MinISO = 100.f;
	MaxISO = 3200.f;
	ISOStep = 100.f;
	MaxFilmGrainIntensity = 1.f;

	// Starting guess to offset the mismatch between the physical exposure formula and this
	// project's (likely uncalibrated) light intensities. Nudge live in the viewfinder to taste.
	ExposureCompensationEV = 8.f;

	// Both poses are relative to the player's first person camera (see AttachCamera), so X is
	// distance in front of the eye, Y is left(-)/right(+), Z is down(-)/up(+).

	// Held down at the bottom-right of the screen, turned slightly away - a resting pose
	HipPose = FTransform(FRotator(0.f, 0.f, 0.f), FVector(25.f, 15.f, -10.f));
	// Raised and centred in front of the eye, framing through the viewfinder
	ViewfinderPose = FTransform(FRotator(0.f, 0.f, 0.f), FVector(-5.f, 0.f, 0.f));

	// Default the input assets to the real assets shipped in the project (still overridable per
	// Blueprint instance via the Details panel, same as TP_WeaponComponent's FireAction/
	// FireMappingContext). See the project notes for the exact assets each needs to contain.
	static ConstructorHelpers::FObjectFinder<UInputAction> AimActionAsset(TEXT("/Game/PhotoSim/Input/IA_AimCamera.IA_AimCamera"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ZoomActionAsset(TEXT("/Game/PhotoSim/Input/IA_ZoomCamera.IA_ZoomCamera"));
	static ConstructorHelpers::FObjectFinder<UInputAction> PhotoActionAsset(TEXT("/Game/PhotoSim/Input/IA_TakePhoto.IA_TakePhoto"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> CameraMappingContextAsset(TEXT("/Game/PhotoSim/Input/IMC_Camera.IMC_Camera"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ApertureModifierActionAsset(TEXT("/Game/PhotoSim/Input/IA_ApertureModifier.IA_ApertureModifier"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ShutterSpeedModifierActionAsset(TEXT("/Game/PhotoSim/Input/IA_ShutterModifier.IA_ShutterModifier"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ISOModifierActionAsset(TEXT("/Game/PhotoSim/Input/IA_ISOModifier.IA_ISOModifier"));
	static ConstructorHelpers::FObjectFinder<UInputAction> FocusModifierActionAsset(TEXT("/Game/PhotoSim/Input/IA_FocusModifier.IA_FocusModifier"));
	static ConstructorHelpers::FObjectFinder<UInputAction> OpenLensInventoryActionAsset(TEXT("/Game/PhotoSim/Input/IA_OpenLensInventory.IA_OpenLensInventory"));

	AimAction = AimActionAsset.Object;
	ZoomAction = ZoomActionAsset.Object;
	PhotoAction = PhotoActionAsset.Object;
	CameraMappingContext = CameraMappingContextAsset.Object;
	ApertureModifierAction = ApertureModifierActionAsset.Object;
	ShutterSpeedModifierAction = ShutterSpeedModifierActionAsset.Object;
	ISOModifierAction = ISOModifierActionAsset.Object;
	FocusModifierAction = FocusModifierActionAsset.Object;
	OpenLensInventoryAction = OpenLensInventoryActionAsset.Object;

	// Five lenses spanning wide to super-telephoto. Purely data - tweak or add more in the
	// Details panel any time, no code changes needed. Fields: Name, MinAperture, MaxAperture,
	// MinFocalLengthMM, MaxFocalLengthMM, ZoomStepMM, MinFocusDistanceCM.
	AvailableLenses.Add({ FText::FromString(TEXT("14-24mm Wide Zoom")), 2.8f, 16.f, 14.f, 24.f, 1.f, 25.f });
	AvailableLenses.Add({ FText::FromString(TEXT("24-70mm Standard Zoom")), 2.8f, 16.f, 24.f, 70.f, 4.f, 38.f });
	AvailableLenses.Add({ FText::FromString(TEXT("50mm Prime")), 1.4f, 16.f, 50.f, 50.f, 1.f, 45.f });
	AvailableLenses.Add({ FText::FromString(TEXT("70-200mm Telephoto")), 2.8f, 22.f, 70.f, 200.f, 10.f, 100.f });
	AvailableLenses.Add({ FText::FromString(TEXT("200-600mm Super-Telephoto")), 4.f, 22.f, 200.f, 600.f, 40.f, 200.f });
	EquipLens(0);
}

void UTP_CameraComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UTP_CameraComponent::AttachCamera(APhotoSimCharacter* TargetCharacter)
{
	Character = TargetCharacter;

	// Check that the character is valid, and has no camera component yet
	if (Character == nullptr || Character->GetInstanceComponents().FindItemByClass<UTP_CameraComponent>())
	{
		return false;
	}

	// Move ownership from the pickup actor to the character. AddInstanceComponent alone does
	// NOT do this - it only tracks the component in an editor/serialization-facing array. The
	// actual owner (and which actor's OwnedComponents set this lives in) is driven by the
	// component's Outer, which only Rename() updates. Without this, destroying the pickup actor
	// would garbage-collect this component along with it, since Destroy() marks everything still
	// in its OwnedComponents set as garbage.
	Rename(nullptr, Character);

	// Attach directly to the first person camera rather than the hand/GripPoint socket. The
	// GripPoint socket lives on the arms skeleton and inherits its walk-cycle bob/sway, which
	// reads as the camera model drifting around while moving. The camera component itself only
	// rotates with mouselook, so anything attached to it stays visually locked in place on screen.
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(Character->GetFirstPersonCameraComponent(), AttachmentRules);
	SetRelativeLocationAndRotation(HipPose.GetLocation(), HipPose.GetRotation());

	Character->AddInstanceComponent(this);

	// Set up action bindings
	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(CameraMappingContext, 1);
		}

		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
		{
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_ToggleAim);
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &UTP_CameraComponent::Input_Zoom);
			EnhancedInputComponent->BindAction(PhotoAction, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_TakePhoto);

			EnhancedInputComponent->BindAction(ApertureModifierAction, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_ApertureModifierStarted);
			EnhancedInputComponent->BindAction(ApertureModifierAction, ETriggerEvent::Completed, this, &UTP_CameraComponent::Input_ApertureModifierCompleted);
			EnhancedInputComponent->BindAction(ShutterSpeedModifierAction, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_ShutterModifierStarted);
			EnhancedInputComponent->BindAction(ShutterSpeedModifierAction, ETriggerEvent::Completed, this, &UTP_CameraComponent::Input_ShutterModifierCompleted);
			EnhancedInputComponent->BindAction(ISOModifierAction, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_ISOModifierStarted);
			EnhancedInputComponent->BindAction(ISOModifierAction, ETriggerEvent::Completed, this, &UTP_CameraComponent::Input_ISOModifierCompleted);
			EnhancedInputComponent->BindAction(FocusModifierAction, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_FocusModifierStarted);
			EnhancedInputComponent->BindAction(FocusModifierAction, ETriggerEvent::Completed, this, &UTP_CameraComponent::Input_FocusModifierCompleted);
			EnhancedInputComponent->BindAction(OpenLensInventoryAction, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_OpenLensInventory);
		}
	}

	SetComponentTickEnabled(true);

	// Runtime-created capture used to save each photo into the gallery - kept in sync with the
	// player's first person camera so it captures exactly what's on screen at the moment of the shot.
	if (PhotoCapture == nullptr)
	{
		if (UCameraComponent* PlayerCamera = Character->GetFirstPersonCameraComponent())
		{
			PhotoCapture = NewObject<USceneCaptureComponent2D>(Character, TEXT("PhotoCapture"));
			PhotoCapture->SetupAttachment(PlayerCamera);
			PhotoCapture->bCaptureEveryFrame = false;
			PhotoCapture->bCaptureOnMovement = false;
			PhotoCapture->CaptureSource = SCS_FinalColorLDR;
			PhotoCapture->PostProcessBlendWeight = 1.f;
			ApplyExposureSettings(PhotoCapture->PostProcessSettings);
			PhotoCapture->RegisterComponent();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Camera] Attached to %s and bound Aim/Zoom/Photo input."), *Character->GetName());

	return true;
}

void UTP_CameraComponent::SetupLiveViewScreen(UStaticMeshComponent* InScreenMesh, UMaterialInterface* InScreenMaterialBase)
{
	if (InScreenMesh == nullptr || InScreenMaterialBase == nullptr || Character == nullptr || LensCapture != nullptr)
	{
		return;
	}

	// Captures from the lens, facing the same way the model does, so the screen shows what the
	// camera itself is pointed at rather than reusing the player's eye camera.
	LensCapture = NewObject<USceneCaptureComponent2D>(Character, TEXT("LensCapture"));
	LensCapture->SetupAttachment(this);
	LensCapture->SetRelativeLocation(LensCaptureOffset);
	LensCapture->bCaptureEveryFrame = true;
	LensCapture->bCaptureOnMovement = false;
	LensCapture->CaptureSource = SCS_FinalColorLDR;
	LensCapture->FOVAngle = GetFOVForFocalLengthMM(GetFocalLengthMM());
	LensCapture->PostProcessBlendWeight = 1.f;
	ApplyExposureSettings(LensCapture->PostProcessSettings);
	LensCapture->RegisterComponent();

	// Kept small - this renders every frame while the camera is equipped.
	LiveViewTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	LiveViewTarget->RenderTargetFormat = RTF_RGBA8;
	LiveViewTarget->InitAutoFormat(256, 144);
	LiveViewTarget->UpdateResourceImmediate(true);
	LensCapture->TextureTarget = LiveViewTarget;

	UMaterialInstanceDynamic* ScreenMID = UMaterialInstanceDynamic::Create(InScreenMaterialBase, this);
	ScreenMID->SetTextureParameterValue(TEXT("ScreenTexture"), LiveViewTarget);
	InScreenMesh->SetMaterial(0, ScreenMID);
}

void UTP_CameraComponent::ToggleViewfinder()
{
	bIsAiming = !bIsAiming;

	UE_LOG(LogTemp, Log, TEXT("[Camera] Viewfinder %s"), bIsAiming ? TEXT("raised") : TEXT("lowered"));

	if (bIsAiming)
	{
		TSubclassOf<UUserWidget> WidgetClass = ViewfinderWidgetClass;
		if (WidgetClass == nullptr)
		{
			WidgetClass = UViewfinderWidget::StaticClass();
		}

		APlayerController* PlayerController = Character != nullptr ? Cast<APlayerController>(Character->GetController()) : nullptr;
		if (PlayerController != nullptr && ViewfinderWidgetInstance == nullptr)
		{
			ViewfinderWidgetInstance = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
			if (ViewfinderWidgetInstance != nullptr)
			{
				if (UViewfinderWidget* Viewfinder = Cast<UViewfinderWidget>(ViewfinderWidgetInstance))
				{
					Viewfinder->SetOwningCamera(this);
				}
				ViewfinderWidgetInstance->AddToViewport();
			}
		}
	}
	else if (ViewfinderWidgetInstance != nullptr)
	{
		ViewfinderWidgetInstance->RemoveFromParent();
		ViewfinderWidgetInstance = nullptr;
	}
}

float UTP_CameraComponent::GetFOVForFocalLengthMM(float FocalLengthMM) const
{
	const float HalfSensor = SensorWidthMM * 0.5f;
	const float HalfFOVRadians = FMath::Atan(HalfSensor / FMath::Max(FocalLengthMM, 1.f));
	return FMath::RadiansToDegrees(HalfFOVRadians) * 2.f;
}

void UTP_CameraComponent::EquipLens(int32 LensIndex)
{
	if (!AvailableLenses.IsValidIndex(LensIndex))
	{
		return;
	}

	EquippedLensIndex = LensIndex;
	const FLensDefinition& Lens = AvailableLenses[LensIndex];

	MinAperture = Lens.MinAperture;
	MaxAperture = Lens.MaxAperture;
	MinFocalLengthMM = Lens.MinFocalLengthMM;
	MaxFocalLengthMM = Lens.MaxFocalLengthMM;
	ZoomStepMM = Lens.ZoomStepMM;
	MinFocusDistance = Lens.MinFocusDistanceCM;

	Aperture = FMath::Clamp(Aperture, MinAperture, MaxAperture);
	FocalDistance = FMath::Clamp(FocalDistance, MinFocusDistance, MaxFocusDistance);
	ZoomAlpha = 0.f;

	UE_LOG(LogTemp, Log, TEXT("[Camera] Equipped lens: %s"), *Lens.Name.ToString());
}

void UTP_CameraComponent::ToggleLensInventory()
{
	if (Character == nullptr)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
	if (PlayerController == nullptr)
	{
		return;
	}

	// Can't sensibly browse lenses while looking through the viewfinder - close it first.
	if (!bIsLensInventoryOpen && bIsAiming)
	{
		ToggleViewfinder();
	}

	bIsLensInventoryOpen = !bIsLensInventoryOpen;

	if (bIsLensInventoryOpen)
	{
		TSubclassOf<UUserWidget> WidgetClass = LensInventoryWidgetClass;
		if (WidgetClass == nullptr)
		{
			WidgetClass = ULensInventoryWidget::StaticClass();
		}

		if (LensInventoryWidgetInstance == nullptr)
		{
			LensInventoryWidgetInstance = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
			if (ULensInventoryWidget* LensInventory = Cast<ULensInventoryWidget>(LensInventoryWidgetInstance))
			{
				LensInventory->SetOwningCamera(this);
			}
		}

		if (LensInventoryWidgetInstance != nullptr)
		{
			LensInventoryWidgetInstance->AddToViewport(150);
			PlayerController->SetShowMouseCursor(true);
			PlayerController->SetInputMode(FInputModeUIOnly().SetWidgetToFocus(LensInventoryWidgetInstance->TakeWidget()).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
		}
	}
	else
	{
		if (LensInventoryWidgetInstance != nullptr)
		{
			LensInventoryWidgetInstance->RemoveFromParent();
		}
		PlayerController->SetShowMouseCursor(false);
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}

void UTP_CameraComponent::TakePhoto()
{
	if (Character == nullptr || (bRequireViewfinderToShoot && !bIsAiming))
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
	if (PlayerController == nullptr)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Camera] Photo taken."));

	if (ShutterSound != nullptr)
	{
		UGameplayStatics::PlaySound2D(this, ShutterSound);
	}

	// Quick white flash to sell the exposure
	if (PlayerController->PlayerCameraManager != nullptr)
	{
		PlayerController->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.12f, FLinearColor::White, false, true);
	}

	// Shutter-blade wipe overlay
	TSubclassOf<UUserWidget> WidgetClass = ShutterWidgetClass;
	if (WidgetClass == nullptr)
	{
		WidgetClass = UCameraShutterWidget::StaticClass();
	}
	if (ShutterWidgetInstance == nullptr)
	{
		UUserWidget* NewWidget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
		ShutterWidgetInstance = Cast<UCameraShutterWidget>(NewWidget);
		if (ShutterWidgetInstance != nullptr)
		{
			ShutterWidgetInstance->AddToViewport(100);
		}
	}
	if (ShutterWidgetInstance != nullptr)
	{
		ShutterWidgetInstance->PlayShutter();
	}

	if (PhotoAnimation != nullptr)
	{
		if (UAnimInstance* AnimInstance = Character->GetMesh1P()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(PhotoAnimation, 1.f);
		}
	}

	// Capture this shot into the in-game gallery. The scene capture only renders the 3D scene
	// (not the viewfinder HUD overlay), so the saved photo comes out clean.
	if (PhotoCapture != nullptr)
	{
		if (UCameraComponent* PlayerCamera = Character->GetFirstPersonCameraComponent())
		{
			PhotoCapture->FOVAngle = PlayerCamera->FieldOfView;
		}
		ApplyExposureSettings(PhotoCapture->PostProcessSettings);

		UTextureRenderTarget2D* Snapshot = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		Snapshot->RenderTargetFormat = RTF_RGBA8;
		Snapshot->InitAutoFormat(PhotoResolution.X, PhotoResolution.Y);
		Snapshot->UpdateResourceImmediate(true);

		PhotoCapture->TextureTarget = Snapshot;
		PhotoCapture->CaptureScene();

		if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this))
		{
			if (UPhotoLibrarySubsystem* Library = GameInstance->GetSubsystem<UPhotoLibrarySubsystem>())
			{
				Library->AddPhoto(Snapshot);
			}
		}
	}

	PlayerController->ConsoleCommand(TEXT("HighResShot 1"), true);
}

void UTP_CameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Character == nullptr)
	{
		return;
	}

	const FTransform TargetPose = bIsAiming ? ViewfinderPose : HipPose;
	const FVector NewLocation = FMath::VInterpTo(GetRelativeLocation(), TargetPose.GetLocation(), DeltaTime, RaiseInterpSpeed);
	const float RotationAlpha = FMath::Clamp(DeltaTime * RaiseInterpSpeed, 0.f, 1.f);
	const FQuat NewRotation = FQuat::Slerp(GetRelativeRotation().Quaternion(), TargetPose.GetRotation(), RotationAlpha);
	SetRelativeLocationAndRotation(NewLocation, NewRotation);

	// Autofocus tracks whatever's centred in frame, unless the player is manually pulling focus
	// (holding the Focus modifier), in which case Input_Zoom is driving FocalDistance instead.
	if (!bFocusModifierHeld)
	{
		UpdateAutoFocus();
	}

	if (UCameraComponent* PlayerCamera = Character->GetFirstPersonCameraComponent())
	{
		const float TargetFOV = bIsAiming ? GetFOVForFocalLengthMM(GetFocalLengthMM()) : DefaultFOV;
		PlayerCamera->FieldOfView = FMath::FInterpTo(PlayerCamera->FieldOfView, TargetFOV, DeltaTime, RaiseInterpSpeed);

		// Preview the exposure/DOF/motion-blur/grain effect live in the actual game view while
		// looking through the viewfinder, instead of only seeing it on the LCD screen or the
		// final photo. Cleared when not aiming so normal exploration is unaffected.
		if (bIsAiming)
		{
			PlayerCamera->PostProcessBlendWeight = 1.f;
			ApplyExposureSettings(PlayerCamera->PostProcessSettings);
		}
		else
		{
			PlayerCamera->PostProcessBlendWeight = 0.f;
		}
	}

	if (LensCapture != nullptr)
	{
		LensCapture->FOVAngle = GetFOVForFocalLengthMM(GetFocalLengthMM());
		ApplyExposureSettings(LensCapture->PostProcessSettings);
	}
}

void UTP_CameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ViewfinderWidgetInstance != nullptr)
	{
		ViewfinderWidgetInstance->RemoveFromParent();
		ViewfinderWidgetInstance = nullptr;
	}

	if (ShutterWidgetInstance != nullptr)
	{
		ShutterWidgetInstance->RemoveFromParent();
		ShutterWidgetInstance = nullptr;
	}

	if (LensInventoryWidgetInstance != nullptr)
	{
		LensInventoryWidgetInstance->RemoveFromParent();
		LensInventoryWidgetInstance = nullptr;
	}

	if (Character != nullptr)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(CameraMappingContext);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UTP_CameraComponent::Input_ToggleAim(const FInputActionValue& Value)
{
	ToggleViewfinder();
}

void UTP_CameraComponent::Input_Zoom(const FInputActionValue& Value)
{
	if (!bIsAiming)
	{
		return;
	}

	const float Delta = Value.Get<float>();

	// Holding a modifier repurposes scroll to adjust that exposure stat instead of zooming.
	if (bApertureModifierHeld)
	{
		Aperture = FMath::Clamp(Aperture + Delta * ApertureStep, MinAperture, MaxAperture);
	}
	else if (bShutterSpeedModifierHeld)
	{
		ShutterSpeed = FMath::Clamp(ShutterSpeed + Delta * ShutterSpeedStep, MinShutterSpeed, MaxShutterSpeed);
	}
	else if (bISOModifierHeld)
	{
		ISO = FMath::Clamp(ISO + Delta * ISOStep, MinISO, MaxISO);
	}
	else if (bFocusModifierHeld)
	{
		FocalDistance = FMath::Clamp(FocalDistance + Delta * FocusStep, MinFocusDistance, MaxFocusDistance);
	}
	else
	{
		// Step in real mm (ZoomStepMM, tuned per lens) rather than a flat 0-1 alpha step, then
		// convert back to alpha - otherwise every lens took the same relative number of notches
		// to sweep its own range regardless of how many mm that range actually spans.
		const float FocalRange = MaxFocalLengthMM - MinFocalLengthMM;
		if (FocalRange > KINDA_SMALL_NUMBER)
		{
			const float CurrentFocalLength = GetFocalLengthMM();
			const float NewFocalLength = FMath::Clamp(CurrentFocalLength + Delta * ZoomStepMM, MinFocalLengthMM, MaxFocalLengthMM);
			ZoomAlpha = (NewFocalLength - MinFocalLengthMM) / FocalRange;
		}
	}
}

void UTP_CameraComponent::Input_TakePhoto(const FInputActionValue& Value)
{
	TakePhoto();
}

void UTP_CameraComponent::Input_ApertureModifierStarted(const FInputActionValue& Value)
{
	bApertureModifierHeld = true;
}

void UTP_CameraComponent::Input_ApertureModifierCompleted(const FInputActionValue& Value)
{
	bApertureModifierHeld = false;
}

void UTP_CameraComponent::Input_ShutterModifierStarted(const FInputActionValue& Value)
{
	bShutterSpeedModifierHeld = true;
}

void UTP_CameraComponent::Input_ShutterModifierCompleted(const FInputActionValue& Value)
{
	bShutterSpeedModifierHeld = false;
}

void UTP_CameraComponent::Input_ISOModifierStarted(const FInputActionValue& Value)
{
	bISOModifierHeld = true;
}

void UTP_CameraComponent::Input_ISOModifierCompleted(const FInputActionValue& Value)
{
	bISOModifierHeld = false;
}

void UTP_CameraComponent::Input_FocusModifierStarted(const FInputActionValue& Value)
{
	bFocusModifierHeld = true;
	UE_LOG(LogTemp, Log, TEXT("[Camera] Focus modifier held (manual focus)"));
}

void UTP_CameraComponent::Input_FocusModifierCompleted(const FInputActionValue& Value)
{
	bFocusModifierHeld = false;
	UE_LOG(LogTemp, Log, TEXT("[Camera] Focus modifier released (autofocus resumed)"));
}

void UTP_CameraComponent::Input_OpenLensInventory(const FInputActionValue& Value)
{
	ToggleLensInventory();
}

void UTP_CameraComponent::ApplyExposureSettings(FPostProcessSettings& PPS) const
{
	// Brightness - Manual mode computes exposure from ISO/ShutterSpeed/Aperture like a real
	// camera, rather than auto-exposure (which never gets time to adapt for an occasional capture).
	PPS.bOverride_AutoExposureMethod = true;
	PPS.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;

	PPS.bOverride_CameraISO = true;
	PPS.CameraISO = ISO;

	PPS.bOverride_CameraShutterSpeed = true;
	PPS.CameraShutterSpeed = ShutterSpeed;

	PPS.bOverride_DepthOfFieldFstop = true;
	PPS.DepthOfFieldFstop = Aperture;

	// Flat compensation on top of the physical calculation above - see ExposureCompensationEV's
	// comment for why this is necessary (the physical formula assumes real-world light units).
	PPS.bOverride_AutoExposureBias = true;
	PPS.AutoExposureBias = ExposureCompensationEV;

	// Depth of field - wider aperture (lower f-stop) blurs the background more
	PPS.bOverride_DepthOfFieldFocalDistance = true;
	PPS.DepthOfFieldFocalDistance = FocalDistance;

	// Motion blur - slower shutter speed blurs movement more
	const float MotionBlurAmount = FMath::GetMappedRangeValueClamped(
		FVector2D(MinShutterSpeed, MaxShutterSpeed), FVector2D(MaxMotionBlurAmount, 0.f), ShutterSpeed);
	PPS.bOverride_MotionBlurAmount = true;
	PPS.MotionBlurAmount = MotionBlurAmount;

	// Film grain - higher ISO looks grainier
	const float FilmGrain = FMath::GetMappedRangeValueClamped(
		FVector2D(MinISO, MaxISO), FVector2D(0.f, MaxFilmGrainIntensity), ISO);
	PPS.bOverride_FilmGrainIntensity = true;
	PPS.FilmGrainIntensity = FilmGrain;
}

void UTP_CameraComponent::UpdateAutoFocus()
{
	if (Character == nullptr)
	{
		return;
	}

	UCameraComponent* PlayerCamera = Character->GetFirstPersonCameraComponent();
	if (PlayerCamera == nullptr)
	{
		return;
	}

	const FVector Start = PlayerCamera->GetComponentLocation();
	const FVector End = Start + PlayerCamera->GetForwardVector() * MaxFocusDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams))
	{
		FocalDistance = FMath::Clamp(FVector::Dist(Start, Hit.ImpactPoint), MinFocusDistance, MaxFocusDistance);
	}
	else
	{
		FocalDistance = MaxFocusDistance;
	}
}
