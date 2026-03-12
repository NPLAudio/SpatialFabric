// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "UI/SSpatialFabricPanel.h"
#include "SpatialFabricManagerActor.h"
#include "SpatialFabricSettings.h"
#include "SpatialStageVolume.h"
#include "SpatialFabricTypes.h"
#include "SpatialFabricEditorCompatibility.h"
#include "SpatialMath.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Selection.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "SSpatialFabricPanel"

// SSpatialFabricPanel: Spatial Fabric Manager window (Stage/Objects/Radar/Output tabs)
// Hide stage checkbox below listener buttons.

// ─────────────────────────────────────────────────────────────────────────────
//  SRadarView — live 2-D top-down position display
// ─────────────────────────────────────────────────────────────────────────────

class SRadarView : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRadarView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) {}

	/** Called from OnRefreshTimer — pushes the current PIE snapshot into the widget. */
	void SetSnapshot(const FSpatialFrameSnapshot& Snap)
	{
		CachedSnapshot = Snap;
		bHasSnapshot   = true;
	}

	/** Update the cached stage physical dimensions for aspect-correct drawing. */
	void SetStageDimensions(float InWidthM, float InDepthM)
	{
		StageWidthM = FMath::Max(InWidthM, 0.1f);
		StageDepthM = FMath::Max(InDepthM, 0.1f);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(2000.f, 2000.f);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const float     cx   = Size.X * 0.5f;
		const float     cy   = Size.Y * 0.5f;

		// Available half-size on screen (with margin for edge labels)
		const float Margin   = 56.f;
		const float AvailHalfX = FMath::Max((Size.X - Margin * 2.f) * 0.5f, 20.f);
		const float AvailHalfY = FMath::Max((Size.Y - Margin * 2.f) * 0.5f, 20.f);

		// Compute Rx (screen horizontal = StageDepthM, Y axis = left/right)
		// and     Ry (screen vertical   = StageWidthM, X axis = front/back)
		// preserving the stage volume's aspect ratio.
		float Rx, Ry;
		{
			const float Aspect = StageDepthM / StageWidthM; // horizontal / vertical
			Ry = AvailHalfY;
			Rx = Ry * Aspect;
			if (Rx > AvailHalfX)
			{
				Rx = AvailHalfX;
				Ry = Rx / Aspect;
			}
		}

		const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteTexture");

		// ── Background ──────────────────────────────────────────────────────
		FSlateDrawElement::MakeBox(
			OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.06f, 0.06f, 0.06f, 1.f));
		++LayerId;

		// ── Stage boundary (±1 rectangle, aspect-correct) ───────────────────
		{
			TArray<FVector2D> Rect;
			Rect.Add(FVector2D(cx - Rx, cy - Ry));
			Rect.Add(FVector2D(cx + Rx, cy - Ry));
			Rect.Add(FVector2D(cx + Rx, cy + Ry));
			Rect.Add(FVector2D(cx - Rx, cy + Ry));
			Rect.Add(FVector2D(cx - Rx, cy - Ry));
			FSlateDrawElement::MakeLines(
				OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(),
				Rect, ESlateDrawEffect::None,
				FLinearColor(1.f, 1.f, 1.f, 0.3f), true, 1.f);
		}

		// ── Crosshairs ───────────────────────────────────────────────────────
		{
			TArray<FVector2D> HLine = { FVector2D(cx - Rx, cy), FVector2D(cx + Rx, cy) };
			TArray<FVector2D> VLine = { FVector2D(cx, cy - Ry), FVector2D(cx, cy + Ry) };
			const FLinearColor Gray(0.25f, 0.25f, 0.25f, 1.f);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(), HLine, ESlateDrawEffect::None, Gray, true, 1.f);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(), VLine, ESlateDrawEffect::None, Gray, true, 1.f);
		}
		++LayerId;

		// ── Dimension labels at boundary edges ──────────────────────────────
		{
			const FSlateFontInfo SmallFont = FAppStyle::GetFontStyle("SmallFont");
			const FLinearColor DimColor(0.45f, 0.55f, 0.45f);
			const float HalfW = StageWidthM * 0.5f;  // vertical (front/back)
			const float HalfD = StageDepthM * 0.5f;  // horizontal (left/right)

			// Top: front (+X)
			FSlateDrawElement::MakeText(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2f(60.f, 14.f),
					FSlateLayoutTransform(FVector2f(cx - 30.f, cy - Ry - 16.f))),
				FText::FromString(FString::Printf(TEXT("Front +%.0fm"), HalfW)),
				SmallFont, ESlateDrawEffect::None, DimColor);
			// Bottom: back (-X)
			FSlateDrawElement::MakeText(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2f(60.f, 14.f),
					FSlateLayoutTransform(FVector2f(cx - 30.f, cy + Ry + 4.f))),
				FText::FromString(FString::Printf(TEXT("Back -%.0fm"), HalfW)),
				SmallFont, ESlateDrawEffect::None, DimColor);
			// Left: house-left (-Y)
			FSlateDrawElement::MakeText(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2f(50.f, 14.f),
					FSlateLayoutTransform(FVector2f(cx - Rx - 52.f, cy - 7.f))),
				FText::FromString(FString::Printf(TEXT("L -%.0fm"), HalfD)),
				SmallFont, ESlateDrawEffect::None, DimColor);
			// Right: house-right (+Y)
			FSlateDrawElement::MakeText(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2f(50.f, 14.f),
					FSlateLayoutTransform(FVector2f(cx + Rx + 4.f, cy - 7.f))),
				FText::FromString(FString::Printf(TEXT("R +%.0fm"), HalfD)),
				SmallFont, ESlateDrawEffect::None, DimColor);
		}

		// ── No-data hint ────────────────────────────────────────────────────
		if (!bHasSnapshot || SFArrayIsEmpty(CachedSnapshot.Objects))
		{
			const FString HintStr = bHasSnapshot
				? TEXT("No objects tracked")
				: TEXT("No data -- start PIE");
			FSlateDrawElement::MakeText(
				OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(160.f, 16.f),
					FSlateLayoutTransform(FVector2f(cx - 80.f, cy - 8.f))),
				FText::FromString(HintStr),
				FAppStyle::GetFontStyle("SmallFont"),
				ESlateDrawEffect::None,
				FLinearColor(0.5f, 0.5f, 0.5f));
			return LayerId;
		}

		const FSpatialFrameSnapshot& Snap = CachedSnapshot;

		// Helper: StageNormalized → screen position
		// +X = front → screen top  |  +Y = right → screen right
		auto NormToScreen = [&](float NormX, float NormY) -> FVector2D
		{
			return FVector2D(cx + NormY * Rx, cy - NormX * Ry);
		};

		// ── Object dots ─────────────────────────────────────────────────────
		const float DotHalf = 4.f;
		int32 OffScreenCount = 0;
		for (const FSpatialNormalizedState& Obj : Snap.Objects)
		{
			const float NX = Obj.StageNormalized.X;
			const float NY = Obj.StageNormalized.Y;
			if (FMath::Abs(NX) > 1.1f || FMath::Abs(NY) > 1.1f)
			{
				++OffScreenCount;
				continue;
			}

			FLinearColor DotColor = Obj.bMuted
				? FLinearColor(1.f, 0.5f, 0.0f)   // orange = muted
				: FLinearColor(0.2f, 1.f, 0.3f);  // green  = active

			const FVector2D Pos = NormToScreen(NX, NY);
			FSlateDrawElement::MakeBox(
				OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(DotHalf * 2.f, DotHalf * 2.f),
					FSlateLayoutTransform(FVector2f(Pos.X - DotHalf, Pos.Y - DotHalf))),
				WhiteBrush, ESlateDrawEffect::None, DotColor);

			// Label below dot
			FSlateDrawElement::MakeText(
				OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(80.f, 14.f),
					FSlateLayoutTransform(FVector2f(Pos.X - 40.f, Pos.Y + DotHalf + 2.f))),
				FText::FromString(Obj.Label),
				FAppStyle::GetFontStyle("SmallFont"),
				ESlateDrawEffect::None,
				FLinearColor(0.85f, 0.85f, 0.85f));

			// Coordinates below label (meters)
			const FString CoordStr = FString::Printf(TEXT("(%.1f, %.1f)m"),
				Obj.StageMeters.X, Obj.StageMeters.Y);
			FSlateDrawElement::MakeText(
				OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(80.f, 12.f),
					FSlateLayoutTransform(FVector2f(Pos.X - 40.f, Pos.Y + DotHalf + 16.f))),
				FText::FromString(CoordStr),
				FAppStyle::GetFontStyle("SmallFont"),
				ESlateDrawEffect::None,
				FLinearColor(0.55f, 0.65f, 0.55f));
		}

		// ── Off-screen warning ───────────────────────────────────────────────
		if (OffScreenCount > 0)
		{
			const FString WarnStr = FString::Printf(
				TEXT("%d object(s) off-screen\n(Assign Stage Volume to Manager)"),
				OffScreenCount);
			FSlateDrawElement::MakeText(
				OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(Size.X - 8.f, 32.f),
					FSlateLayoutTransform(FVector2f(4.f, Size.Y - 36.f))),
				FText::FromString(WarnStr),
				FAppStyle::GetFontStyle("SmallFont"),
				ESlateDrawEffect::None,
				FLinearColor(1.f, 0.55f, 0.1f));
		}
		++LayerId;

		// ── Listener marker (yellow diamond) ────────────────────────────────
		if (Snap.bHasListener)
		{
			const FVector2D LP = NormToScreen(
				Snap.ListenerNormalized.X, Snap.ListenerNormalized.Y);
			const float D = 5.f;
			TArray<FVector2D> Diamond;
			Diamond.Add(LP + FVector2D(0.f, -D));
			Diamond.Add(LP + FVector2D(D,   0.f));
			Diamond.Add(LP + FVector2D(0.f,  D));
			Diamond.Add(LP + FVector2D(-D,  0.f));
			Diamond.Add(LP + FVector2D(0.f, -D));
			FSlateDrawElement::MakeLines(
				OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(),
				Diamond, ESlateDrawEffect::None,
				FLinearColor(1.f, 0.95f, 0.1f), true, 1.5f);
		}

		return LayerId;
	}

private:
	FSpatialFrameSnapshot CachedSnapshot;
	bool bHasSnapshot = false;
	float StageWidthM  = 20.f;  // PhysicalWidthMeters  (X axis = radar vertical)
	float StageDepthM  = 15.f;  // PhysicalDepthMeters  (Y axis = radar horizontal)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Format selector definitions (Phase-1 adapters)
// ─────────────────────────────────────────────────────────────────────────────

struct FSFFormatDef { ESpatialAdapterType Type; FString Name; FLinearColor Color; };

static TArray<FSFFormatDef> GetFormatDefs()
{
	return {
		{ ESpatialAdapterType::ADMOSC, TEXT("ADM-OSC"), FLinearColor(0.10f, 0.45f, 0.85f) },
		{ ESpatialAdapterType::Custom, TEXT("Custom"),  FLinearColor(0.55f, 0.35f, 0.85f) },
	};
}

// ─────────────────────────────────────────────────────────────────────────────
//  Construct
// ─────────────────────────────────────────────────────────────────────────────

void SSpatialFabricPanel::Construct(const FArguments& InArgs)
{
	RegisterActiveTimer(0.25f,
		FWidgetActiveTimerDelegate::CreateSP(this, &SSpatialFabricPanel::OnRefreshTimer));

	// Build the content switcher first so tab buttons can reference it
	TSharedRef<SWidgetSwitcher> Switcher = SAssignNew(TabSwitcher, SWidgetSwitcher)
		+ SWidgetSwitcher::Slot()[ BuildStageTab()   ]   // 0
		+ SWidgetSwitcher::Slot()[ BuildObjectsTab() ]   // 1
		+ SWidgetSwitcher::Slot()[ BuildRadarTab()   ]   // 2
		+ SWidgetSwitcher::Slot()[ BuildOutputTab()  ];  // 3

	// ── Tab button row ──────────────────────────────────────────────────────
	auto MakeTabBtn = [this](FText Label, int32 Idx) -> TSharedRef<SWidget>
	{
		return SNew(SBox).MinDesiredWidth(80.f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ButtonColorAndOpacity_Lambda([this, Idx]()
			{
				return (TabSwitcher->GetActiveWidgetIndex() == Idx)
					? FLinearColor(0.15f, 0.45f, 0.80f)
					: FLinearColor(0.12f, 0.12f, 0.12f);
			})
			.OnClicked_Lambda([this, Idx]() -> FReply
			{
				TabSwitcher->SetActiveWidgetIndex(Idx);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(Label)
				.Justification(ETextJustify::Center)
				.Font_Lambda([this, Idx]()
				{
					return (TabSwitcher->GetActiveWidgetIndex() == Idx)
						? FAppStyle::GetFontStyle("BoldFont")
						: FAppStyle::GetFontStyle("NormalFont");
				})
			]
		];
	};

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(4.f))
		[
			SNew(SVerticalBox)

			// ── Panel title ───────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f, 2.f, 2.f, 6.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "Spatial Fabric — Multi-Protocol Spatial Control"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
				.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f))
			]

			// ── Tab strip ─────────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)
				[ MakeTabBtn(LOCTEXT("TabStage",    "Stage"),    0) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)
				[ MakeTabBtn(LOCTEXT("TabObjects",  "Objects"),  1) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)
				[ MakeTabBtn(LOCTEXT("TabRadar",  "Radar"),  2) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)
				[ MakeTabBtn(LOCTEXT("TabOutput", "Output"), 3) ]
				+ SHorizontalBox::Slot().FillWidth(1.f) // spacer
			]

			// ── Content ──────────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[ Switcher ]
		]
	];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tab — Stage
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SSpatialFabricPanel::BuildStageTab()
{
	// Helper shared between lambdas: get the linked stage volume (may be null).
	// Works both in editor and PIE because GetManager() already picks the right world.
	auto GetSV = [this]() -> ASpatialStageVolume*
	{
		ASpatialFabricManagerActor* Mgr = GetManager();
		return Mgr ? Mgr->StageVolume.Get() : nullptr;
	};

	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(6.f, 4.f)
		[
			SNew(SVerticalBox)

			// ── Stage Volume header ───────────────────────────────────────
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("StageHeader", "Stage Volume"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("StageHelp",
					"1. Place an ASpatialStageVolume in the level (or click Spawn below).\n"
					"2. Assign it to the SpatialFabricManagerActor → StageVolume property.\n"
					"3. Set Physical Width / Depth / Height on the Stage Volume to match your real space."))
				.AutoWrapText(true)
				.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SButton)
				// Label changes: "Select / Assign" when a stage volume exists, "Spawn" when none.
				.Text_Lambda([this]()
				{
					UWorld* W = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
					if (W)
						for (TActorIterator<ASpatialStageVolume> It(W); It; ++It)
							return LOCTEXT("SelectStage", "Select / Assign Stage Volume");
					return LOCTEXT("SpawnStage", "Spawn Stage Volume in Level");
				})
				.HAlign(HAlign_Center)
				.ButtonColorAndOpacity(FLinearColor(0.15f, 0.45f, 0.80f))
				.OnClicked_Lambda([this]() -> FReply
				{
					if (!GEditor) return FReply::Handled();
					UWorld* W = GEditor->GetEditorWorldContext().World();
					if (!W) return FReply::Handled();

					// Reuse existing stage volume if one is already in the level.
					ASpatialStageVolume* SV = nullptr;
					for (TActorIterator<ASpatialStageVolume> It(W); It; ++It)
					{
						SV = *It;
						break;
					}
					if (!SV)
					{
						SV = W->SpawnActor<ASpatialStageVolume>();
					}
					if (!SV) return FReply::Handled();

					// Auto-assign to the manager if not already set.
					if (ASpatialFabricManagerActor* Mgr = GetManager())
					{
						if (Mgr->StageVolume.Get() != SV)
						{
							Mgr->StageVolume = SV;
							Mgr->MarkPackageDirty();
						}
					}

					// Select it in the viewport.
					GEditor->SelectNone(false, true);
					GEditor->SelectActor(SV, true, true);

					return FReply::Handled();
				})
			]

			// ── Stage volume link status + Assign picker ──────────────────
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
			[
				SNew(SHorizontalBox)

				// Status text (green = linked, orange = not set)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const ASpatialFabricManagerActor* Mgr = GetManager();
						if (!Mgr)
							return LOCTEXT("NoMgr", "No Manager actor found in level.");
						const ASpatialStageVolume* SV = Mgr->StageVolume.Get();
						if (!SV)
							return LOCTEXT("NoSV", "Stage Volume not assigned — use Assign ▼");
						return FText::Format(LOCTEXT("SVStatus", "Linked: {0}"),
							FText::FromString(SV->GetActorLabel()));
					})
					.ColorAndOpacity_Lambda([this]()
					{
						const ASpatialFabricManagerActor* Mgr = GetManager();
						const ASpatialStageVolume* SV = Mgr ? Mgr->StageVolume.Get() : nullptr;
						return SV
							? FSlateColor(FLinearColor(0.30f, 0.85f, 0.30f))
							: FSlateColor(FLinearColor(1.00f, 0.50f, 0.10f));
					})
					.AutoWrapText(true)
				]

				// Assign combo button — lists stage volumes in the level
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SComboButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton")
					.ButtonColorAndOpacity(FLinearColor(0.18f, 0.18f, 0.18f))
					.ContentPadding(FMargin(6.f, 2.f))
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AssignBtn", "Assign \u25bc"))
						.ColorAndOpacity(FLinearColor::White)
					]
					.OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
					{
						FMenuBuilder MenuBuilder(true, nullptr);

						if (SFArrayIsEmpty(SVActorList))
						{
							MenuBuilder.AddMenuEntry(
								LOCTEXT("NoSVInLevel", "No Stage Volume in level"),
								FText::GetEmpty(),
								FSlateIcon(),
								FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]{ return false; })));
						}
						else
						{
							for (const TWeakObjectPtr<ASpatialStageVolume>& WeakSV : SVActorList)
							{
								ASpatialStageVolume* SV = WeakSV.Get();
								if (!SV) { continue; }

								const FText Label = FText::FromString(SV->GetActorLabel());
								MenuBuilder.AddMenuEntry(
									Label,
									FText::GetEmpty(),
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateLambda([this, WeakSV]()
									{
										ASpatialStageVolume* PickedSV = WeakSV.Get();
										if (!PickedSV) { return; }
										if (ASpatialFabricManagerActor* Mgr = GetManager())
										{
											Mgr->StageVolume = PickedSV;
											Mgr->MarkPackageDirty();
										}
									})));
							}
						}

						return MenuBuilder.MakeWidget();
					})
				]
			]

			// ── Listener / Player Mode ────────────────────────────────────
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 4.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ListenerHeader", "Listener / Player Mode"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			// ── Listener mode button group ────────────────────────────────
			+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 3.f)
			[
				SNew(SHorizontalBox)

				// Off
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(SButton)
					.IsEnabled_Lambda([GetSV]() { return GetSV() != nullptr; })
					.ButtonColorAndOpacity_Lambda([GetSV]()
					{
						const ASpatialStageVolume* SV = GetSV();
						return (SV && SV->ListenerMode == EListenerMode::Off)
							? FLinearColor(0.10f, 0.45f, 0.85f) : FLinearColor(0.18f, 0.18f, 0.18f);
					})
					.OnClicked_Lambda([GetSV]() -> FReply
					{
						if (ASpatialStageVolume* SV = GetSV())
						{ SV->ListenerMode = EListenerMode::Off; SV->MarkPackageDirty(); }
						return FReply::Handled();
					})
					.ToolTipText(LOCTEXT("ListenerOffTip",
						"Stage is fixed at its design-time location.\n"
						"All coordinates are relative to the box center."))
					[ SNew(STextBlock).Text(LOCTEXT("ListenerOff", "Off")).ColorAndOpacity(FLinearColor::White) ]
				]

				// Follow Position & Orientation
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.IsEnabled_Lambda([GetSV]() { return GetSV() != nullptr; })
					.ButtonColorAndOpacity_Lambda([GetSV]()
					{
						const ASpatialStageVolume* SV = GetSV();
						return (SV && SV->ListenerMode == EListenerMode::FollowPositionAndOrientation)
							? FLinearColor(0.10f, 0.45f, 0.85f) : FLinearColor(0.18f, 0.18f, 0.18f);
					})
					.OnClicked_Lambda([GetSV]() -> FReply
					{
						if (ASpatialStageVolume* SV = GetSV())
						{ SV->ListenerMode = EListenerMode::FollowPositionAndOrientation; SV->MarkPackageDirty(); }
						return FReply::Handled();
					})
					.ToolTipText(LOCTEXT("ListenerPosRotTip",
						"Stage origin AND axes follow the player each frame.\n"
						"\"Forward\" always equals the listener's facing direction.\n"
						"Use for fully listener-relative coordinate output."))
					[ SNew(STextBlock).Text(LOCTEXT("ListenerPosRot", "Follow Position & Orientation")).ColorAndOpacity(FLinearColor::White) ]
				]
			]

			// ── Hide stage in PIE ───────────────────────────────────────────
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]()
				{
					const ASpatialFabricManagerActor* Mgr = GetManager();
					return (Mgr && Mgr->bHideStageInPIE) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					if (ASpatialFabricManagerActor* Mgr = GetManager())
					{
						Mgr->bHideStageInPIE = (NewState == ECheckBoxState::Checked);
						Mgr->MarkPackageDirty();
						Mgr->ApplyStageVisibility();
					}
				})
				.ToolTipText(LOCTEXT("HideStageInPIETip",
					"When checked, hides the stage volume actor in the game view during PIE and packaged builds.\n"
					"Useful to avoid the stage box cluttering the viewport while testing."))
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("HideStageInPIE", "Hide stage volume when game is running (PIE)"))
					.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
				]
			]
		];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tab — Objects
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SSpatialFabricPanel::BuildObjectsTab()
{
	// ── Format selector row (built before Slate DSL to loop cleanly) ─────
	TSharedRef<SHorizontalBox> FormatRow = SNew(SHorizontalBox);

	FormatRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ProtocolLabel", "Protocol:"))
			.ColorAndOpacity(FLinearColor(0.60f, 0.60f, 0.60f))
		];

	for (const FSFFormatDef& Def : GetFormatDefs())
	{
		const ESpatialAdapterType FormatType = Def.Type;
		const FString FormatName             = Def.Name;
		const FLinearColor ActiveColor       = Def.Color;

		FormatRow->AddSlot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SBox)
				.HeightOverride(28.f)
				.MinDesiredWidth(90.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton")
					.HAlign(HAlign_Center)
					.ButtonColorAndOpacity_Lambda([this, FormatType, ActiveColor]()
					{
						const ASpatialFabricManagerActor* Mgr = GetManager();
						const bool bActive = Mgr && Mgr->ActiveAdapterType == FormatType;
						return bActive
							? ActiveColor
							: FLinearColor(0.15f, 0.15f, 0.15f);
					})
					.ToolTipText(FText::Format(
						LOCTEXT("FormatTip", "Route all objects via {0}"),
						FText::FromString(FormatName)))
					.OnClicked_Lambda([this, FormatType]() -> FReply
					{
						SetActiveFormat(FormatType);
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(FText::FromString(FormatName))
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FLinearColor::White)
					]
				]
			];
	}

	// Editable IP : Port for the active adapter
	FormatRow->AddSlot().FillWidth(1.f);

	FormatRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(6.f, 0.f, 2.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("IPLabel", "IP:"))
				.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.ToolTipText(LOCTEXT("IPTip", "Target IP address for the active protocol adapter"))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(110.f)
				[
					SNew(SEditableTextBox)
					.Text_Lambda([this]() -> FText
					{
						const ASpatialFabricManagerActor* Mgr = GetManager();
						if (!Mgr) return FText::GetEmpty();
						if (const FSpatialAdapterConfig* C = Mgr->AdapterConfigs.Find((uint8)Mgr->ActiveAdapterType))
							return FText::FromString(C->TargetIP);
						return FText::GetEmpty();
					})
					.OnTextCommitted_Lambda([this](const FText& Val, ETextCommit::Type)
					{
						if (ASpatialFabricManagerActor* Mgr = GetManager())
							if (FSpatialAdapterConfig* C = Mgr->AdapterConfigs.Find((uint8)Mgr->ActiveAdapterType))
							{
								C->TargetIP = Val.ToString();
								Mgr->MarkPackageDirty();
								if (Mgr->GetRouter()) Mgr->InitializeAdapters();
							}
					})
					.ToolTipText(LOCTEXT("IPTip", "Target IP address for the active protocol adapter"))
				]
			]
		];

	// ── Port control: combo preset for ADM-OSC, plain numeric for others ──
	//
	// Known ADM-OSC receiver defaults:
	//   8880 — L-ISA Controller
	//   9000 — L-ISA Processor
	//   4001 — Spat Revolution
	//   7000 — TiMax SoundHub
	// 38033 — Meyer SpaceMap Go
	struct FADMPortPreset { const TCHAR* Label; int32 Port; };
	static const FADMPortPreset ADMPortPresets[] =
	{
		{ TEXT("8880  (L-ISA Controller)"),  8880 },
		{ TEXT("9000  (L-ISA Processor)"),   9000 },
		{ TEXT("4001  (Spat Revolution)"),   4001 },
		{ TEXT("7000  (TiMax SoundHub)"),    7000 },
		{ TEXT("38033 (Meyer SpaceMap Go)"), 38033 },
		{ TEXT("50018 (en-bridge)"),         50018 },
	};

	// Helper lambda — commit a new port value and reinitialize adapters.
	auto CommitPort = [this](int32 Val)
	{
		if (ASpatialFabricManagerActor* Mgr = GetManager())
			if (FSpatialAdapterConfig* C = Mgr->AdapterConfigs.Find((uint8)Mgr->ActiveAdapterType))
			{
				C->TargetPort = FMath::Clamp(Val, 1024, 65535);
				Mgr->MarkPackageDirty();
				if (Mgr->GetRouter()) Mgr->InitializeAdapters();
			}
	};

	// Port control: dropdown (presets) + numeric field always visible together.
	// Dropdown populates the field; user can also type custom values directly.
	FormatRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			// Dropdown (presets) — populates the port field when selected
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(175.f)
				[
					SNew(SComboButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.HasDownArrow(true)
					.ToolTipText(LOCTEXT("ADMPortPresetTip",
						"Select a preset to populate the port field, or type a custom value in the field."))
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text_Lambda([this]() -> FText
						{
							const ASpatialFabricManagerActor* Mgr = GetManager();
							if (!Mgr) return LOCTEXT("PortUnknown", "—");
							const FSpatialAdapterConfig* C = Mgr->AdapterConfigs.Find((uint8)Mgr->ActiveAdapterType);
							if (!C) return LOCTEXT("PortUnknown", "—");
							const int32 P = C->TargetPort;
							static const FADMPortPreset Presets[] =
							{
								{ TEXT("8880  (L-ISA Controller)"),  8880 },
								{ TEXT("9000  (L-ISA Processor)"),   9000 },
								{ TEXT("4001  (Spat Revolution)"),   4001 },
								{ TEXT("7000  (TiMax SoundHub)"),    7000 },
								{ TEXT("38033 (Meyer SpaceMap Go)"), 38033 },
								{ TEXT("50018 (en-bridge)"),         50018 },
							};
							for (const FADMPortPreset& Pre : Presets)
								if (Pre.Port == P)
									return FText::FromString(Pre.Label);
							return FText::Format(LOCTEXT("PortCustomFmt", "{0}  (custom)"), P);
						})
						.Font(FAppStyle::GetFontStyle("SmallFont"))
					]
					.OnGetMenuContent_Lambda([this, CommitPort]() -> TSharedRef<SWidget>
					{
						FMenuBuilder MenuBuilder(true, nullptr);
						static const FADMPortPreset Presets[] =
						{
							{ TEXT("8880  (L-ISA Controller)"),  8880 },
							{ TEXT("9000  (L-ISA Processor)"),   9000 },
							{ TEXT("4001  (Spat Revolution)"),   4001 },
							{ TEXT("7000  (TiMax SoundHub)"),    7000 },
							{ TEXT("38033 (Meyer SpaceMap Go)"), 38033 },
							{ TEXT("50018 (en-bridge)"),         50018 },
						};
						for (const FADMPortPreset& Pre : Presets)
						{
							const int32 Port = Pre.Port;
							const FString Label = Pre.Label;
							MenuBuilder.AddMenuEntry(
								FText::FromString(Label),
								FText::Format(LOCTEXT("ADMPortPresetTipFmt", "Set port to {0}"), Port),
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda([this, CommitPort, Port]()
								{
									CommitPort(Port);
								}))
							);
						}
						return MenuBuilder.MakeWidget();
					})
				]
			]
			// Port numeric field (always visible)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(70.f)
				[
					SNew(SNumericEntryBox<int32>)
					.Value_Lambda([this]() -> TOptional<int32>
					{
						const ASpatialFabricManagerActor* Mgr = GetManager();
						if (!Mgr) return TOptional<int32>();
						if (const FSpatialAdapterConfig* C = Mgr->AdapterConfigs.Find((uint8)Mgr->ActiveAdapterType))
							return C->TargetPort;
						return TOptional<int32>();
					})
					.OnValueCommitted_Lambda([this, CommitPort](int32 Val, ETextCommit::Type)
					{
						CommitPort(Val);
					})
					.MinValue(1024).MaxValue(65535).AllowSpin(true)
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ToolTipText(LOCTEXT("PortTip", "Target UDP port. Use dropdown (ADM-OSC) to pick a preset, or type any value 1024–65535."))
				]
			]
		];

	// Helper lambda — commit SendRateHz and reinitialize adapters.
	auto CommitSendRate = [this](float Val)
	{
		if (ASpatialFabricManagerActor* Mgr = GetManager())
			if (FSpatialAdapterConfig* C = Mgr->AdapterConfigs.Find((uint8)Mgr->ActiveAdapterType))
			{
				C->SendRateHz = FMath::Clamp(Val, 1.f, 120.f);
				Mgr->MarkPackageDirty();
				if (Mgr->GetRouter()) Mgr->InitializeAdapters();
			}
	};

	// Send rate (polling limit) and "only on change" controls
	FormatRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(12.f, 0.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RateLabel", "Rate (Hz):"))
				.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.ToolTipText(LOCTEXT("RateTip", "Max updates per second (1–120 Hz). Lower values reduce network traffic."))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(55.f)
					[
						SNew(SNumericEntryBox<float>)
						.Value_Lambda([this]() -> TOptional<float>
						{
							const ASpatialFabricManagerActor* Mgr = GetManager();
							if (!Mgr) return TOptional<float>();
							if (const FSpatialAdapterConfig* C = Mgr->AdapterConfigs.Find((uint8)Mgr->ActiveAdapterType))
								return C->SendRateHz;
							return TOptional<float>();
						})
						.OnValueCommitted_Lambda([this, CommitSendRate](float Val, ETextCommit::Type)
						{
							CommitSendRate(Val);
						})
						.MinValue(1.f).MaxValue(120.f).AllowSpin(true)
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ToolTipText(LOCTEXT("RateTip", "Max updates per second (1–120 Hz). Lower values reduce network traffic."))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("HzUnit", "Hz"))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("HzUnit", "Hz"))
				.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
				.Font(FAppStyle::GetFontStyle("SmallFont"))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(10.f, 0.f, 0.f, 0.f).VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() -> ECheckBoxState
				{
					const ASpatialFabricManagerActor* Mgr = GetManager();
					if (!Mgr) return ECheckBoxState::Unchecked;
					if (const FSpatialAdapterConfig* C = Mgr->AdapterConfigs.Find((uint8)Mgr->ActiveAdapterType))
						return C->bSendOnlyOnChange ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					return ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					if (ASpatialFabricManagerActor* Mgr = GetManager())
						if (FSpatialAdapterConfig* C = Mgr->AdapterConfigs.Find((uint8)Mgr->ActiveAdapterType))
						{
							C->bSendOnlyOnChange = (NewState == ECheckBoxState::Checked);
							Mgr->MarkPackageDirty();
							if (Mgr->GetRouter()) Mgr->InitializeAdapters();
						}
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OnlyOnChangeLabel", "Only when changed"))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
				]
				.ToolTipText(LOCTEXT("OnlyOnChangeTip",
					"When checked, sends full state on level start, then only when coordinates (and gain/width/mute) change. Reduces traffic when objects are static."))
			]
		];

	// ── List view ─────────────────────────────────────────────────────────
	TSharedRef<SListView<TSharedPtr<int32>>> ListView =
		SAssignNew(BindingListView, SListView<TSharedPtr<int32>>)
		.ListItemsSource(&BindingRows)
		.OnGenerateRow(this, &SSpatialFabricPanel::GenerateBindingRow)
		.SelectionMode(ESelectionMode::None);

	// ── Assemble tab ──────────────────────────────────────────────────────
	return SNew(SVerticalBox)

		// ── Protocol format selector ──────────────────────────────────────
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f, 6.f, 4.f, 4.f)
		[ FormatRow ]

		// ── Adapter disabled hint (visible when active adapter is disabled) ─
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f, 2.f, 4.f, 2.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor(0.4f, 0.3f, 0.1f, 0.4f))
			.Padding(FMargin(8.f, 6.f))
			.Visibility_Lambda([this]()
			{
				const ASpatialFabricManagerActor* M = GetManager();
				if (!M) return EVisibility::Collapsed;
				const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)M->ActiveAdapterType);
				return (C && !C->bEnabled) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AdapterDisabledHint",
						"This adapter is disabled — no output will be sent. Enable it in the Manager's Details panel (AdapterConfigs) or click Enable."))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FLinearColor(0.95f, 0.85f, 0.5f))
					.AutoWrapText(true)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.f, 0.f, 0.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("EnableAdapterBtn", "Enable"))
					.OnClicked_Lambda([this]() -> FReply
					{
						if (ASpatialFabricManagerActor* Mgr = GetManager())
							if (FSpatialAdapterConfig* C = Mgr->AdapterConfigs.Find((uint8)Mgr->ActiveAdapterType))
							{
								C->bEnabled = true;
								Mgr->MarkPackageDirty();
								if (Mgr->GetRouter()) Mgr->InitializeAdapters();
							}
						return FReply::Handled();
					})
				]
			]
		]

		// ── Custom template row (visible only when Custom is active) ───────
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f, 2.f, 4.f, 2.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(FMargin(6.f, 4.f))
			.Visibility_Lambda([this]()
			{
				const ASpatialFabricManagerActor* M = GetManager();
				return (M && M->ActiveAdapterType == ESpatialAdapterType::Custom)
					? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				SNew(SVerticalBox)

				// Expandable help: placeholder reference
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(SExpandableArea)
					.InitiallyCollapsed(true)
					.HeaderContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CustomTemplateHelpHeader", "Template reference (placeholders & args)"))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					]
					.BodyContent()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 4.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CustomTemplateHelpBody",
								"Address template: Use {placeholders} in the OSC path. Built-in placeholders:\n"
								"  {id} = object ID, {x} {y} {z} = position, {gain} = gain, {width} = extent, {label} = actor name\n"
								"  {axis} = current axis name (for Discrete send mode; e.g. /source/{id}/{axis} -> /source/1/x)\n"
								"  Add per-binding CustomFields (e.g. slot, channel) for {slot}, {channel}, etc.\n\n"
								"Coords: xyz = Cartesian (x,y,z). aed = polar (azimuth, elevation, distance); args: azimuth/a, elevation/e, distance/d.\n\n"
								"Range mapping: Use the Range row to map output values. Position input is -1..1 (stage-normalized);\n"
								"  set e.g. Pos 0–100 to send 0–100 instead. Gain/Width input is 0..1.\n\n"
								"Args template: Space-separated names: x y z gain width id (xyz) or a e d (aed). Example: \"x y z\" sends position."))
							.Font(FAppStyle::GetFontStyle("SmallFont"))
							.ColorAndOpacity(FLinearColor(0.65f, 0.65f, 0.65f))
							.AutoWrapText(true)
						]
					]
				]

				// Coordinate mode (xyz / aed) and Send mode (bundled / discrete)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CoordModeLabel", "Coords:"))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ToolTipText(LOCTEXT("CoordModeTip", "xyz = Cartesian; aed = polar (azimuth elevation distance)"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() -> ECheckBoxState
						{
							if (const ASpatialFabricManagerActor* M = GetManager())
								if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									return (C->CustomCoordinateMode == ECustomCoordinateMode::XYZ) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							return ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
						{
							if (NewState == ECheckBoxState::Checked)
							{
								if (ASpatialFabricManagerActor* M = GetManager())
									if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									{
										C->CustomCoordinateMode = ECustomCoordinateMode::XYZ;
										M->MarkPackageDirty();
										if (M->GetRouter()) M->InitializeAdapters();
									}
							}
						})
						.Content()
						[
							SNew(STextBlock).Text(LOCTEXT("XYZ", "xyz")).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() -> ECheckBoxState
						{
							if (const ASpatialFabricManagerActor* M = GetManager())
								if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									return (C->CustomCoordinateMode == ECustomCoordinateMode::AED) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							return ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
						{
							if (NewState == ECheckBoxState::Checked)
							{
								if (ASpatialFabricManagerActor* M = GetManager())
									if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									{
										C->CustomCoordinateMode = ECustomCoordinateMode::AED;
										M->MarkPackageDirty();
										if (M->GetRouter()) M->InitializeAdapters();
									}
							}
						})
						.Content()
						[
							SNew(STextBlock).Text(LOCTEXT("AED", "aed")).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SendModeLabel", "Send:"))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ToolTipText(LOCTEXT("SendModeTip", "Bundled = one message with multiple args; Discrete = one message per value (use {axis} in address)"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() -> ECheckBoxState
						{
							if (const ASpatialFabricManagerActor* M = GetManager())
								if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									return (C->CustomSendMode == ECustomSendMode::Bundled) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							return ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
						{
							if (NewState == ECheckBoxState::Checked)
							{
								if (ASpatialFabricManagerActor* M = GetManager())
									if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									{
										C->CustomSendMode = ECustomSendMode::Bundled;
										M->MarkPackageDirty();
										if (M->GetRouter()) M->InitializeAdapters();
									}
							}
						})
						.Content()
						[
							SNew(STextBlock).Text(LOCTEXT("Bundled", "Bundled")).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() -> ECheckBoxState
						{
							if (const ASpatialFabricManagerActor* M = GetManager())
								if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									return (C->CustomSendMode == ECustomSendMode::Discrete) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							return ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
						{
							if (NewState == ECheckBoxState::Checked)
							{
								if (ASpatialFabricManagerActor* M = GetManager())
									if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									{
										C->CustomSendMode = ECustomSendMode::Discrete;
										M->MarkPackageDirty();
										if (M->GetRouter()) M->InitializeAdapters();
									}
							}
						})
						.Content()
						[
							SNew(STextBlock).Text(LOCTEXT("Discrete", "Discrete")).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
				]

				// Address and Args editors
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AddressTemplateLabel", "Address:"))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ToolTipText(LOCTEXT("AddressTemplateTip",
							"OSC address template. Placeholders: {id}, {x}, {y}, {z}, {gain}, {width}, {label}, plus CustomFields keys"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SEditableTextBox)
						.Text_Lambda([this]() -> FText
						{
							const ASpatialFabricManagerActor* M = GetManager();
							if (!M) return FText::GetEmpty();
							if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
								return FText::FromString(C->CustomAddressTemplate);
							return FText::FromString(TEXT("/source/{id}/position"));
						})
						.OnTextCommitted_Lambda([this](const FText& Val, ETextCommit::Type)
						{
							if (ASpatialFabricManagerActor* M = GetManager())
								if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
								{
									C->CustomAddressTemplate = Val.ToString();
									M->MarkPackageDirty();
									if (M->GetRouter()) M->InitializeAdapters();
								}
						})
						.Font(FAppStyle::GetFontStyle("SmallFont"))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ArgTemplateLabel", "Args:"))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ToolTipText(LOCTEXT("ArgTemplateTip",
							"Space-separated arg names: x, y, z, gain, width, id"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox).WidthOverride(120.f)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([this]() -> FText
							{
								const ASpatialFabricManagerActor* M = GetManager();
								if (!M) return FText::FromString(TEXT("x y z"));
								if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									return FText::FromString(C->CustomArgTemplate);
								return FText::FromString(TEXT("x y z"));
							})
							.OnTextCommitted_Lambda([this](const FText& Val, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = GetManager())
									if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									{
										C->CustomArgTemplate = Val.ToString();
										M->MarkPackageDirty();
										if (M->GetRouter()) M->InitializeAdapters();
									}
							})
							.Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
				]

				// Range mapping: Position, Gain, Width (min/max)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RangeMappingLabel", "Range:"))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ToolTipText(LOCTEXT("RangeMappingTip",
							"Map output values to custom ranges. Position: -1..1 input. Gain/Width: 0..1 input."))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 2.f, 0.f)
					[ SNew(STextBlock).Text(LOCTEXT("PosLabel", "Pos")).ColorAndOpacity(FLinearColor(0.55f,0.55f,0.55f)).Font(FAppStyle::GetFontStyle("SmallFont")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SBox).WidthOverride(50.f)
						[
							SNew(SNumericEntryBox<float>)
							.Value_Lambda([this]() -> TOptional<float>
							{
								if (const ASpatialFabricManagerActor* M = GetManager())
									if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
										return C->CustomPositionRange.X;
								return TOptional<float>();
							})
							.OnValueCommitted_Lambda([this](float Val, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = GetManager())
									if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									{
										C->CustomPositionRange.X = Val;
										M->MarkPackageDirty();
										if (M->GetRouter()) M->InitializeAdapters();
									}
							})
							.AllowSpin(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 2.f, 0.f)
					[ SNew(STextBlock).Text(FText::FromString(TEXT("–"))).ColorAndOpacity(FLinearColor(0.5f,0.5f,0.5f)).Font(FAppStyle::GetFontStyle("SmallFont")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
					[
						SNew(SBox).WidthOverride(50.f)
						[
							SNew(SNumericEntryBox<float>)
							.Value_Lambda([this]() -> TOptional<float>
							{
								if (const ASpatialFabricManagerActor* M = GetManager())
									if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
										return C->CustomPositionRange.Y;
								return TOptional<float>();
							})
							.OnValueCommitted_Lambda([this](float Val, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = GetManager())
									if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									{
										C->CustomPositionRange.Y = Val;
										M->MarkPackageDirty();
										if (M->GetRouter()) M->InitializeAdapters();
									}
							})
							.AllowSpin(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 2.f, 0.f)
					[ SNew(STextBlock).Text(LOCTEXT("GainLabel", "Gain")).ColorAndOpacity(FLinearColor(0.55f,0.55f,0.55f)).Font(FAppStyle::GetFontStyle("SmallFont")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SBox).WidthOverride(50.f)
						[
							SNew(SNumericEntryBox<float>)
							.Value_Lambda([this]() -> TOptional<float>
							{
								if (const ASpatialFabricManagerActor* M = GetManager())
									if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
										return C->CustomGainRange.X;
								return TOptional<float>();
							})
							.OnValueCommitted_Lambda([this](float Val, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = GetManager())
									if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									{
										C->CustomGainRange.X = Val;
										M->MarkPackageDirty();
										if (M->GetRouter()) M->InitializeAdapters();
									}
							})
							.AllowSpin(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 2.f, 0.f)
					[ SNew(STextBlock).Text(FText::FromString(TEXT("–"))).ColorAndOpacity(FLinearColor(0.5f,0.5f,0.5f)).Font(FAppStyle::GetFontStyle("SmallFont")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
					[
						SNew(SBox).WidthOverride(50.f)
						[
							SNew(SNumericEntryBox<float>)
							.Value_Lambda([this]() -> TOptional<float>
							{
								if (const ASpatialFabricManagerActor* M = GetManager())
									if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
										return C->CustomGainRange.Y;
								return TOptional<float>();
							})
							.OnValueCommitted_Lambda([this](float Val, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = GetManager())
									if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									{
										C->CustomGainRange.Y = Val;
										M->MarkPackageDirty();
										if (M->GetRouter()) M->InitializeAdapters();
									}
							})
							.AllowSpin(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 2.f, 0.f)
					[ SNew(STextBlock).Text(LOCTEXT("WidthLabel", "Width")).ColorAndOpacity(FLinearColor(0.55f,0.55f,0.55f)).Font(FAppStyle::GetFontStyle("SmallFont")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SBox).WidthOverride(50.f)
						[
							SNew(SNumericEntryBox<float>)
							.Value_Lambda([this]() -> TOptional<float>
							{
								if (const ASpatialFabricManagerActor* M = GetManager())
									if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
										return C->CustomWidthRange.X;
								return TOptional<float>();
							})
							.OnValueCommitted_Lambda([this](float Val, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = GetManager())
									if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									{
										C->CustomWidthRange.X = Val;
										M->MarkPackageDirty();
										if (M->GetRouter()) M->InitializeAdapters();
									}
							})
							.AllowSpin(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 2.f, 0.f)
					[ SNew(STextBlock).Text(FText::FromString(TEXT("–"))).ColorAndOpacity(FLinearColor(0.5f,0.5f,0.5f)).Font(FAppStyle::GetFontStyle("SmallFont")) ]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(50.f)
						[
							SNew(SNumericEntryBox<float>)
							.Value_Lambda([this]() -> TOptional<float>
							{
								if (const ASpatialFabricManagerActor* M = GetManager())
									if (const FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
										return C->CustomWidthRange.Y;
								return TOptional<float>();
							})
							.OnValueCommitted_Lambda([this](float Val, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = GetManager())
									if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::Custom))
									{
										C->CustomWidthRange.Y = Val;
										M->MarkPackageDirty();
										if (M->GetRouter()) M->InitializeAdapters();
									}
							})
							.AllowSpin(true)							.Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
				]
			]
		]

		// ── Separator ────────────────────────────────────────────────────
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Menu.Separator"))
			.Padding(0.f)
		]

		// ── Add / Remove toolbar ──────────────────────────────────────────
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f, 4.f, 4.f, 2.f)
		[
			SNew(SHorizontalBox)

			// Add Selected Actors
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SButton)
				.Text_Lambda([this]() { return GetAddButtonText(); })
				.ToolTipText(LOCTEXT("AddSelTip",
					"Add all currently selected level actors as tracked objects.\n"
					"Already-bound actors and the Manager/Stage actors are skipped."))
				.ButtonColorAndOpacity_Lambda([this]()
				{
					return GetNewActorCount() > 0
						? FLinearColor(0.08f, 0.55f, 0.22f)
						: FLinearColor(0.18f, 0.18f, 0.18f);
				})
				.IsEnabled_Lambda([this]() { return GetNewActorCount() > 0; })
				.OnClicked_Lambda([this]() -> FReply
				{
					OnAddSelectedActors();
					return FReply::Handled();
				})
			]

			// Spacer
			+ SHorizontalBox::Slot().FillWidth(1.f)

			// Binding count label
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					const int32 N = BindingRows.Num();
					return FText::Format(
						LOCTEXT("BindingCount", "{0} {0}|plural(one=object,other=objects)"), N);
				})
				.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
			]

			// Remove All
			+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveAll", "Remove All"))
				.ToolTipText(LOCTEXT("RemoveAllTip", "Remove all object bindings from the manager."))
				.ButtonColorAndOpacity(FLinearColor(0.55f, 0.12f, 0.12f))
				.IsEnabled_Lambda([this]() { return BindingRows.Num() > 0; })
				.OnClicked_Lambda([this]() -> FReply
				{
					if (ASpatialFabricManagerActor* Mgr = GetManager())
					{
						Mgr->ObjectBindings.Empty();
						Mgr->MarkPackageDirty();
						RebuildBindingList();
					}
					return FReply::Handled();
				})
			]
		]

		// ── Separator ────────────────────────────────────────────────────
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Menu.Separator"))
			.Padding(0.f)
		]

		// ── Binding list / empty state ────────────────────────────────────
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SOverlay)

			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EmptyBindings",
					"No objects tracked.\nSelect actors in the viewport and click Add."))
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))
				.Visibility_Lambda([this]()
				{
					return BindingRows.Num() == 0
						? EVisibility::Visible : EVisibility::Collapsed;
				})
			]

			+ SOverlay::Slot()
			[ ListView ]
		];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Binding row generation
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<ITableRow> SSpatialFabricPanel::GenerateBindingRow(
	TSharedPtr<int32>                 RowIndex,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	if (!RowIndex.IsValid())
	{
		return SNew(STableRow<TSharedPtr<int32>>, OwnerTable);
	}

	const int32 BIdx = *RowIndex;
	ASpatialFabricManagerActor* Mgr = GetManager();

	if (!Mgr || !Mgr->ObjectBindings.IsValidIndex(BIdx))
	{
		return SNew(STableRow<TSharedPtr<int32>>, OwnerTable);
	}

	const FSpatialObjectBinding& B = Mgr->ObjectBindings[BIdx];
	TWeakObjectPtr<ASpatialFabricManagerActor> WeakMgr(Mgr);

	// Resolve actor for display name
	AActor* Actor = B.TargetActor.Get();
	const bool bResolved = Actor != nullptr;
	const FString DisplayName = bResolved
		? Actor->GetActorLabel()
		: (B.CachedActorLabel.IsEmpty() ? TEXT("(None)") : B.CachedActorLabel + TEXT("  !"));

	return SNew(STableRow<TSharedPtr<int32>>, OwnerTable)
		.Padding(FMargin(2.f, 2.f))
		[
			SNew(SVerticalBox)

			// ── Main row ──────────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				// Actor name
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				.Padding(4.f, 0.f, 6.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(DisplayName))
					.ColorAndOpacity_Lambda([WeakMgr, BIdx]()
					{
						if (ASpatialFabricManagerActor* M = WeakMgr.Get())
							if (M->ObjectBindings.IsValidIndex(BIdx))
							{
								if (!M->ObjectBindings[BIdx].TargetActor.Get())
									return FLinearColor(1.0f, 0.65f, 0.1f);
								if (!M->ObjectBindings[BIdx].bEnabled)
									return FLinearColor(0.45f, 0.45f, 0.45f);
							}
						return FLinearColor(0.90f, 0.90f, 0.90f);
					})
					.ToolTipText(FText::FromString(
						bResolved ? Actor->GetPathName() : TEXT("Actor not found in level")))
				]

				// Object ID
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("IDLabel", "ID"))
						.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(52.f)
						[
							SNew(SNumericEntryBox<int32>)
							.Value_Lambda([WeakMgr, BIdx]() -> TOptional<int32>
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
										return M->ObjectBindings[BIdx].DefaultObjectID;
								return TOptional<int32>();
							})
							.OnValueCommitted_Lambda([WeakMgr, BIdx](int32 Val, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
									{
										M->ObjectBindings[BIdx].DefaultObjectID = FMath::Max(1, Val);
										M->MarkPackageDirty();
									}
							})
							.MinValue(1)
							.AllowSpin(false)
						]
					]
				]

				// Enable toggle
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.f, 0.f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([WeakMgr, BIdx]()
					{
						if (ASpatialFabricManagerActor* M = WeakMgr.Get())
							if (M->ObjectBindings.IsValidIndex(BIdx))
								return M->ObjectBindings[BIdx].bEnabled
									? ECheckBoxState::Checked
									: ECheckBoxState::Unchecked;
						return ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([WeakMgr, BIdx](ECheckBoxState State)
					{
						if (ASpatialFabricManagerActor* M = WeakMgr.Get())
							if (M->ObjectBindings.IsValidIndex(BIdx))
							{
								M->ObjectBindings[BIdx].bEnabled = (State == ECheckBoxState::Checked);
								M->MarkPackageDirty();
							}
					})
					.ToolTipText(LOCTEXT("EnableToggle", "Enable / disable this binding"))
				]

				// Remove button — use Property Editor's MakeDeleteButton (same as Details panel)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 0.f, 2.f, 0.f)
				[
					PropertyCustomizationHelpers::MakeDeleteButton(
						FSimpleDelegate::CreateLambda([this, BIdx]() { RemoveBinding(BIdx); }),
						LOCTEXT("RemoveBinding", "Remove this binding"),
						TAttribute<bool>::CreateLambda([this]() { return true; }))
				]
			]

			// ── DS100 sub-row (visible only when DS100 is the active format) ──
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f, 2.f, 4.f, 2.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.Padding(FMargin(6.f, 3.f))
				.Visibility_Lambda([WeakMgr]()
				{
					const ASpatialFabricManagerActor* M = WeakMgr.Get();
					return (M && M->ActiveAdapterType == ESpatialAdapterType::DS100)
						? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(SHorizontalBox)

					// "Spread:" label
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SpreadLabel", "Spread:"))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
					]

					// Fixed button
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)
					[
						SNew(SBox).HeightOverride(22.f).MinDesiredWidth(46.f)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "FlatButton")
							.HAlign(HAlign_Center)
							.ButtonColorAndOpacity_Lambda([WeakMgr, BIdx]()
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
										if (M->ObjectBindings[BIdx].DS100SpreadMode == EDS100SpreadMode::Fixed)
											return FLinearColor(1.00f, 0.42f, 0.10f);
								return FLinearColor(0.18f, 0.18f, 0.18f);
							})
							.ToolTipText(LOCTEXT("SpreadFixedTip", "Fixed: send Width01 as spread unchanged"))
							.OnClicked_Lambda([WeakMgr, BIdx]() -> FReply
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
									{
										M->ObjectBindings[BIdx].DS100SpreadMode = EDS100SpreadMode::Fixed;
										M->MarkPackageDirty();
									}
								return FReply::Handled();
							})
							[ SNew(STextBlock).Text(LOCTEXT("Fixed", "Fixed")).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor::White) ]
						]
					]

					// Proximity button
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
					[
						SNew(SBox).HeightOverride(22.f).MinDesiredWidth(64.f)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "FlatButton")
							.HAlign(HAlign_Center)
							.ButtonColorAndOpacity_Lambda([WeakMgr, BIdx]()
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
										if (M->ObjectBindings[BIdx].DS100SpreadMode == EDS100SpreadMode::Proximity)
											return FLinearColor(0.10f, 0.72f, 0.80f);
								return FLinearColor(0.18f, 0.18f, 0.18f);
							})
							.ToolTipText(LOCTEXT("SpreadProxTip",
								"Proximity: spread grows inverse-square as listener nears source.\n"
								"Min = far spread, Max = at-source spread, Dist = reference distance."))
							.OnClicked_Lambda([WeakMgr, BIdx]() -> FReply
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
									{
										M->ObjectBindings[BIdx].DS100SpreadMode = EDS100SpreadMode::Proximity;
										M->MarkPackageDirty();
									}
								return FReply::Handled();
							})
							[ SNew(STextBlock).Text(LOCTEXT("Proximity", "Proximity")).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor::White) ]
						]
					]

					// Proximity controls — only shown in Proximity mode
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox)
						.Visibility_Lambda([WeakMgr, BIdx]()
						{
							if (ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
									return M->ObjectBindings[BIdx].DS100SpreadMode == EDS100SpreadMode::Proximity
										? EVisibility::Visible : EVisibility::Collapsed;
							return EVisibility::Collapsed;
						})
						[
							SNew(SHorizontalBox)

							// Min label + spinner
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 3.f, 0.f)
							[ SNew(STextBlock).Text(LOCTEXT("MinLabel", "Min")).ColorAndOpacity(FLinearColor(0.55f,0.55f,0.55f)).Font(FAppStyle::GetFontStyle("SmallFont")) ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
							[
								SNew(SBox).WidthOverride(48.f)
								[
									SNew(SNumericEntryBox<float>)
									.Value_Lambda([WeakMgr, BIdx]() -> TOptional<float>
									{
										if (ASpatialFabricManagerActor* M = WeakMgr.Get())
											if (M->ObjectBindings.IsValidIndex(BIdx))
												return M->ObjectBindings[BIdx].DS100SpreadMin;
										return TOptional<float>();
									})
									.OnValueCommitted_Lambda([WeakMgr, BIdx](float Val, ETextCommit::Type)
									{
										if (ASpatialFabricManagerActor* M = WeakMgr.Get())
											if (M->ObjectBindings.IsValidIndex(BIdx))
											{
												M->ObjectBindings[BIdx].DS100SpreadMin = FMath::Clamp(Val, 0.f, 1.f);
												M->MarkPackageDirty();
											}
									})
									.MinValue(0.f).MaxValue(1.f).AllowSpin(true)
									.Font(FAppStyle::GetFontStyle("SmallFont"))
								]
							]

							// Max label + spinner
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 3.f, 0.f)
							[ SNew(STextBlock).Text(LOCTEXT("MaxLabel", "Max")).ColorAndOpacity(FLinearColor(0.55f,0.55f,0.55f)).Font(FAppStyle::GetFontStyle("SmallFont")) ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
							[
								SNew(SBox).WidthOverride(48.f)
								[
									SNew(SNumericEntryBox<float>)
									.Value_Lambda([WeakMgr, BIdx]() -> TOptional<float>
									{
										if (ASpatialFabricManagerActor* M = WeakMgr.Get())
											if (M->ObjectBindings.IsValidIndex(BIdx))
												return M->ObjectBindings[BIdx].DS100SpreadMax;
										return TOptional<float>();
									})
									.OnValueCommitted_Lambda([WeakMgr, BIdx](float Val, ETextCommit::Type)
									{
										if (ASpatialFabricManagerActor* M = WeakMgr.Get())
											if (M->ObjectBindings.IsValidIndex(BIdx))
											{
												M->ObjectBindings[BIdx].DS100SpreadMax = FMath::Clamp(Val, 0.f, 1.f);
												M->MarkPackageDirty();
											}
									})
									.MinValue(0.f).MaxValue(1.f).AllowSpin(true)
									.Font(FAppStyle::GetFontStyle("SmallFont"))
								]
							]

							// Dist label + spinner
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 3.f, 0.f)
							[ SNew(STextBlock).Text(LOCTEXT("DistLabel", "Dist")).ColorAndOpacity(FLinearColor(0.55f,0.55f,0.55f)).Font(FAppStyle::GetFontStyle("SmallFont")) ]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SBox).WidthOverride(48.f)
								[
									SNew(SNumericEntryBox<float>)
									.Value_Lambda([WeakMgr, BIdx]() -> TOptional<float>
									{
										if (ASpatialFabricManagerActor* M = WeakMgr.Get())
											if (M->ObjectBindings.IsValidIndex(BIdx))
												return M->ObjectBindings[BIdx].DS100ProximityMaxDistance;
										return TOptional<float>();
									})
									.OnValueCommitted_Lambda([WeakMgr, BIdx](float Val, ETextCommit::Type)
									{
										if (ASpatialFabricManagerActor* M = WeakMgr.Get())
											if (M->ObjectBindings.IsValidIndex(BIdx))
											{
												M->ObjectBindings[BIdx].DS100ProximityMaxDistance = FMath::Max(Val, 0.01f);
												M->MarkPackageDirty();
											}
									})
									.MinValue(0.01f).AllowSpin(true)
									.Font(FAppStyle::GetFontStyle("SmallFont"))
								]
							]
						]
					]

					// Spacer
					+ SHorizontalBox::Slot().FillWidth(1.f)

					// "Delay:" label + spinner
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DelayLabel", "Delay:"))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ToolTipText(LOCTEXT("DelayTip", "DS100 delay mode: -1=off (not sent), 0=off, 1=tight, 2=full"))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(48.f)
						[
							SNew(SNumericEntryBox<int32>)
							.Value_Lambda([WeakMgr, BIdx]() -> TOptional<int32>
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
										return M->ObjectBindings[BIdx].DS100DelayMode;
								return TOptional<int32>();
							})
							.OnValueCommitted_Lambda([WeakMgr, BIdx](int32 Val, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
									{
										M->ObjectBindings[BIdx].DS100DelayMode = FMath::Clamp(Val, -1, 2);
										M->MarkPackageDirty();
									}
							})
							.MinValue(-1).MaxValue(2).AllowSpin(true)
							.Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
				]
			]

			// ── ADM-OSC sub-row (visible when ADMOSC is the active format) ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f, 2.f, 4.f, 2.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(FMargin(6.f, 3.f))
			.Visibility_Lambda([WeakMgr]()
			{
				const ASpatialFabricManagerActor* M = WeakMgr.Get();
				return (M && M->ActiveAdapterType == ESpatialAdapterType::ADMOSC)
					? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				SNew(SHorizontalBox)

				// ── Coord mode buttons: xyz / aed / both ──────────────
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SHorizontalBox)
					.ToolTipText(LOCTEXT("ADMCoordTip",
						"ADM-OSC position format.\n"
						"xyz = Cartesian (/adm/obj/{n}/xyz)\n"
						"aed = Polar (/adm/obj/{n}/aed  azimuth / elevation / distance)\n"
						"both = send both messages each frame"))

					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ButtonColorAndOpacity_Lambda([WeakMgr]()
						{
							const ASpatialFabricManagerActor* M = WeakMgr.Get();
							const FSpatialAdapterConfig* C = M ? M->AdapterConfigs.Find((uint8)ESpatialAdapterType::ADMOSC) : nullptr;
							return (C && C->ADMCoordinateMode == EADMCoordinateMode::Cartesian)
								? FLinearColor(0.10f, 0.45f, 0.85f) : FLinearColor(0.18f, 0.18f, 0.18f);
						})
						.OnClicked_Lambda([WeakMgr]() -> FReply
						{
							if (ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::ADMOSC))
								{ C->ADMCoordinateMode = EADMCoordinateMode::Cartesian; M->MarkPackageDirty(); if (M->GetRouter()) M->InitializeAdapters(); }
							return FReply::Handled();
						})
						[ SNew(STextBlock).Text(LOCTEXT("ADMXYZBtn", "xyz")).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor::White) ]
					]

					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ButtonColorAndOpacity_Lambda([WeakMgr]()
						{
							const ASpatialFabricManagerActor* M = WeakMgr.Get();
							const FSpatialAdapterConfig* C = M ? M->AdapterConfigs.Find((uint8)ESpatialAdapterType::ADMOSC) : nullptr;
							return (C && C->ADMCoordinateMode == EADMCoordinateMode::Polar)
								? FLinearColor(0.10f, 0.45f, 0.85f) : FLinearColor(0.18f, 0.18f, 0.18f);
						})
						.OnClicked_Lambda([WeakMgr]() -> FReply
						{
							if (ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::ADMOSC))
								{ C->ADMCoordinateMode = EADMCoordinateMode::Polar; M->MarkPackageDirty(); if (M->GetRouter()) M->InitializeAdapters(); }
							return FReply::Handled();
						})
						[ SNew(STextBlock).Text(LOCTEXT("ADMAEDBtn", "aed")).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor::White) ]
					]

					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ButtonColorAndOpacity_Lambda([WeakMgr]()
						{
							const ASpatialFabricManagerActor* M = WeakMgr.Get();
							const FSpatialAdapterConfig* C = M ? M->AdapterConfigs.Find((uint8)ESpatialAdapterType::ADMOSC) : nullptr;
							return (C && C->ADMCoordinateMode == EADMCoordinateMode::Both)
								? FLinearColor(0.10f, 0.45f, 0.85f) : FLinearColor(0.18f, 0.18f, 0.18f);
						})
						.OnClicked_Lambda([WeakMgr]() -> FReply
						{
							if (ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (FSpatialAdapterConfig* C = M->AdapterConfigs.Find((uint8)ESpatialAdapterType::ADMOSC))
								{ C->ADMCoordinateMode = EADMCoordinateMode::Both; M->MarkPackageDirty(); if (M->GetRouter()) M->InitializeAdapters(); }
							return FReply::Handled();
						})
						[ SNew(STextBlock).Text(LOCTEXT("ADMBothBtn", "both")).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor::White) ]
					]
				]

				// "w:" label
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ADMWLabel", "w:"))
					.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ToolTipText(LOCTEXT("ADMWTip",
						"ADM-OSC horizontal extent /adm/obj/{n}/w [0–1].\n"
						"0 = point source, 1 = full stage width."))
				]

				// w spinner [0..1]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(60.f)
					[
						SNew(SNumericEntryBox<float>)
						.Value_Lambda([WeakMgr, BIdx]() -> TOptional<float>
						{
							if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
									return M->ObjectBindings[BIdx].Width01;
							return TOptional<float>();
						})
						.OnValueCommitted_Lambda([WeakMgr, BIdx](float Val, ETextCommit::Type)
						{
							if (ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
								{
									M->ObjectBindings[BIdx].Width01 = FMath::Clamp(Val, 0.f, 1.f);
									M->MarkPackageDirty();
								}
						})
						.MinValue(0.f).MaxValue(1.f).AllowSpin(true)
						.Font(FAppStyle::GetFontStyle("SmallFont"))
					]
				]

				// "send:" label before optional message toggles
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 4.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ADMOptLabel", "send:"))
					.ColorAndOpacity(FLinearColor(0.45f, 0.45f, 0.45f))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ToolTipText(LOCTEXT("ADMOptTip", "Enable optional ADM-OSC messages for this object."))
				]

				// gain: checkbox toggle + numeric spinner (spinner hidden when disabled)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([WeakMgr, BIdx]() -> ECheckBoxState
						{
							if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
									return M->ObjectBindings[BIdx].bADMSendGain ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							return ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([WeakMgr, BIdx](ECheckBoxState NewState)
						{
							if (ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
								{
									M->ObjectBindings[BIdx].bADMSendGain = (NewState == ECheckBoxState::Checked);
									M->MarkPackageDirty();
								}
						})
						.ToolTipText(LOCTEXT("ADMGainTip",
							"Send /adm/obj/{n}/gain (linear amplitude) each frame."))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f, 0.f, 4.f, 0.f)
					[
						SNew(STextBlock).Text(LOCTEXT("ADMGainLabel", "gain"))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(52.f)
						.Visibility_Lambda([WeakMgr, BIdx]() -> EVisibility
						{
							if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
									return M->ObjectBindings[BIdx].bADMSendGain ? EVisibility::Visible : EVisibility::Collapsed;
							return EVisibility::Collapsed;
						})
						[
							SNew(SNumericEntryBox<float>)
							.Value_Lambda([WeakMgr, BIdx]() -> TOptional<float>
							{
								if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
										return M->ObjectBindings[BIdx].GainLinear;
								return TOptional<float>();
							})
							.OnValueCommitted_Lambda([WeakMgr, BIdx](float Val, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
									{
										M->ObjectBindings[BIdx].GainLinear = FMath::Clamp(Val, 0.f, 2.f);
										M->MarkPackageDirty();
									}
							})
							.MinValue(0.f).MaxValue(2.f).AllowSpin(true)
							.Font(FAppStyle::GetFontStyle("SmallFont"))
							.ToolTipText(LOCTEXT("ADMGainValTip", "Linear gain [0..2]. 1.0 = unity."))
						]
					]
				]

				// mute: "send mute" checkbox + "muted" state toggle button
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SHorizontalBox)
					// checkbox: enable sending the mute message
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([WeakMgr, BIdx]() -> ECheckBoxState
						{
							if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
									return M->ObjectBindings[BIdx].bADMSendMute ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							return ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([WeakMgr, BIdx](ECheckBoxState NewState)
						{
							if (ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
								{
									M->ObjectBindings[BIdx].bADMSendMute = (NewState == ECheckBoxState::Checked);
									M->MarkPackageDirty();
								}
						})
						.ToolTipText(LOCTEXT("ADMMuteTip",
							"Send /adm/obj/{n}/mute (0 = active, 1 = muted) each frame."))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f, 0.f, 4.f, 0.f)
					[
						SNew(STextBlock).Text(LOCTEXT("ADMMuteLabel", "mute"))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
					]
					// toggle button: actual muted state (visible only when send-mute is enabled)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox)
						.Visibility_Lambda([WeakMgr, BIdx]() -> EVisibility
						{
							if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
									return M->ObjectBindings[BIdx].bADMSendMute ? EVisibility::Visible : EVisibility::Collapsed;
							return EVisibility::Collapsed;
						})
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "ToggleButton")
							.ButtonColorAndOpacity_Lambda([WeakMgr, BIdx]() -> FLinearColor
							{
								if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
										return M->ObjectBindings[BIdx].bMuted
											? FLinearColor(0.7f, 0.15f, 0.05f)
											: FLinearColor(0.18f, 0.18f, 0.18f);
								return FLinearColor(0.18f, 0.18f, 0.18f);
							})
							.OnClicked_Lambda([WeakMgr, BIdx]() -> FReply
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
									{
										M->ObjectBindings[BIdx].bMuted = !M->ObjectBindings[BIdx].bMuted;
										M->MarkPackageDirty();
									}
								return FReply::Handled();
							})
							.ToolTipText(LOCTEXT("ADMMuteStateTip",
								"Toggle the muted state sent via /adm/obj/{n}/mute. Red = muted, dark = active."))
							.HAlign(HAlign_Center).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([WeakMgr, BIdx]() -> FText
								{
									if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
										if (M->ObjectBindings.IsValidIndex(BIdx))
											return M->ObjectBindings[BIdx].bMuted
												? LOCTEXT("MutedOn", "MUTED")
												: LOCTEXT("MutedOff", "live");
									return LOCTEXT("MutedOff", "live");
								})
								.Font(FAppStyle::GetFontStyle("SmallFont"))
								.ColorAndOpacity(FLinearColor::White)
							]
						]
					]
				]

				// name: checkbox toggle + text field (field hidden when disabled)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([WeakMgr, BIdx]() -> ECheckBoxState
						{
							if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
									return M->ObjectBindings[BIdx].bADMSendName ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							return ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([WeakMgr, BIdx](ECheckBoxState NewState)
						{
							if (ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
								{
									M->ObjectBindings[BIdx].bADMSendName = (NewState == ECheckBoxState::Checked);
									M->MarkPackageDirty();
								}
						})
						.ToolTipText(LOCTEXT("ADMNameTip",
							"Send /adm/obj/{n}/name each frame. Uses the override text if set, otherwise the actor label."))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f, 0.f, 4.f, 0.f)
					[
						SNew(STextBlock).Text(LOCTEXT("ADMNameLabel", "name"))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(100.f)
						.Visibility_Lambda([WeakMgr, BIdx]() -> EVisibility
						{
							if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
									return M->ObjectBindings[BIdx].bADMSendName ? EVisibility::Visible : EVisibility::Collapsed;
							return EVisibility::Collapsed;
						})
						[
							SNew(SEditableTextBox)
							.Text_Lambda([WeakMgr, BIdx]() -> FText
							{
								if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
										return FText::FromString(M->ObjectBindings[BIdx].ADMNameOverride);
								return FText::GetEmpty();
							})
							.OnTextCommitted_Lambda([WeakMgr, BIdx](const FText& NewText, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
									{
										M->ObjectBindings[BIdx].ADMNameOverride = NewText.ToString();
										M->MarkPackageDirty();
									}
							})
							.HintText(LOCTEXT("ADMNameHint", "actor label"))
							.Font(FAppStyle::GetFontStyle("SmallFont"))
							.ToolTipText(LOCTEXT("ADMNameFieldTip",
								"Custom name sent as /adm/obj/{n}/name. Leave empty to use the actor label."))
						]
					]
				]

				// Spacer
				+ SHorizontalBox::Slot().FillWidth(1.f)
			]
		]

		// ── SpaceMap Go sub-row (visible only when SpaceMapGo is the active format) ──
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f, 2.f, 4.f, 2.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.Padding(FMargin(6.f, 3.f))
				.Visibility_Lambda([WeakMgr]()
				{
					const ASpatialFabricManagerActor* M = WeakMgr.Get();
					return (M && M->ActiveAdapterType == ESpatialAdapterType::SpaceMapGo)
						? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(SHorizontalBox)

					// â”€â”€ Live position display â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([WeakMgr, BIdx]() -> FText
						{
							if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
								if (M->ObjectBindings.IsValidIndex(BIdx))
								{
									const FSpatialNormalizedState* State = nullptr;
									for (const FSpatialNormalizedState& S : M->GetLastSnapshot().Objects)
										if (S.ObjectID == M->ObjectBindings[BIdx].DefaultObjectID)
											{ State = &S; break; }
									if (State)
									{
										const float SMX = FMath::Clamp(State->StageNormalized.Y * 1000.f, -1000.f, 1000.f);
										const float SMY = FMath::Clamp(State->StageNormalized.X * 1000.f, -1000.f, 1000.f);
										return FText::Format(
											LOCTEXT("SMGLive", "/channel/{0}/position  {1}  {2}"),
											State->ObjectID,
											FText::AsNumber(SMX, &FNumberFormattingOptions()
												.SetMinimumFractionalDigits(0).SetMaximumFractionalDigits(0)),
											FText::AsNumber(SMY, &FNumberFormattingOptions()
												.SetMinimumFractionalDigits(0).SetMaximumFractionalDigits(0)));
									}
								}
							return LOCTEXT("SMGHint", "/channel/{id}/position  X  Y");
						})
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
					]

					// â”€â”€ Spread label â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 4.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SMGSpreadLabel", "Spread"))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
					]

					// â”€â”€ Spread numeric entry (0â€“100) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(52.f)
						[
							SNew(SNumericEntryBox<float>)
							.Value_Lambda([WeakMgr, BIdx]() -> TOptional<float>
							{
								if (const ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
										return M->ObjectBindings[BIdx].Width01 * 100.f;
								return TOptional<float>();
							})
							.OnValueCommitted_Lambda([WeakMgr, BIdx](float Val, ETextCommit::Type)
							{
								if (ASpatialFabricManagerActor* M = WeakMgr.Get())
									if (M->ObjectBindings.IsValidIndex(BIdx))
									{
										M->ObjectBindings[BIdx].Width01 = FMath::Clamp(Val, 0.f, 100.f) / 100.f;
										M->MarkPackageDirty();
									}
							})
							.MinValue(0.f).MaxValue(100.f).AllowSpin(true)
							.Font(FAppStyle::GetFontStyle("SmallFont"))
							.ToolTipText(LOCTEXT("SMGSpreadTip",
								"Spread [0-100]. Logarithmic scale. 14 = optimal default."))
						]
					]

					// â”€â”€ % suffix â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SMGSpreadPct", "%"))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
					]
				]
			]

		];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Binding management
// ─────────────────────────────────────────────────────────────────────────────

void SSpatialFabricPanel::RebuildBindingList()
{
	BindingRows.Reset();
	if (ASpatialFabricManagerActor* Mgr = GetManager())
	{
		for (int32 i = 0; i < Mgr->ObjectBindings.Num(); ++i)
			BindingRows.Add(MakeShared<int32>(i));
	}
	if (BindingListView.IsValid())
		BindingListView->RequestListRefresh();
}

void SSpatialFabricPanel::OnAddSelectedActors()
{
	ASpatialFabricManagerActor* Mgr = GetManager();
	if (!Mgr || !GEditor) return;

	// Build set of already-bound actors
	TSet<AActor*> AlreadyBound;
	for (const FSpatialObjectBinding& B : Mgr->ObjectBindings)
		if (AActor* A = B.TargetActor.Get()) AlreadyBound.Add(A);

	// Compute next auto-ID
	int32 NextID = 1;
	for (const FSpatialObjectBinding& B : Mgr->ObjectBindings)
		NextID = FMath::Max(NextID, B.DefaultObjectID + 1);

	TArray<UObject*> Selected;
	GEditor->GetSelectedActors()->GetSelectedObjects(AActor::StaticClass(), Selected);

	bool bAnyAdded = false;
	for (UObject* Obj : Selected)
	{
		AActor* Actor = Cast<AActor>(Obj);
		if (!Actor || !IsValid(Actor))              continue;
		if (Cast<ASpatialFabricManagerActor>(Actor)) continue;
		if (Cast<ASpatialStageVolume>(Actor))        continue;
		if (AlreadyBound.Contains(Actor))            continue;

		FSpatialObjectBinding Binding;
		Binding.Label            = Actor->GetActorLabel();
		Binding.TargetActor      = Actor;
		Binding.CachedActorLabel = Actor->GetActorLabel();
		Binding.DefaultObjectID  = NextID++;
		Binding.bEnabled         = true;

		// Route to the currently selected protocol format
		FSpatialAdapterTargetEntry T;
		T.AdapterType      = Mgr->ActiveAdapterType;
		T.ObjectIDOverride = -1;
		T.bEnabled         = true;
		Binding.Targets.Add(T);

		Mgr->ObjectBindings.Add(MoveTemp(Binding));
		bAnyAdded = true;
	}

	if (bAnyAdded)
	{
		Mgr->MarkPackageDirty();
		RebuildBindingList();
	}
}

int32 SSpatialFabricPanel::GetNewActorCount() const
{
	if (!GEditor) return 0;

	TSet<AActor*> AlreadyBound;
	if (ASpatialFabricManagerActor* Mgr = GetManager())
		for (const FSpatialObjectBinding& B : Mgr->ObjectBindings)
			if (AActor* A = B.TargetActor.Get()) AlreadyBound.Add(A);

	TArray<UObject*> Selected;
	GEditor->GetSelectedActors()->GetSelectedObjects(AActor::StaticClass(), Selected);

	int32 Count = 0;
	for (UObject* Obj : Selected)
	{
		AActor* A = Cast<AActor>(Obj);
		if (!A || !IsValid(A))              continue;
		if (Cast<ASpatialFabricManagerActor>(A)) continue;
		if (Cast<ASpatialStageVolume>(A))    continue;
		if (AlreadyBound.Contains(A))       continue;
		++Count;
	}
	return Count;
}

FText SSpatialFabricPanel::GetAddButtonText() const
{
	const int32 N = GetNewActorCount();
	if (N <= 0) return LOCTEXT("AddBtnNone", "+ Add Selected Actors");
	return FText::Format(LOCTEXT("AddBtnN", "+ Add Selected Actors ({0})"), N);
}

void SSpatialFabricPanel::RemoveBinding(int32 Index)
{
	ASpatialFabricManagerActor* Mgr = GetManager();
	if (!Mgr || !Mgr->ObjectBindings.IsValidIndex(Index)) return;
	Mgr->ObjectBindings.RemoveAt(Index);
	Mgr->MarkPackageDirty();
	RebuildBindingList();
}

void SSpatialFabricPanel::SetActiveFormat(ESpatialAdapterType Type)
{
	ASpatialFabricManagerActor* Mgr = GetManager();
	if (!Mgr) return;

	Mgr->ActiveAdapterType = Type;

	// Enable only the selected adapter; disable all Phase-1 adapters' configs
	static const ESpatialAdapterType Phase1[] = {
		ESpatialAdapterType::ADMOSC,
		ESpatialAdapterType::Custom,
	};
	for (const ESpatialAdapterType T : Phase1)
	{
		if (FSpatialAdapterConfig* Config = Mgr->AdapterConfigs.Find((uint8)T))
		{
			Config->bEnabled = (T == Type);
			// Ensure ADM-OSC defaults to Polar so elevation (/aed) is sent out of the box.
			if (T == ESpatialAdapterType::ADMOSC && Config->ADMCoordinateMode == EADMCoordinateMode::Cartesian)
				Config->ADMCoordinateMode = EADMCoordinateMode::Polar;
		}
	}

	// Re-route every existing binding to the new format
	for (FSpatialObjectBinding& B : Mgr->ObjectBindings)
	{
		B.Targets.Empty();
		FSpatialAdapterTargetEntry Entry;
		Entry.AdapterType      = Type;
		Entry.ObjectIDOverride = -1;
		Entry.bEnabled         = true;
		B.Targets.Add(Entry);
	}

	Mgr->MarkPackageDirty();

	// Re-initialize the router if we are in PIE so the new bEnabled state takes effect
	if (Mgr->GetRouter())
		Mgr->InitializeAdapters();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Manager resolution
// ─────────────────────────────────────────────────────────────────────────────

ASpatialFabricManagerActor* SSpatialFabricPanel::GetManager() const
{
	UWorld* World = GEditor
		? (GEditor->PlayWorld ? GEditor->PlayWorld.Get()
		                      : GEditor->GetEditorWorldContext().World())
		: nullptr;
	if (!World) return nullptr;

	for (TActorIterator<ASpatialFabricManagerActor> It(World); It; ++It)
		return *It;
	return nullptr;
}

void SSpatialFabricPanel::RefreshFromManager()
{
	RebuildBindingList();
}

void SSpatialFabricPanel::RebuildSVList()
{
	SVActorList.Reset();
	UWorld* World = GEditor
		? (GEditor->PlayWorld ? GEditor->PlayWorld.Get()
		                      : GEditor->GetEditorWorldContext().World())
		: nullptr;
	if (!World) { return; }
	for (TActorIterator<ASpatialStageVolume> It(World); It; ++It)
	{
		SVActorList.Add(*It);
	}
}

EActiveTimerReturnType SSpatialFabricPanel::OnRefreshTimer(
	double InCurrentTime, float InDeltaTime)
{
	// Only rebuild the row list when the binding count changes.
	// Row widgets use _Lambda callbacks that re-query live each paint frame,
	// so they never need a full row rebuild just because values changed.
	const ASpatialFabricManagerActor* Mgr = GetManager();
	const int32 CurCount = Mgr ? Mgr->ObjectBindings.Num() : 0;
	if (CurCount != LastKnownBindingCount)
	{
		LastKnownBindingCount = CurCount;
		RebuildBindingList();
	}

	RebuildSVList();
	RefreshLog();

	// Push the latest snapshot into the radar widget and trigger a repaint.
	// We push here (not in OnPaint) so the widget always reads from the correct
	// PIE-world manager, regardless of which world was active at construction time.
	if (RadarView.IsValid())
	{
		if (ASpatialFabricManagerActor* RadarMgr = GetManager())
		{
			TSharedPtr<SRadarView> Radar = StaticCastSharedPtr<SRadarView>(RadarView);
			Radar->SetSnapshot(RadarMgr->GetLastSnapshot());

			if (ASpatialStageVolume* SV = RadarMgr->StageVolume.Get())
			{
				Radar->SetStageDimensions(SV->PhysicalWidthMeters, SV->PhysicalDepthMeters);
			}
		}
		RadarView->Invalidate(EInvalidateWidgetReason::Paint);
	}

	return EActiveTimerReturnType::Continue;
}

// Stub implementations for tab visual helpers (used by lambda captures)
FSlateColor SSpatialFabricPanel::GetTabColor(int32 TabIdx) const
{
	return (TabSwitcher.IsValid() && TabSwitcher->GetActiveWidgetIndex() == TabIdx)
		? FSlateColor(FLinearColor(0.15f, 0.45f, 0.80f))
		: FSlateColor(FLinearColor(0.12f, 0.12f, 0.12f));
}

FSlateFontInfo SSpatialFabricPanel::GetTabFont(int32 TabIdx) const
{
	return (TabSwitcher.IsValid() && TabSwitcher->GetActiveWidgetIndex() == TabIdx)
		? FAppStyle::GetFontStyle("BoldFont")
		: FAppStyle::GetFontStyle("NormalFont");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tab — Radar
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SSpatialFabricPanel::BuildRadarTab()
{
	TSharedPtr<SRadarView> View;
	SAssignNew(View, SRadarView);
	RadarView = View;

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 4.f, 8.f, 4.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("RadarHelp",
				"Top-down view — front at top, stage-left at left. "
				"Rectangle matches stage volume aspect ratio; coordinates in meters."))
			.AutoWrapText(true)
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(4.f)
		[ View.ToSharedRef() ];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tab — Output
// ─────────────────────────────────────────────────────────────────────────────

/** Format a preview line for one object given the active adapter type. */
static FString FormatOutputPreview(
	ESpatialAdapterType AdapterType,
	const FSpatialNormalizedState& Obj,
	bool bDS100Absolute,
	EADMCoordinateMode ADMCoordMode = EADMCoordinateMode::Cartesian)
{
	const int32 ID = Obj.ObjectID;
	switch (AdapterType)
	{
	case ESpatialAdapterType::ADMOSC:
	{
		const FVector& N = Obj.StageNormalized;
		// ADM-OSC axis mapping: ADM X = our Y (right), ADM Y = our X (front), ADM Z = our Z (up)
		const float ADMX = (float)N.Y;
		const float ADMY = (float)N.X;
		const float ADMZ = (float)N.Z;

		FString Lines;
		if (ADMCoordMode == EADMCoordinateMode::Cartesian || ADMCoordMode == EADMCoordinateMode::Both)
			Lines += FString::Printf(TEXT("/adm/obj/%d/xyz  %.3f %.3f %.3f\n"), ID, ADMX, ADMY, ADMZ);
		if (ADMCoordMode == EADMCoordinateMode::Polar || ADMCoordMode == EADMCoordinateMode::Both)
		{
			const FVector Polar = FSpatialMath::CartesianToPolar(N);
			const float Dist = FMath::Clamp((float)Polar.Z / FMath::Sqrt(3.0f), 0.0f, 1.0f);
			Lines += FString::Printf(TEXT("/adm/obj/%d/aed  %.1f %.1f %.3f\n"), ID, (float)Polar.X, (float)Polar.Y, Dist);
		}
		Lines += FString::Printf(TEXT("/adm/obj/%d/w  %.3f"), ID, Obj.Width01);
		return Lines;
	}
	case ESpatialAdapterType::DS100:
	{
		if (bDS100Absolute)
		{
			const FVector& M = Obj.StageMeters;
			return FString::Printf(TEXT("/dbaudio1/positioning/source_position/%d  %.3f %.3f %.3f"), ID, M.X, M.Y, M.Z);
		}
		else
		{
			const FVector2D Mapped = FSpatialMath::NormalizedToDS100Mapped(Obj.StageNormalized);
			return FString::Printf(TEXT("/dbaudio1/coordinatemapping/source_position_xy/1/%d  %.3f %.3f"), ID, Mapped.X, Mapped.Y);
		}
	}
	case ESpatialAdapterType::SpaceMapGo:
	{
		const float SMX = FMath::Clamp((float)Obj.StageNormalized.Y * 1000.f, -1000.f, 1000.f);
		const float SMY = FMath::Clamp((float)Obj.StageNormalized.X * 1000.f, -1000.f, 1000.f);
		const float Spread = FMath::Clamp(Obj.Width01 * 100.f, 0.f, 100.f);
		return FString::Printf(TEXT("/channel/%d/position  %.0f %.0f\n/channel/%d/spread  %.0f"), ID, SMX, SMY, ID, Spread);
	}
	case ESpatialAdapterType::Custom:
	{
		const FVector& N = Obj.StageNormalized;
		return FString::Printf(TEXT("/source/%d/position  %.3f %.3f %.3f"), ID, N.X, N.Y, N.Z);
	}
	default:
		return FString::Printf(TEXT("[%d] %.3f %.3f %.3f"), ID,
			Obj.StageNormalized.X, Obj.StageNormalized.Y, Obj.StageNormalized.Z);
	}
}

TSharedRef<SWidget> SSpatialFabricPanel::BuildOutputTab()
{
	return SNew(SVerticalBox)

		// ── Per-object state preview header + debug toggle ───────────────
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f, 4.f, 6.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OutputHeader", "Live Output Preview"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([]() -> ECheckBoxState
				{
					return GetDefault<USpatialFabricSettings>()->bEnableDebugMessages
						? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					USpatialFabricSettings* S = GetMutableDefault<USpatialFabricSettings>();
					S->bEnableDebugMessages = (NewState == ECheckBoxState::Checked);
					S->SaveConfig();
				})
				.ToolTipText(LOCTEXT("DebugMsgTip",
					"Enable verbose debug messages in the Output Log\n"
					"(OSC connect/disconnect, missing-actor warnings).\n"
					"Off by default to keep the log clean."))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DebugMsgLabel", "Debug messages"))
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.ColorAndOpacity(FLinearColor(0.65f, 0.65f, 0.65f))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(300.f)
		.Padding(6.f, 0.f, 6.f, 4.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					ASpatialFabricManagerActor* Mgr = GetManager();
					if (!Mgr) { return LOCTEXT("OutputNoMgr", "No SpatialFabricManagerActor in level"); }

					const FSpatialFrameSnapshot& Snap = Mgr->GetLastSnapshot();
					if (SFArrayIsEmpty(Snap.Objects)) { return LOCTEXT("OutputEmpty", "No objects tracked"); }

					const ESpatialAdapterType Type = Mgr->ActiveAdapterType;
					const uint8 TypeKey = static_cast<uint8>(Type);
					const FSpatialAdapterConfig* Cfg = Mgr->AdapterConfigs.Find(TypeKey);
					const bool bAbsolute = Cfg ? Cfg->bDS100AbsoluteMode : true;
					const EADMCoordinateMode ADMCoordMode = Cfg
						? Cfg->ADMCoordinateMode
						: EADMCoordinateMode::Cartesian;

					FString Result;
					for (const FSpatialNormalizedState& Obj : Snap.Objects)
					{
						const FString Muted = Obj.bMuted ? TEXT(" [MUTED]") : TEXT("");
						Result += FString::Printf(TEXT("%s (ID %d)%s\n  %s\n"),
							*Obj.Label, Obj.ObjectID, *Muted,
							*FormatOutputPreview(Type, Obj, bAbsolute, ADMCoordMode));
					}

					if (Snap.bHasListener)
					{
						const FVector& LP = Snap.ListenerNormalized;
						const FRotator& LR = Snap.ListenerYPR;
						Result += FString::Printf(TEXT("\nListener\n  xyz: %.3f %.3f %.3f  ypr: %.1f %.1f %.1f"),
							LP.X, LP.Y, LP.Z, LR.Yaw, LR.Pitch, LR.Roll);
					}

					return FText::FromString(Result);
				})
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.AutoWrapText(false)
				.ColorAndOpacity(FLinearColor(0.8f, 0.9f, 0.7f))
			]
		]

		// ── Custom OSC Send ───────────────────────────────────────────────
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f, 4.f, 6.f, 2.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(FMargin(6.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CustomOSCLabel", "Custom OSC:"))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SAssignNew(CustomAddressBox, SEditableTextBox)
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.HintText(LOCTEXT("CustomOSCHint", "/address (e.g. /trigger/5)"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("SendCustom", "Send"))
					.OnClicked_Lambda([this]() -> FReply
					{
						ASpatialFabricManagerActor* Mgr = GetManager();
						if (Mgr && CustomAddressBox.IsValid())
						{
							const FString Addr = CustomAddressBox->GetText().ToString().TrimStartAndEnd();
							if (!Addr.IsEmpty())
							{
								Mgr->SendCustomOSC(Addr, {}, {}, {});
								RefreshLog();
							}
						}
						return FReply::Handled();
					})
					.ToolTipText(LOCTEXT("SendCustomTip",
						"Send the address to CustomTargetIP:CustomTargetPort.\n"
						"Use Blueprint SendCustomOSC for messages with arguments."))
				]
			]
		]

		// ── Message log header + clear button ────────────────────────────
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f, 4.f, 6.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LogHeader", "Protocol Log"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearLog", "Clear"))
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.OnClicked_Lambda([this]() -> FReply
				{
					LogItems.Empty();
					if (LogListView.IsValid()) LogListView->RequestListRefresh();
					return FReply::Handled();
				})
			]
		]

		// ── Scrolling log list ───────────────────────────────────────────
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(6.f, 0.f, 6.f, 4.f)
		[
			SAssignNew(LogListView, SListView<TSharedPtr<FSpatialFabricLogEntry>>)
			.ListItemsSource(&LogItems)
			.OnGenerateRow(this, &SSpatialFabricPanel::GenerateLogRow)
			.SelectionMode(ESelectionMode::None)
		];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Output tab — log helpers
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<ITableRow> SSpatialFabricPanel::GenerateLogRow(
	TSharedPtr<FSpatialFabricLogEntry>       Item,
	const TSharedRef<STableViewBase>&         OwnerTable)
{
	if (!Item.IsValid())
		return SNew(STableRow<TSharedPtr<FSpatialFabricLogEntry>>, OwnerTable);

	return SNew(STableRow<TSharedPtr<FSpatialFabricLogEntry>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)
			[ SNew(STextBlock).Text(FText::FromString(Item->Timestamp)).ColorAndOpacity(FLinearColor(0.45f, 0.45f, 0.45f)) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
			[ SNew(STextBlock).Text(FText::FromString(Item->Adapter)).ColorAndOpacity(FLinearColor(0.4f, 0.8f, 1.f)) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f)
			[ SNew(STextBlock).Text(FText::FromString(Item->Address)) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
			[ SNew(STextBlock).Text(FText::FromString(Item->ValueStr)).ColorAndOpacity(FLinearColor(0.8f, 1.f, 0.4f)) ]
		];
}

void SSpatialFabricPanel::RefreshLog()
{
	ASpatialFabricManagerActor* Manager = GetManager();
	if (!Manager) return;

	const TArray<FSpatialFabricLogEntry> Recent = Manager->GetRecentLog(100);
	LogItems.Empty(Recent.Num());
	for (const FSpatialFabricLogEntry& E : Recent)
		LogItems.Add(MakeShared<FSpatialFabricLogEntry>(E));

	if (LogListView.IsValid())
	{
		LogListView->RequestListRefresh();
		LogListView->ScrollToBottom();
	}
}

#undef LOCTEXT_NAMESPACE
