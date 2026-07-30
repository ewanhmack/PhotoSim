// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "LensDefinition.h"
#include "TP_CameraComponent.generated.h"

class APhotoSimCharacter;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
class UCameraShutterWidget;
class ULensInventoryWidget;
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
	// Viewfinder FOV is derived from focal length (see GetFOVForFocalLengthMM) rather than
	// tuned directly, so a lens's focal length range is the single source of truth for both the
	// mm HUD readout and how far the view actually zooms in.

	/** Player camera FOV when not aiming (no lens framing applies at the hip) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float DefaultFOV;

	/** How much one scroll notch changes focal length (mm) - copied from the equipped lens's own ZoomStepMM, see EquipLens */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomStepMM;

	/** Assumed sensor width in mm, used to convert focal length to FOV (36mm = full-frame equivalent) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float SensorWidthMM;

	/** Widest end of the current lens's zoom range (scroll at 0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float MinFocalLengthMM;

	/** Most zoomed-in end of the current lens's zoom range (scroll at max) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float MaxFocalLengthMM;

	/** Converts a focal length (mm) to horizontal FOV (degrees), assuming SensorWidthMM */
	UFUNCTION(BlueprintPure, Category = "Camera|Zoom")
	float GetFOVForFocalLengthMM(float FocalLengthMM) const;

	// --- Lenses ---

	/** Every lens the player currently has. Index 0 is equipped by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Lenses")
	TArray<FLensDefinition> AvailableLenses;

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Lenses")
	int32 EquippedLensIndex;

	/** Swaps to AvailableLenses[LensIndex]: applies its aperture/focal length range, clamps current Aperture into it, resets zoom to 0 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Lenses")
	void EquipLens(int32 LensIndex);

	/** Toggles the lens-picker UI. Works any time the camera is equipped, not just while aiming. */
	UFUNCTION(BlueprintCallable, Category = "Camera|Lenses")
	void ToggleLensInventory();

	/** Optional widget class for the lens inventory. Defaults to ULensInventoryWidget if left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Lenses")
	TSubclassOf<UUserWidget> LensInventoryWidgetClass;

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

	// --- Focus ---
	// Continuously autofocused via a centre-frame raycast (see UpdateAutoFocus) unless the Focus
	// modifier is held, in which case scroll manually pulls focus instead - same hold+scroll
	// pattern as Aperture/Shutter/ISO. Without this, FocalDistance was a fixed constant that only
	// matched what was actually in frame by coincidence, which got very obvious on long lenses
	// (depth of field narrows sharply with focal length, so being focused at the wrong distance
	// blurs everything).

	/** How far in front of the lens is currently in focus - kept in sync automatically unless manually overridden */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Focus")
	float FocalDistance;

	/** Closest distance autofocus/manual focus will resolve to - copied from the equipped lens's own MinFocusDistanceCM, see EquipLens */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Focus")
	float MinFocusDistance;

	/**
	 * Farthest distance autofocus/manual focus will resolve to - also the raycast's max range,
	 * and the fallback if nothing's hit. Not lens-specific (every lens can focus to infinity), so
	 * unlike MinFocusDistance this is a single shared value, not copied per lens. Reaching it is
	 * displayed as the infinity symbol in the HUD rather than a distance, same as a real lens's
	 * focus scale.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Focus")
	float MaxFocusDistance;

	/** How much holding the Focus modifier + one scroll notch changes FocalDistance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Focus")
	float FocusStep;

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
	//
	// Transient, not EditAnywhere: these are re-resolved by the constructor via ConstructorHelpers
	// every time, and are never meant to be hand-picked differently per Blueprint instance. Marking
	// them EditAnywhere let a Blueprint serialize (and freeze) whatever value happened to be
	// resolved at save time - often None, if the asset didn't exist yet - which then permanently
	// overrode the constructor's default until manually reset. Transient properties are never
	// serialized, so there's nothing left for a Blueprint to freeze.

	/** Mapping Context to be used while the camera is equipped */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> CameraMappingContext;

	/** Right-click: toggle the viewfinder */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	/** Scroll wheel: zoom while aiming */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ZoomAction;

	/** Left-click: take a photo */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> PhotoAction;

	/** Hold + scroll: adjust Aperture instead of zooming */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ApertureModifierAction;

	/** Hold + scroll: adjust ShutterSpeed instead of zooming */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ShutterSpeedModifierAction;

	/** Hold + scroll: adjust ISO instead of zooming */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ISOModifierAction;

	/** Hold + scroll: manually pull focus instead of zooming; released, autofocus resumes */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> FocusModifierAction;

	/** I: toggle the lens inventory */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> OpenLensInventoryAction;

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
	void Input_FocusModifierStarted(const FInputActionValue& Value);
	void Input_FocusModifierCompleted(const FInputActionValue& Value);
	void Input_OpenLensInventory(const FInputActionValue& Value);

	/** Applies Aperture/ShutterSpeed/ISO as a fixed Manual exposure (plus DOF/motion blur/grain) onto a post process settings struct */
	void ApplyExposureSettings(FPostProcessSettings& PPS) const;

	/** Raycasts from the lens through the centre of frame and sets FocalDistance to the hit distance */
	void UpdateAutoFocus();

private:
	UPROPERTY(Transient)
	TObjectPtr<APhotoSimCharacter> Character;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ViewfinderWidgetInstance;

	UPROPERTY(Transient)
	TObjectPtr<UCameraShutterWidget> ShutterWidgetInstance;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LensInventoryWidgetInstance;

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
	bool bFocusModifierHeld = false;
	bool bIsLensInventoryOpen = false;
};
