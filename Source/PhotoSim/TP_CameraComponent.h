// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TP_CameraComponent.generated.h"

class APhotoSimCharacter;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
class UCameraShutterWidget;
class UAnimMontage;
class USoundBase;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UStaticMeshComponent;
class UMaterialInterface;
struct FInputActionValue;
struct FPostProcessSettings;

/**
 * Handheld, pickup-able camera logic and rig. This component is purely the "brain" of the
 * camera - equip/aim/zoom/photo state, input, and the procedural viewfinder blend. The visual
 * mesh parts live directly on the owning actor (see ACameraPickup) and are attached to this
 * component so they move together with it; keeping them on the actor (rather than nested inside
 * this component) is deliberate so a Blueprint made from that actor reliably inherits them.
 *
 * The "look through the viewfinder" motion is a procedural transform + FOV blend rather than a
 * hand-keyframed animation, so it works without any DCC tool or Editor animation authoring -
 * see RaiseInterpSpeed / HipPose / ViewfinderPose.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PHOTOSIM_API UTP_CameraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTP_CameraComponent();

	/** Attaches the camera prop to a FirstPersonCharacter's hand and enables its controls */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	bool AttachCamera(APhotoSimCharacter* TargetCharacter);

	/** Toggles looking through the viewfinder (aim mode) */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ToggleViewfinder();

	/** Takes a photo. Gated behind bRequireViewfinderToShoot. */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void TakePhoto();

	/** Current 0-1 scroll zoom level, persists across aim toggles */
	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetZoomAlpha() const { return ZoomAlpha; }

	/** Current focal length in mm, interpolated between MinFocalLengthMM and MaxFocalLengthMM by zoom */
	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetFocalLengthMM() const { return FMath::Lerp(MinFocalLengthMM, MaxFocalLengthMM, ZoomAlpha); }

	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetAperture() const { return Aperture; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetShutterSpeed() const { return ShutterSpeed; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetISO() const { return ISO; }

	/**
	 * Wires up the live-view screen on the back of the body: a scene capture positioned at the
	 * lens, continuously rendering into a render target that's fed to InScreenMesh's material as
	 * its "ScreenTexture" parameter. Called by ACameraPickup once this component is attached.
	 */
	void SetupLiveViewScreen(UStaticMeshComponent* InScreenMesh, UMaterialInterface* InScreenMaterialBase);

	/** Where the lens capture sits, relative to this component - line this up with the imported mesh's actual lens */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Photo")
	FVector LensCaptureOffset = FVector(8.6f, 0.f, 0.f);

	// --- Pose tuning (the "animation") ---

	/** Relative transform (to the hand socket) when held down, not looking through the viewfinder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pose")
	FTransform HipPose;

	/** Relative transform when raised to the eye, looking through the viewfinder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pose")
	FTransform ViewfinderPose;

	/** How quickly the camera blends between hip and viewfinder poses, and the FOV blends */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pose")
	float RaiseInterpSpeed;

	// --- Zoom / FOV tuning ---

	/** Player camera FOV when not aiming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float DefaultFOV;

	/** Player camera FOV the instant the viewfinder is raised (scroll at 0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ViewfinderFOV;

	/** Player camera FOV at maximum scroll-in zoom */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float MaxZoomFOV;

	/** How much each scroll notch moves the 0-1 zoom level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomStep;

	/** Displayed focal length (HUD readout) at zero zoom */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float MinFocalLengthMM;

	/** Displayed focal length (HUD readout) at full zoom */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float MaxFocalLengthMM;

	// --- Exposure ---
	// Both captures use Manual exposure driven by these rather than auto-exposure: a capture
	// that only fires occasionally (PhotoCapture, on the shutter) never gets time to temporally
	// adapt the way the main viewport does, so it was rendering from a cold, badly-guessed
	// exposure - hence photos coming out massively underexposed.

	/** Aperture (f-stop). Lower = brighter, shallower depth of field (blurrier background). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float Aperture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float MinAperture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float MaxAperture;

	/** How much holding the Aperture modifier + one scroll notch changes Aperture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float ApertureStep;

	/** How far in front of the lens stays in focus - only matters while depth of field is visibly blurring the background */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float FocalDistance;

	/** Shutter speed as the denominator of 1/x seconds. Lower = brighter and more motion blur. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float ShutterSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float MinShutterSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float MaxShutterSpeed;

	/** How much holding the Shutter modifier + one scroll notch changes ShutterSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float ShutterSpeedStep;

	/** Motion blur amount at the slowest shutter speed (MinShutterSpeed); 0 at the fastest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float MaxMotionBlurAmount;

	/** Sensor sensitivity. Higher = brighter and grainier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float ISO;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float MinISO;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float MaxISO;

	/** How much holding the ISO modifier + one scroll notch changes ISO */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float ISOStep;

	/** Film grain intensity at the highest ISO (MaxISO); 0 at the lowest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float MaxFilmGrainIntensity;

	/**
	 * Flat brightness offset (in EV, i.e. camera stops) added on top of the physical
	 * Aperture/ShutterSpeed/ISO exposure calculation. That calculation assumes the level's
	 * lighting is calibrated to real-world photometric units, which most levels (including
	 * ones built from a landscape/template starting point) aren't - so this exists to bring
	 * brightness in line with how the scene is actually lit, independent of the physical model.
	 * Positive = brighter. Tune this live while looking through the viewfinder.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Exposure")
	float ExposureCompensationEV;

	// --- Behaviour / feedback ---

	/** If true, TakePhoto only works while looking through the viewfinder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bRequireViewfinderToShoot;

	/** Optional shutter sound, played on TakePhoto */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Feedback")
	TObjectPtr<USoundBase> ShutterSound;

	/** Optional arm montage played on TakePhoto. Purely cosmetic; leave empty to skip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Feedback")
	TObjectPtr<UAnimMontage> PhotoAnimation;

	/** Optional widget class for the viewfinder overlay HUD. Defaults to UViewfinderWidget if left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Feedback")
	TSubclassOf<UUserWidget> ViewfinderWidgetClass;

	/** Optional widget class for the shutter-close flash. Defaults to UCameraShutterWidget if left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Feedback")
	TSubclassOf<UUserWidget> ShutterWidgetClass;

	/** Resolution of each captured photo stored in the gallery */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Photo")
	FIntPoint PhotoResolution;

	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	bool bIsAiming;

	// --- Input ---
	// Real assets (see /Game/PhotoSim/Input/), not created at runtime: UInputAction/
	// UInputMappingContext are asset classes that Unreal expects to live in a package, and
	// runtime-created instances of them (whatever object owns them, whenever they're created)
	// destabilize editor systems that enumerate objects - this was the cause of several crashes.

	/** Mapping Context to be used while the camera is equipped */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> CameraMappingContext;

	/** Right-click: toggle the viewfinder */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	/** Scroll wheel: zoom while aiming */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ZoomAction;

	/** Left-click: take a photo */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> PhotoAction;

	/** Hold + scroll: adjust Aperture instead of zooming */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ApertureModifierAction;

	/** Hold + scroll: adjust ShutterSpeed instead of zooming */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ShutterSpeedModifierAction;

	/** Hold + scroll: adjust ISO instead of zooming */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ISOModifierAction;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Input_ToggleAim(const FInputActionValue& Value);
	void Input_Zoom(const FInputActionValue& Value);
	void Input_TakePhoto(const FInputActionValue& Value);
	void Input_ApertureModifierStarted(const FInputActionValue& Value);
	void Input_ApertureModifierCompleted(const FInputActionValue& Value);
	void Input_ShutterModifierStarted(const FInputActionValue& Value);
	void Input_ShutterModifierCompleted(const FInputActionValue& Value);
	void Input_ISOModifierStarted(const FInputActionValue& Value);
	void Input_ISOModifierCompleted(const FInputActionValue& Value);

	/** Applies Aperture/ShutterSpeed/ISO as a fixed Manual exposure (plus DOF/motion blur/grain) onto a post process settings struct */
	void ApplyExposureSettings(FPostProcessSettings& PPS) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<APhotoSimCharacter> Character;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ViewfinderWidgetInstance;

	UPROPERTY(Transient)
	TObjectPtr<UCameraShutterWidget> ShutterWidgetInstance;

	/** Created lazily in AttachCamera, attached to the player's first person camera */
	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> PhotoCapture;

	/** Created lazily in SetupLiveViewScreen, attached to this component at the lens position */
	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> LensCapture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> LiveViewTarget;

	/** 0-1, persists across aim toggles */
	float ZoomAlpha;

	bool bApertureModifierHeld = false;
	bool bShutterSpeedModifierHeld = false;
	bool bISOModifierHeld = false;
};
