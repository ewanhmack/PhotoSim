// Copyright Epic Games, Inc. All Rights Reserved.

#include "TP_CameraComponent.h"
#include "PhotoSimCharacter.h"
#include "ViewfinderWidget.h"
#include "CameraAwareWidget.h"
#include "CameraShutterWidget.h"
#include "ExposureModeToastWidget.h"
#include "PhotoLibrarySubsystem.h"
#include "InventoryComponent.h"
#include "LensGridInventoryComponent.h"
#include "LensGridInventoryWidget.h"
#include "PhotoSimGameMode.h"
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
#include "ImageUtils.h"
#include "ImageRotation.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

// EV = log2(N^2 * shutter_denominator) - log2(ISO/100), the same physical relationship
// ApplyExposureSettings assumes for Aperture/ShutterSpeed/ISO - solved for whichever one of the
// three isn't user-fixed in a given exposure mode (see UpdateAutoExposure).
float UTP_CameraComponent::ComputeExposureEV(float ApertureFNumber, float ShutterSpeedDenominator, float ISOValue)
{
	return FMath::Log2(FMath::Square(ApertureFNumber) * ShutterSpeedDenominator) - FMath::Log2(ISOValue / 100.f);
}

float UTP_CameraComponent::SolveShutterSpeedForEV(float ApertureFNumber, float ISOValue, float TargetEVValue)
{
	return FMath::Pow(2.f, TargetEVValue) * (ISOValue / 100.f) / FMath::Square(ApertureFNumber);
}

float UTP_CameraComponent::SolveApertureForEV(float ShutterSpeedDenominator, float ISOValue, float TargetEVValue)
{
	return FMath::Sqrt(FMath::Pow(2.f, TargetEVValue) * (ISOValue / 100.f) / FMath::Max(ShutterSpeedDenominator, KINDA_SMALL_NUMBER));
}

float UTP_CameraComponent::SolveISOForEV(float ApertureFNumber, float ShutterSpeedDenominator, float TargetEVValue)
{
	return 100.f * FMath::Square(ApertureFNumber) * ShutterSpeedDenominator / FMath::Pow(2.f, TargetEVValue);
}

ECameraExposureMode UTP_CameraComponent::GetNextExposureMode(ECameraExposureMode Current, int32 Direction)
{
	static const ECameraExposureMode Order[] = {
		ECameraExposureMode::Manual,
		ECameraExposureMode::AperturePriority,
		ECameraExposureMode::ShutterPriority,
		ECameraExposureMode::ISOPriority,
		ECameraExposureMode::Auto
	};
	constexpr int32 NumModes = UE_ARRAY_COUNT(Order);

	int32 CurrentIndex = 0;
	for (int32 Index = 0; Index < NumModes; ++Index)
	{
		if (Order[Index] == Current)
		{
			CurrentIndex = Index;
			break;
		}
	}

	const int32 NewIndex = ((CurrentIndex + Direction) % NumModes + NumModes) % NumModes;
	return Order[NewIndex];
}

float UTP_CameraComponent::NormalizeRollDegrees(float Degrees)
{
	float Wrapped = FMath::Fmod(Degrees, 360.f);
	if (Wrapped < 0.f)
	{
		Wrapped += 360.f;
	}
	return Wrapped;
}

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

	// Calibrated from the Manual defaults above, so switching into Auto or a priority mode for
	// the first time doesn't jump brightness - see TargetEV's own comment in the header.
	TargetEV = ComputeExposureEV(Aperture, ShutterSpeed, ISO);

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
	static ConstructorHelpers::FObjectFinder<UInputAction> RollModifierActionAsset(TEXT("/Game/PhotoSim/Input/IA_RollCameraModifier.IA_RollCameraModifier"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ExposureModeModifierActionAsset(TEXT("/Game/PhotoSim/Input/IA_ExposureModeModifier.IA_ExposureModeModifier"));
	// Not shipped yet - create these three in-editor (Input Action assets, digital/bool trigger)
	// and bind them to 1/2/3 in IMC_Camera. Guarded like RollModifierAction below, so a checkout
	// without them yet still compiles and runs, just without hotbar hotkeys.
	static ConstructorHelpers::FObjectFinder<UInputAction> HotbarSlot1ActionAsset(TEXT("/Game/PhotoSim/Input/IA_HotbarSlot1.IA_HotbarSlot1"));
	static ConstructorHelpers::FObjectFinder<UInputAction> HotbarSlot2ActionAsset(TEXT("/Game/PhotoSim/Input/IA_HotbarSlot2.IA_HotbarSlot2"));
	static ConstructorHelpers::FObjectFinder<UInputAction> HotbarSlot3ActionAsset(TEXT("/Game/PhotoSim/Input/IA_HotbarSlot3.IA_HotbarSlot3"));

	AimAction = AimActionAsset.Object;
	ZoomAction = ZoomActionAsset.Object;
	PhotoAction = PhotoActionAsset.Object;
	CameraMappingContext = CameraMappingContextAsset.Object;
	ApertureModifierAction = ApertureModifierActionAsset.Object;
	ShutterSpeedModifierAction = ShutterSpeedModifierActionAsset.Object;
	ISOModifierAction = ISOModifierActionAsset.Object;
	FocusModifierAction = FocusModifierActionAsset.Object;
	OpenLensInventoryAction = OpenLensInventoryActionAsset.Object;
	RollModifierAction = RollModifierActionAsset.Object;
	ExposureModeModifierAction = ExposureModeModifierActionAsset.Object;
	HotbarSlot1Action = HotbarSlot1ActionAsset.Object;
	HotbarSlot2Action = HotbarSlot2ActionAsset.Object;
	HotbarSlot3Action = HotbarSlot3ActionAsset.Object;

	// The starter lens is seeded once, on the character (see APhotoSimCharacter::BeginPlay), not
	// here - lenses live in the character's lens bag now, independent of whichever camera (if any)
	// is currently held, so seeding it per-camera would wrongly reset the bag on every pickup.
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
	RefreshCarriedWeight();

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

			// Guarded, unlike the binds above: these actions' assets are created by a one-off
			// editor script rather than shipped from the start, so a project checked out before
			// that script has run would otherwise crash binding a null action.
			if (RollModifierAction != nullptr)
			{
				EnhancedInputComponent->BindAction(RollModifierAction, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_RollModifierStarted);
				EnhancedInputComponent->BindAction(RollModifierAction, ETriggerEvent::Completed, this, &UTP_CameraComponent::Input_RollModifierCompleted);
			}
			if (ExposureModeModifierAction != nullptr)
			{
				EnhancedInputComponent->BindAction(ExposureModeModifierAction, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_ExposureModeModifierStarted);
				EnhancedInputComponent->BindAction(ExposureModeModifierAction, ETriggerEvent::Completed, this, &UTP_CameraComponent::Input_ExposureModeModifierCompleted);
			}
			if (HotbarSlot1Action != nullptr)
			{
				EnhancedInputComponent->BindAction(HotbarSlot1Action, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_HotbarSlot1);
			}
			if (HotbarSlot2Action != nullptr)
			{
				EnhancedInputComponent->BindAction(HotbarSlot2Action, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_HotbarSlot2);
			}
			if (HotbarSlot3Action != nullptr)
			{
				EnhancedInputComponent->BindAction(HotbarSlot3Action, ETriggerEvent::Started, this, &UTP_CameraComponent::Input_HotbarSlot3);
			}
		}
	}

	if (ULensGridInventoryComponent* LensInventory = GetLensInventory())
	{
		LensInventory->OnActiveLensChanged.AddUObject(this, &UTP_CameraComponent::HandleActiveLensChanged);
		ApplyActiveLensFromInventory();
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
			// Starts off; ToggleViewfinder turns it on while aiming so it's already warm by the
			// time TakePhoto fires - see the render target comment just below.
			PhotoCapture->bCaptureEveryFrame = false;
			PhotoCapture->bCaptureOnMovement = false;
			// Without this, the capture's view state (Lumen GI accumulation, TAA history) is torn
			// down after every one-shot CaptureScene(), so each photo renders with zero history -
			// indirectly-lit areas (anything not hit by direct light) read back crushed to black
			// even though the same view looks correctly exposed in the continuously-rendering
			// viewfinder/LCD feed.
			PhotoCapture->bAlwaysPersistRenderingState = true;
			PhotoCapture->CaptureSource = SCS_FinalColorLDR;
			PhotoCapture->PostProcessBlendWeight = 1.f;
			ApplyExposureSettings(PhotoCapture->PostProcessSettings, /*bIsSceneCapture=*/ true);
			ApplyCaptureLumenOverride(PhotoCapture->PostProcessSettings);

			// A persistent target, never recreated per shot, so bAlwaysPersistRenderingState has
			// something stable to accumulate history into. TakePhoto used to spin up a brand new
			// render target on every single press and capture into it exactly once - with no prior
			// frames rendered into THAT target, Lumen/TAA had nothing to persist from regardless of
			// the flag above, which is why photos still came out darker than the viewfinder.
			UTextureRenderTarget2D* CaptureTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
			CaptureTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
			CaptureTarget->InitAutoFormat(PhotoResolution.X, PhotoResolution.Y);
			CaptureTarget->UpdateResourceImmediate(true);
			PhotoCapture->TextureTarget = CaptureTarget;

			PhotoCapture->RegisterComponent();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Camera] Attached to %s and bound Aim/Zoom/Photo input."), *Character->GetName());

	return true;
}

void UTP_CameraComponent::ConfigureFromBodyDefinition(const FCameraBodyDefinition& InBodyDefinition)
{
	CurrentBodyDefinition = InBodyDefinition;
	SensorWidthMM = InBodyDefinition.SensorWidthMM;

	ShotsTakenOnRoll = 0;
	bRollComplete = false;
	CurrentRollId = InBodyDefinition.MediumType == ECameraMediumType::Film ? FGuid::NewGuid() : FGuid();

	RefreshCarriedWeight();

	UE_LOG(LogTemp, Log, TEXT("[Camera] Configured body: %s"), *InBodyDefinition.Name.ToString());
}

void UTP_CameraComponent::RefreshCarriedWeight()
{
	// Lens weight is tracked independently by ULensGridInventoryComponent, under itself as the
	// weight source - the camera body is the only weight this component contributes.
	if (Character == nullptr)
	{
		return;
	}

	if (UInventoryComponent* Inventory = Character->GetInventoryComponent())
	{
		Inventory->RegisterWeight(this, CurrentBodyDefinition.WeightKG);
	}
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
	ApplyExposureSettings(LensCapture->PostProcessSettings, /*bIsSceneCapture=*/ true);
	ApplyCaptureLumenOverride(LensCapture->PostProcessSettings);
	LensCapture->RegisterComponent();

	// Kept small - this renders every frame while the camera is equipped.
	LiveViewTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	LiveViewTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
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

	if (PhotoCapture != nullptr)
	{
		// Only pay for the extra render while aiming - also the only time TakePhoto can fire
		// with bRequireViewfinderToShoot on - so it's warmed up by the time the shutter is pressed.
		PhotoCapture->bCaptureEveryFrame = bIsAiming;
	}

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
				else if (ViewfinderWidgetInstance->GetClass()->ImplementsInterface(UCameraAwareWidget::StaticClass()))
				{
					// Lets a pure-Blueprint ViewfinderWidgetClass (not a UViewfinderWidget subclass)
					// get the camera too, without needing to fish for it itself - see CameraAwareWidget.h.
					ICameraAwareWidget::Execute_SetOwningCamera(ViewfinderWidgetInstance, this);
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

float UTP_CameraComponent::GetRollDegrees() const
{
	return CurrentRollDegrees;
}

float UTP_CameraComponent::GetFrameScaleForRoll(float RollDegrees, FVector2D ViewportSize)
{
	const float W = ViewportSize.X;
	const float H = ViewportSize.Y;
	if (W <= 0.f || H <= 0.f)
	{
		return 1.f;
	}

	const float CosTheta = FMath::Abs(FMath::Cos(FMath::DegreesToRadians(RollDegrees)));
	const float SinTheta = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(RollDegrees)));

	// The largest scale that keeps a screen-shaped WxH rectangle, rotated by RollDegrees, entirely
	// within the WxH screen - the classic "largest rotated rectangle inscribed in a rectangle"
	// problem. Whichever axis would overflow first (width or height) is the one that binds.
	const float ScaleForWidth = W / (W * CosTheta + H * SinTheta);
	const float ScaleForHeight = H / (H * CosTheta + W * SinTheta);
	return FMath::Min(ScaleForWidth, ScaleForHeight);
}

ULensGridInventoryComponent* UTP_CameraComponent::GetLensInventory() const
{
	return Character != nullptr ? Character->GetLensInventoryComponent() : nullptr;
}

void UTP_CameraComponent::ApplyActiveLensFromInventory()
{
	ULensGridInventoryComponent* LensInventory = GetLensInventory();
	if (LensInventory == nullptr)
	{
		return;
	}

	const FGuid ActiveLensId = LensInventory->GetHotbarSlot(LensInventory->GetActiveHotbarSlotIndex());
	const FLensDefinition* Lens = LensInventory->FindDefinition(ActiveLensId);
	if (Lens == nullptr)
	{
		// Empty active hotbar slot - leave the dials exactly as they are rather than clearing them.
		return;
	}

	MinAperture = Lens->MinAperture;
	MaxAperture = Lens->MaxAperture;
	MinFocalLengthMM = Lens->MinFocalLengthMM;
	MaxFocalLengthMM = Lens->MaxFocalLengthMM;
	ZoomStepMM = Lens->ZoomStepMM;
	MinFocusDistance = Lens->MinFocusDistanceCM;

	Aperture = FMath::Clamp(Aperture, MinAperture, MaxAperture);
	FocalDistance = FMath::Clamp(FocalDistance, MinFocusDistance, MaxFocusDistance);
	ZoomAlpha = 0.f;

	UE_LOG(LogTemp, Log, TEXT("[Camera] Active lens: %s"), *Lens->Name.ToString());
}

void UTP_CameraComponent::HandleActiveLensChanged(FGuid NewActiveLensId)
{
	ApplyActiveLensFromInventory();
}

void UTP_CameraComponent::SetActiveHotbarSlotFromInput(int32 SlotIndex)
{
	if (ULensGridInventoryComponent* LensInventory = GetLensInventory())
	{
		LensInventory->SetActiveHotbarSlot(SlotIndex);
	}
}

void UTP_CameraComponent::Input_HotbarSlot1(const FInputActionValue& Value) { SetActiveHotbarSlotFromInput(0); }
void UTP_CameraComponent::Input_HotbarSlot2(const FInputActionValue& Value) { SetActiveHotbarSlotFromInput(1); }
void UTP_CameraComponent::Input_HotbarSlot3(const FInputActionValue& Value) { SetActiveHotbarSlotFromInput(2); }

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
			WidgetClass = ULensGridInventoryWidget::StaticClass();
		}

		if (LensInventoryWidgetInstance == nullptr)
		{
			LensInventoryWidgetInstance = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
			if (ULensGridInventoryWidget* LensInventoryWidget = Cast<ULensGridInventoryWidget>(LensInventoryWidgetInstance))
			{
				LensInventoryWidget->SetOwningCharacter(Character);
			}
		}

		if (LensInventoryWidgetInstance != nullptr)
		{
			LensInventoryWidgetInstance->AddToViewport(150);
			PlayerController->SetShowMouseCursor(true);
			// GameAndUI, not UIOnly: UIOnly stops Enhanced Input from reaching the game at all, so
			// the "I" binding that opened this couldn't fire again to close it.
			PlayerController->SetInputMode(FInputModeGameAndUI().SetWidgetToFocus(LensInventoryWidgetInstance->TakeWidget()).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
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

void UTP_CameraComponent::CycleExposureMode(int32 Direction)
{
	ExposureMode = GetNextExposureMode(ExposureMode, Direction);

	// Recalibrate from whatever's on the dials right now, so the first frame of Auto/a priority
	// mode matches current brightness exactly instead of jumping to a stale TargetEV.
	if (ExposureMode != ECameraExposureMode::Manual)
	{
		TargetEV = ComputeExposureEV(Aperture, ShutterSpeed, ISO);
	}

	ShowExposureModeToast();
	UE_LOG(LogTemp, Log, TEXT("[Camera] Exposure mode: %s"), *GetExposureModeDisplayText().ToString());
}

FText UTP_CameraComponent::GetExposureModeDisplayText() const
{
	switch (ExposureMode)
	{
	case ECameraExposureMode::Manual: return FText::FromString(TEXT("MANUAL"));
	case ECameraExposureMode::AperturePriority: return FText::FromString(TEXT("APERTURE PRIORITY"));
	case ECameraExposureMode::ShutterPriority: return FText::FromString(TEXT("SHUTTER PRIORITY"));
	case ECameraExposureMode::ISOPriority: return FText::FromString(TEXT("ISO PRIORITY"));
	case ECameraExposureMode::Auto: return FText::FromString(TEXT("AUTO"));
	default: return FText::GetEmpty();
	}
}

void UTP_CameraComponent::ShowExposureModeToast()
{
	APlayerController* PlayerController = Character != nullptr ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (PlayerController == nullptr)
	{
		return;
	}

	if (ExposureModeToastWidgetInstance == nullptr)
	{
		TSubclassOf<UUserWidget> WidgetClass = ExposureModeToastWidgetClass;
		if (WidgetClass == nullptr)
		{
			WidgetClass = UExposureModeToastWidget::StaticClass();
		}

		UUserWidget* NewWidget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
		ExposureModeToastWidgetInstance = Cast<UExposureModeToastWidget>(NewWidget);
		if (ExposureModeToastWidgetInstance != nullptr)
		{
			ExposureModeToastWidgetInstance->AddToViewport(90);
		}
	}

	if (ExposureModeToastWidgetInstance != nullptr)
	{
		ExposureModeToastWidgetInstance->ShowMode(GetExposureModeDisplayText());
	}
}

void UTP_CameraComponent::TakePhoto()
{
	if (Character == nullptr || (bRequireViewfinderToShoot && !bIsAiming))
	{
		return;
	}

	if (CurrentBodyDefinition.MediumType == ECameraMediumType::Film && ShotsTakenOnRoll >= CurrentBodyDefinition.FilmRollCapacity)
	{
		UE_LOG(LogTemp, Log, TEXT("[Camera] Film roll is full (%d/%d) - develop it before shooting again."), ShotsTakenOnRoll, CurrentBodyDefinition.FilmRollCapacity);
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
	if (PlayerController == nullptr)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Camera] Photo taken."));

	// Objective areas care about where this shot was taken from and which way the camera was
	// facing, not what's in focus - see APhotoObjectiveArea::TryCaptureFrom.
	if (UCameraComponent* ObjectiveCheckCamera = Character->GetFirstPersonCameraComponent())
	{
		if (APhotoSimGameMode* PhotoGameMode = GetWorld()->GetAuthGameMode<APhotoSimGameMode>())
		{
			PhotoGameMode->TryCaptureObjectiveAt(Character->GetActorLocation(), ObjectiveCheckCamera->GetForwardVector());
		}
	}

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
	if (PhotoCapture != nullptr && PhotoCapture->TextureTarget != nullptr)
	{
		if (UCameraComponent* PlayerCamera = Character->GetFirstPersonCameraComponent())
		{
			PhotoCapture->FOVAngle = PlayerCamera->FieldOfView;
		}
		ApplyExposureSettings(PhotoCapture->PostProcessSettings, /*bIsSceneCapture=*/ true);

		// The 3D capture always renders into the SAME fixed PhotoResolution-shaped buffer, like a
		// real sensor's physical dimensions never changing - only the capture's ORIENTATION rolls
		// with the camera. Rolling it is what actually gives a true portrait FOV (more vertical
		// coverage, less horizontal) rather than just a reshaped crop of the normal landscape view:
		// the render target's fixed aspect still defines FOVAngle's "horizontal", but that local
		// axis now points along world-vertical once the capture itself is tilted 90 degrees.
		//
		// That capture comes out with its content tilted the opposite way the sensor rolled (an
		// FRotator Roll of +N degrees makes the rendered content appear rotated N degrees
		// counter-clockwise), the same way an unprocessed photo from a physically-rotated real
		// camera would - so it still needs counter-rotating to read upright. RotateImageExpandCanvas
		// rotates clockwise for a positive angle, so passing the same +CurrentRollDegrees here
		// cancels the capture's counter-clockwise tilt exactly, and resizes the canvas to the
		// rotated bounding box while it's at it - a clean dimension swap at 90/270, a padded
		// diamond at any angle between.
		PhotoCapture->SetRelativeRotation(FRotator(0.f, 0.f, CurrentRollDegrees));

		// One more capture right at the shutter press, on top of whatever Lumen/TAA history this
		// already has from rendering every frame while aiming (see ToggleViewfinder) - the target
		// itself is the same persistent one created in AttachCamera, never recreated per shot, so
		// that history actually carries over instead of starting cold each time.
		PhotoCapture->CaptureScene();

		UTextureRenderTarget2D* Snapshot = PhotoCapture->TextureTarget;
		TArray<FColor> CapturedPixels;
		Snapshot->GameThread_GetRenderTargetResource()->ReadPixels(CapturedPixels);

		TArray<FColor> RotatedPixels;
		FIntPoint RotatedSize;
		PhotoSim::RotateImageExpandCanvas(CapturedPixels, PhotoResolution, CurrentRollDegrees, FColor::Black, RotatedPixels, RotatedSize);

		// SCS_FinalColorLDR (the capture's own setting) is already gamma-encoded, display-ready
		// colour, so this stays sRGB end to end - the same fix that stopped photos coming out
		// with shadows crushed to black when this used to go through a plain RTF_RGBA8 target.
		UTexture2D* Photo = FImageUtils::CreateTexture2DFromImage(FImageView(RotatedPixels.GetData(), RotatedSize.X, RotatedSize.Y, EGammaSpace::sRGB));

		// Read off the dials at the exact moment of the shot, same as the pixels above - these
		// are what the gallery's playback view reads back to show "what this was taken with".
		FPhotoMetadata Metadata;
		Metadata.Aperture = Aperture;
		Metadata.ShutterSpeed = ShutterSpeed;
		Metadata.ISO = ISO;
		Metadata.FocalLengthMM = GetFocalLengthMM();
		Metadata.CameraBodyName = CurrentBodyDefinition.Name.ToString();
		Metadata.Timestamp = FDateTime::Now();

		if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this))
		{
			if (UPhotoLibrarySubsystem* Library = GameInstance->GetSubsystem<UPhotoLibrarySubsystem>())
			{
				if (CurrentBodyDefinition.MediumType == ECameraMediumType::Film)
				{
					// Doesn't appear in the gallery until the whole roll is developed - see
					// UPhotoLibrarySubsystem::DevelopRoll. The PNG export below still happens
					// per-shot regardless; the reveal mechanic is purely about the in-game gallery.
					Library->AddPendingFilmShot(CurrentRollId, Photo, Metadata);
					++ShotsTakenOnRoll;
					if (ShotsTakenOnRoll >= CurrentBodyDefinition.FilmRollCapacity)
					{
						Library->DevelopRoll(CurrentRollId);
						bRollComplete = true;
					}
				}
				else
				{
					Library->AddPhoto(Photo, Metadata);
				}
			}
		}

		// Save the exact same (rotated) pixels shown in the in-game gallery out to disk, rather
		// than using HighResShot (which re-renders the viewport through a separate path with its
		// own global state and had come out with drastically different exposure).
		const FString PhotoDir = FPaths::ProjectSavedDir() / TEXT("Photos");
		IFileManager::Get().MakeDirectory(*PhotoDir, true);
		const FString FilePath = PhotoDir / FString::Printf(TEXT("Photo_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));

		TArray64<uint8> PNGBytes;
		FImageUtils::PNGCompressImageArray(RotatedSize.X, RotatedSize.Y, RotatedPixels, PNGBytes);
		if (FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FilePath))
		{
			FileWriter->Serialize(PNGBytes.GetData(), PNGBytes.Num());
			FileWriter->Close();
			delete FileWriter;
		}
	}
}

void UTP_CameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Character == nullptr)
	{
		return;
	}

	const FTransform BasePose = bIsAiming ? ViewfinderPose : HipPose;
	// Roll is composed onto the base pose's rotation (about the component's own forward axis)
	// rather than given its own interpolation - this reuses the exact Slerp/RotationAlpha blend
	// already easing the hip<->viewfinder transition, so a 90-degree roll swing eases the same way.
	const FQuat RollQuat(FRotator(0.f, 0.f, GetRollDegrees()));
	const FTransform TargetPose(BasePose.GetRotation() * RollQuat, BasePose.GetLocation());

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

	UpdateAutoExposure();

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
		ApplyExposureSettings(LensCapture->PostProcessSettings, /*bIsSceneCapture=*/ true);
	}

	// Kept in sync every tick, not just at shot time, so whatever Lumen/TAA history it's been
	// accumulating while bCaptureEveryFrame is on (see ToggleViewfinder) matches the roll/FOV/
	// exposure the shot actually gets taken with, instead of snapping to new values the one frame
	// of the shutter press and losing convergence right when it matters.
	if (PhotoCapture != nullptr)
	{
		if (UCameraComponent* PlayerCamera = Character->GetFirstPersonCameraComponent())
		{
			PhotoCapture->FOVAngle = PlayerCamera->FieldOfView;
		}
		PhotoCapture->SetRelativeRotation(FRotator(0.f, 0.f, GetRollDegrees()));
		ApplyExposureSettings(PhotoCapture->PostProcessSettings, /*bIsSceneCapture=*/ true);
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

	if (ExposureModeToastWidgetInstance != nullptr)
	{
		ExposureModeToastWidgetInstance->RemoveFromParent();
		ExposureModeToastWidgetInstance = nullptr;
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
	const float Delta = Value.Get<float>();

	// Roll works even at hip, unlike every other modifier below - you can twist the camera to
	// portrait in your hands before raising it to your eye, so this is checked before the aiming
	// gate rather than inside the aiming-gated branches further down.
	if (bRollModifierHeld)
	{
		CurrentRollDegrees = NormalizeRollDegrees(CurrentRollDegrees + (Delta > 0.f ? RollStepDegrees : -RollStepDegrees));
		return;
	}

	if (!bIsAiming)
	{
		return;
	}

	// Holding a modifier repurposes scroll to adjust that exposure stat instead of zooming. In a
	// non-Manual exposure mode, whichever stat that mode solves for is skipped here - otherwise
	// the user's scroll would get silently overwritten by UpdateAutoExposure on the very next tick.
	const bool bApertureIsSolved = (ExposureMode == ECameraExposureMode::ShutterPriority || ExposureMode == ECameraExposureMode::Auto);
	const bool bShutterIsSolved = (ExposureMode == ECameraExposureMode::AperturePriority || ExposureMode == ECameraExposureMode::ISOPriority || ExposureMode == ECameraExposureMode::Auto);
	const bool bISOIsSolved = (ExposureMode == ECameraExposureMode::Auto);

	if (bApertureModifierHeld)
	{
		if (!bApertureIsSolved)
		{
			Aperture = FMath::Clamp(Aperture + Delta * ApertureStep, MinAperture, MaxAperture);
		}
	}
	else if (bShutterSpeedModifierHeld)
	{
		if (!bShutterIsSolved)
		{
			ShutterSpeed = FMath::Clamp(ShutterSpeed + Delta * ShutterSpeedStep, MinShutterSpeed, MaxShutterSpeed);
		}
	}
	else if (bISOModifierHeld)
	{
		if (!bISOIsSolved)
		{
			ISO = FMath::Clamp(ISO + Delta * ISOStep, MinISO, MaxISO);
		}
	}
	else if (bFocusModifierHeld)
	{
		FocalDistance = FMath::Clamp(FocalDistance + Delta * FocusStep, MinFocusDistance, MaxFocusDistance);
	}
	else if (bExposureModeModifierHeld)
	{
		CycleExposureMode(Delta > 0.f ? 1 : -1);
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

void UTP_CameraComponent::Input_RollModifierStarted(const FInputActionValue& Value)
{
	bRollModifierHeld = true;
}

void UTP_CameraComponent::Input_RollModifierCompleted(const FInputActionValue& Value)
{
	bRollModifierHeld = false;
}

void UTP_CameraComponent::Input_ExposureModeModifierStarted(const FInputActionValue& Value)
{
	bExposureModeModifierHeld = true;
}

void UTP_CameraComponent::Input_ExposureModeModifierCompleted(const FInputActionValue& Value)
{
	bExposureModeModifierHeld = false;
}

void UTP_CameraComponent::ApplyExposureSettings(FPostProcessSettings& PPS, bool bIsSceneCapture) const
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

	// See CaptureExposureCompensationEV's comment - a scene capture needs an extra nudge to match
	// what the real viewport shows, even with every other setting here identical.
	if (bIsSceneCapture)
	{
		PPS.AutoExposureBias += CaptureExposureCompensationEV;
	}

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

void UTP_CameraComponent::ApplyCaptureLumenOverride(FPostProcessSettings& PPS)
{
	PPS.bOverride_DynamicGlobalIlluminationMethod = true;
	PPS.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;

	PPS.bOverride_ReflectionMethod = true;
	PPS.ReflectionMethod = EReflectionMethod::Lumen;

	// SceneCaptureRendering.cpp also force-halves this for every non-cube scene capture unless
	// overridden here - a coarser surface cache under-samples indirect lighting, reading back
	// dimmer than the main view even with GI switched back on above.
	PPS.bOverride_LumenSurfaceCacheResolution = true;
	PPS.LumenSurfaceCacheResolution = 1.f;
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
		FocusedActor = Hit.GetActor();
	}
	else
	{
		FocalDistance = MaxFocusDistance;
		FocusedActor = nullptr;
	}
}

void UTP_CameraComponent::UpdateAutoExposure()
{
	switch (ExposureMode)
	{
	case ECameraExposureMode::Manual:
		return;

	case ECameraExposureMode::AperturePriority:
	case ECameraExposureMode::ISOPriority:
		// Both fix Aperture and ISO (whichever the player last set either one to) and solve
		// ShutterSpeed - see the header's note on TargetEV/ExposureMode for why these two modes
		// end up mechanically identical here despite differing in which dial the player thinks
		// of as "the one they're prioritizing."
		ShutterSpeed = FMath::Clamp(SolveShutterSpeedForEV(Aperture, ISO, TargetEV), MinShutterSpeed, MaxShutterSpeed);
		break;

	case ECameraExposureMode::ShutterPriority:
		Aperture = FMath::Clamp(SolveApertureForEV(ShutterSpeed, ISO, TargetEV), MinAperture, MaxAperture);
		break;

	case ECameraExposureMode::Auto:
		{
			// Deliberate shutter-then-ISO simplification, not a true multi-variable optimizer:
			// prefer the lowest ISO, only letting it float once ShutterSpeed alone can't reach
			// TargetEV within its own range.
			ISO = MinISO;
			const float CandidateShutterSpeed = SolveShutterSpeedForEV(Aperture, ISO, TargetEV);
			if (CandidateShutterSpeed < MinShutterSpeed || CandidateShutterSpeed > MaxShutterSpeed)
			{
				ShutterSpeed = FMath::Clamp(CandidateShutterSpeed, MinShutterSpeed, MaxShutterSpeed);
				ISO = FMath::Clamp(SolveISOForEV(Aperture, ShutterSpeed, TargetEV), MinISO, MaxISO);
			}
			else
			{
				ShutterSpeed = CandidateShutterSpeed;
			}
		}
		break;
	}
}
