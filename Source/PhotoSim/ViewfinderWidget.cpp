// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewfinderWidget.h"
#include "TP_CameraComponent.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "WidgetHelpers.h"

namespace
{
	/** An L-shaped corner bracket: a bar along the vertical edge, a bar along the horizontal edge. */
	TSharedRef<SWidget> MakeCornerBracket(EHorizontalAlignment HAlign, EVerticalAlignment VAlign)
	{
		const float ArmLength = 34.f;
		const float Thickness = 3.f;
		const FLinearColor BracketColor = FLinearColor(1.f, 1.f, 1.f, 0.85f);

		return SNew(SBox)
			.WidthOverride(ArmLength)
			.HeightOverride(ArmLength)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign)
				[
					SNew(SBox)
					.HeightOverride(Thickness)
					[
						MakeFlatColorBox(BracketColor)
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign)
				.VAlign(VAlign_Fill)
				[
					SNew(SBox)
					.WidthOverride(Thickness)
					[
						MakeFlatColorBox(BracketColor)
					]
				]
			];
	}

	// How far past each edge of the screen the rotation mask extends (see MakeRotationMask) - needs
	// to stay wide enough that rotating it by any realistic roll angle still leaves every screen
	// corner covered by black rather than exposing the mask's own far edge.
	constexpr float MaskExtent = 2000.f;
}

void UViewfinderWidget::SetOwningCamera(UTP_CameraComponent* InCamera)
{
	OwningCamera = InCamera;
}

void UViewfinderWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The geometry Slate hands this widget this tick is the one authoritative size for the frame
	// and the mask's hole to agree on - see CachedViewportSize's comment.
	CachedViewportSize = MyGeometry.GetLocalSize();

	// Temporary diagnostic for the frame/mask mismatch bug - remove once that's closed.
	if (GFrameCounter % 90 == 0)
	{
		const float Scale = GetFrameScale();
		UE_LOG(LogTemp, Warning, TEXT("[ViewfinderDiag] CachedViewportSize=(%.1f,%.1f) Scale=%.3f FrameSize=(%.1f,%.1f) MaskOuterSize=(%.1f,%.1f) Roll=%.1f"),
			CachedViewportSize.X, CachedViewportSize.Y, Scale,
			GetFrameWidth().Get(), GetFrameHeight().Get(),
			GetMaskOuterWidth().Get(), GetMaskOuterHeight().Get(),
			OwningCamera.IsValid() ? OwningCamera->GetRollDegrees() : -1.f);
	}
}

TOptional<FSlateRenderTransform> UViewfinderWidget::GetFrameRenderTransform() const
{
	const float RollDegrees = OwningCamera.IsValid() ? OwningCamera->GetRollDegrees() : 0.f;
	return FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(RollDegrees)));
}

float UViewfinderWidget::GetFrameScale() const
{
	const float RollDegrees = OwningCamera.IsValid() ? OwningCamera->GetRollDegrees() : 0.f;
	const float CosTheta = FMath::Abs(FMath::Cos(FMath::DegreesToRadians(RollDegrees)));
	const float SinTheta = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(RollDegrees)));

	const float W = CachedViewportSize.X;
	const float H = CachedViewportSize.Y;

	// The largest scale that keeps a screen-shaped WxH rectangle, rotated by RollDegrees, entirely
	// within the WxH screen - the classic "largest rotated rectangle inscribed in a rectangle"
	// problem. Whichever axis would overflow first (width or height) is the one that binds.
	const float ScaleForWidth = W / (W * CosTheta + H * SinTheta);
	const float ScaleForHeight = H / (H * CosTheta + W * SinTheta);
	return FMath::Min(ScaleForWidth, ScaleForHeight);
}

FOptionalSize UViewfinderWidget::GetFrameWidth() const
{
	return FOptionalSize(CachedViewportSize.X * GetFrameScale());
}

FOptionalSize UViewfinderWidget::GetFrameHeight() const
{
	return FOptionalSize(CachedViewportSize.Y * GetFrameScale());
}

FOptionalSize UViewfinderWidget::GetMaskOuterWidth() const
{
	return FOptionalSize(GetFrameWidth().Get() + MaskExtent * 2.f);
}

FOptionalSize UViewfinderWidget::GetMaskOuterHeight() const
{
	return FOptionalSize(GetFrameHeight().Get() + MaskExtent * 2.f);
}

TSharedRef<SWidget> UViewfinderWidget::MakeRuleOfThirdsGrid() const
{
	const float LineThickness = 1.f;
	const FLinearColor LineColor = FLinearColor(1.f, 1.f, 1.f, 0.35f);

	// Two vertical lines dividing the frame into thirds; FillWidth(1.f) cells are always equal,
	// so this lands exactly on the thirds at any resolution without measuring the viewport.
	TSharedRef<SWidget> VerticalLines =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(LineThickness)[ MakeFlatColorBox(LineColor) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(LineThickness)[ MakeFlatColorBox(LineColor) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f);

	TSharedRef<SWidget> HorizontalLines =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(LineThickness)[ MakeFlatColorBox(LineColor) ]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(LineThickness)[ MakeFlatColorBox(LineColor) ]
		]
		+ SVerticalBox::Slot().FillHeight(1.f);

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			VerticalLines
		]
		+ SOverlay::Slot()
		[
			HorizontalLines
		];
}

FText UViewfinderWidget::GetFocalLengthText() const
{
	const float FocalLength = OwningCamera.IsValid() ? OwningCamera->GetFocalLengthMM() : 0.f;
	return FText::FromString(FString::Printf(TEXT("%dmm"), FMath::RoundToInt(FocalLength)));
}

FText UViewfinderWidget::GetApertureText() const
{
	const float Aperture = OwningCamera.IsValid() ? OwningCamera->GetAperture() : 0.f;
	return FText::FromString(FString::Printf(TEXT("f/%.1f"), Aperture));
}

FText UViewfinderWidget::GetShutterSpeedText() const
{
	const float ShutterSpeed = OwningCamera.IsValid() ? OwningCamera->GetShutterSpeed() : 0.f;
	return FText::FromString(FString::Printf(TEXT("1/%d"), FMath::RoundToInt(ShutterSpeed)));
}

FText UViewfinderWidget::GetISOText() const
{
	const float ISO = OwningCamera.IsValid() ? OwningCamera->GetISO() : 0.f;
	return FText::FromString(FString::Printf(TEXT("ISO %d"), FMath::RoundToInt(ISO)));
}

FText UViewfinderWidget::GetFocusDistanceText() const
{
	if (!OwningCamera.IsValid())
	{
		return FText::GetEmpty();
	}

	// The far limit isn't a real distance, it's "as far as the eye can see" - same as turning a
	// real lens's focus ring all the way to its hard infinity stop.
	if (OwningCamera->FocalDistance >= OwningCamera->MaxFocusDistance - 1.f)
	{
		return FText::FromString(TEXT("∞"));
	}

	const float Meters = OwningCamera->FocalDistance / 100.f;
	if (Meters >= 1000.f)
	{
		return FText::FromString(FString::Printf(TEXT("%.1fkm"), Meters / 1000.f));
	}
	if (Meters >= 10.f)
	{
		return FText::FromString(FString::Printf(TEXT("%.0fm"), Meters));
	}
	return FText::FromString(FString::Printf(TEXT("%.1fm"), Meters));
}

FText UViewfinderWidget::GetExposureModeText() const
{
	return OwningCamera.IsValid() ? OwningCamera->GetExposureModeDisplayText() : FText::GetEmpty();
}

float UViewfinderWidget::GetFocusFraction() const
{
	if (!OwningCamera.IsValid())
	{
		return 0.f;
	}

	// Log-scaled, like a real lens's focus distance markings, since near distances matter
	// proportionally more than far ones - a linear scale would sit pinned near the far end always.
	const float Min = FMath::Max(OwningCamera->MinFocusDistance, 1.f);
	const float Max = FMath::Max(OwningCamera->MaxFocusDistance, Min + 1.f);
	const float Current = FMath::Clamp(OwningCamera->FocalDistance, Min, Max);

	const float LogMin = FMath::Loge(Min);
	const float LogMax = FMath::Loge(Max);
	const float LogCurrent = FMath::Loge(Current);

	return (LogCurrent - LogMin) / FMath::Max(LogMax - LogMin, KINDA_SMALL_NUMBER);
}

FMargin UViewfinderWidget::GetFocusMarkerPadding() const
{
	const float BarWidth = 160.f;
	const float MarkerWidth = 3.f;
	const float Offset = GetFocusFraction() * (BarWidth - MarkerWidth);
	return FMargin(Offset, 0.f, 0.f, 0.f);
}

TSharedRef<SWidget> UViewfinderWidget::MakeFocusBar() const
{
	const float BarWidth = 160.f;
	const float BarHeight = 4.f;
	const float MarkerWidth = 3.f;
	const float MarkerHeight = 14.f;

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(STextBlock)
			.Text_UObject(this, &UViewfinderWidget::GetFocusDistanceText)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			.ColorAndOpacity(FLinearColor(0.1f, 1.f, 0.2f, 0.95f))
		]

		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(BarWidth)
			.HeightOverride(MarkerHeight)
			[
				SNew(SOverlay)

				+ SOverlay::Slot().VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HeightOverride(BarHeight)
					[
						MakeFlatColorBox(FLinearColor(1.f, 1.f, 1.f, 0.3f))
					]
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.Padding(TAttribute<FMargin>::CreateLambda([this]() { return GetFocusMarkerPadding(); }))
				[
					SNew(SBox)
					.WidthOverride(MarkerWidth)
					.HeightOverride(MarkerHeight)
					[
						MakeFlatColorBox(FLinearColor(0.1f, 1.f, 0.2f, 1.f))
					]
				]
			]
		];
}

TSharedRef<SWidget> UViewfinderWidget::MakeExposureReadout() const
{
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	const FLinearColor TextColor = FLinearColor(0.1f, 1.f, 0.2f, 0.95f);

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
		[
			SNew(STextBlock).Text_UObject(this, &UViewfinderWidget::GetApertureText).Font(Font).ColorAndOpacity(TextColor)
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
		[
			SNew(STextBlock).Text_UObject(this, &UViewfinderWidget::GetShutterSpeedText).Font(Font).ColorAndOpacity(TextColor)
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
		[
			SNew(STextBlock).Text_UObject(this, &UViewfinderWidget::GetISOText).Font(Font).ColorAndOpacity(TextColor)
		];
}

TSharedRef<SWidget> UViewfinderWidget::MakeFramingGuides() const
{
	return SNew(SOverlay)

		// Rule of thirds composition grid
		+ SOverlay::Slot().Padding(FMargin(0.f, 24.f))
		[
			MakeRuleOfThirdsGrid()
		]

		// Corner AF brackets
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(28.f)[ MakeCornerBracket(HAlign_Left, VAlign_Top) ]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(28.f)[ MakeCornerBracket(HAlign_Right, VAlign_Top) ]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(28.f)[ MakeCornerBracket(HAlign_Left, VAlign_Bottom) ]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(28.f)[ MakeCornerBracket(HAlign_Right, VAlign_Bottom) ]

		// Centre focus reticle
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(2.f)
			.HeightOverride(24.f)
			[
				MakeFlatColorBox(FLinearColor(1.f, 1.f, 1.f, 0.6f))
			]
		];
}

TSharedRef<SWidget> UViewfinderWidget::MakeFrame() const
{
	// A thin outline, not a filled SBorder - SBorder's BorderBackgroundColor tints its whole
	// interior (fine for the gallery, where a photo image covers it completely), which here would
	// paint the entire frame opaque instead of leaving it transparent to the live scene behind it.
	const float BorderThickness = 2.f;
	const FLinearColor BorderColor = FLinearColor(1.f, 1.f, 1.f, 0.6f);

	return SNew(SOverlay)
		+ SOverlay::Slot().VAlign(VAlign_Top)[ SNew(SBox).HeightOverride(BorderThickness)[ MakeFlatColorBox(BorderColor) ] ]
		+ SOverlay::Slot().VAlign(VAlign_Bottom)[ SNew(SBox).HeightOverride(BorderThickness)[ MakeFlatColorBox(BorderColor) ] ]
		+ SOverlay::Slot().HAlign(HAlign_Left)[ SNew(SBox).WidthOverride(BorderThickness)[ MakeFlatColorBox(BorderColor) ] ]
		+ SOverlay::Slot().HAlign(HAlign_Right)[ SNew(SBox).WidthOverride(BorderThickness)[ MakeFlatColorBox(BorderColor) ] ]
		+ SOverlay::Slot()[ MakeFramingGuides() ];
}

TSharedRef<SWidget> UViewfinderWidget::MakeRotationMask() const
{
	// A nine-patch: four MaskExtent-square black corners, four black edge bars filling the rest of
	// each side, and a transparent centre the exact size of the (shrink-to-fit) frame. Built
	// oversized - frame size plus MaskExtent on every side - and centred, so once this whole mask
	// rotates with the frame (see RebuildWidget), its far edges still fall outside the screen -
	// it's the explicit corner squares that actually guarantee no gap opens up at the screen's own
	// corners, not the overlap between adjacent bars, which is why this needs four corners rather
	// than just four bars.
	auto MakeCorner = [] { return SNew(SBox).WidthOverride(MaskExtent).HeightOverride(MaskExtent)[ MakeFlatColorBox(FLinearColor::Black) ]; };

	return SNew(SBox)
		.WidthOverride_UObject(this, &UViewfinderWidget::GetMaskOuterWidth)
		.HeightOverride_UObject(this, &UViewfinderWidget::GetMaskOuterHeight)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ MakeCorner() ]
				+ SHorizontalBox::Slot().FillWidth(1.f)[ MakeFlatColorBox(FLinearColor::Black) ]
				+ SHorizontalBox::Slot().AutoWidth()[ MakeCorner() ]
			]

			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(MaskExtent)[ MakeFlatColorBox(FLinearColor::Black) ] ]
				// TEMPORARY DIAGNOSTIC: bright, semi-transparent magenta instead of SNew(SSpacer),
				// so a screenshot shows exactly where this "hole" actually lands relative to the
				// frame border - revert to SNew(SSpacer) once the real bug is found.
				+ SHorizontalBox::Slot().FillWidth(1.f)[ MakeFlatColorBox(FLinearColor(1.f, 0.f, 1.f, 0.5f)) ]
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(MaskExtent)[ MakeFlatColorBox(FLinearColor::Black) ] ]
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ MakeCorner() ]
				+ SHorizontalBox::Slot().FillWidth(1.f)[ MakeFlatColorBox(FLinearColor::Black) ]
				+ SHorizontalBox::Slot().AutoWidth()[ MakeCorner() ]
			]
		];
}

TSharedRef<SWidget> UViewfinderWidget::RebuildWidget()
{
	return SNew(SOverlay)

		// The frame fills the screen at zero roll and rotates live by the camera's exact roll,
		// rather than swapping between a fixed landscape/portrait shape - this is what makes it
		// read as "the angle of the camera" instead of just two states, and TakePhoto rotates the
		// captured image by the same angle so it matches. GetFrameScale shrinks it as the roll
		// increases, so the rotated rectangle always stays fully on screen instead of its corners
		// running off it. The mask and the frame are siblings sharing the exact same render
		// transform, pivot and size (see GetFrameWidth/Height), so the mask's transparent window
		// and the frame's border always land on the same rectangle - what the roll turns that
		// rectangle away from reads as black instead of showing extra scene the photo won't capture.
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SOverlay)
			.RenderTransform_UObject(this, &UViewfinderWidget::GetFrameRenderTransform)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))

			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				MakeRotationMask()
			]

			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride_UObject(this, &UViewfinderWidget::GetFrameWidth)
				.HeightOverride_UObject(this, &UViewfinderWidget::GetFrameHeight)
				[
					MakeFrame()
				]
			]
		]

		// Focal length readout
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(16.f, 0.f, 0.f, 34.f))
		[
			SNew(STextBlock)
			.Text_UObject(this, &UViewfinderWidget::GetFocalLengthText)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			.ColorAndOpacity(FLinearColor(0.1f, 1.f, 0.2f, 0.95f))
		]

		// Aperture / shutter speed / ISO readout - real, controllable values (hold Z/X/C + scroll)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 16.f, 34.f))
		[
			MakeExposureReadout()
		]

		// Focal range bar - shows current focus distance; autofocus unless holding F + scroll
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 72.f))
		[
			MakeFocusBar()
		];
}
