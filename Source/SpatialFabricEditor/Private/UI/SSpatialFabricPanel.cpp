// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "UI/SSpatialFabricPanel.h"
#include "SpatialFabricManagerActor.h"
#include "SpatialStageVolume.h"
#include "SpatialFabricTypes.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Selection.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SSpatialFabricPanel"

// ─────────────────────────────────────────────────────────────────────────────
//  Adapter badge definitions (Phase-1 only shown inline)
// ─────────────────────────────────────────────────────────────────────────────

namespace SFBadge
{
	struct FDef { ESpatialAdapterType Type; const TCHAR* Label; float R, G, B; };
	static constexpr FDef All[] = {
		{ ESpatialAdapterType::ADMOSC,     TEXT("ADM"), 0.10f, 0.55f, 1.00f },
		{ ESpatialAdapterType::DS100,      TEXT("DS1"), 1.00f, 0.42f, 0.10f },
		{ ESpatialAdapterType::RTTrPM,     TEXT("RTP"), 0.58f, 0.30f, 0.72f },
		{ ESpatialAdapterType::QLabObject, TEXT("QLB"), 0.10f, 0.72f, 0.32f },
	};
	static constexpr int32 Num = UE_ARRAY_COUNT(All);
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
		+ SWidgetSwitcher::Slot()[ BuildAdaptersTab() ]; // 2

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
				[ MakeTabBtn(LOCTEXT("TabAdapters", "Adapters"), 2) ]
				+ SHorizontalBox::Slot().FillWidth(1.f) // spacer
			]

			// ── Switcher + log splitter ───────────────────────────────────
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SSplitter)
				.Orientation(EOrientation::Orient_Vertical)

				+ SSplitter::Slot().Value(0.62f)
				[ Switcher ]

				+ SSplitter::Slot().Value(0.38f)
				[ BuildLogTab() ]
			]
		]
	];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tab — Stage
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SSpatialFabricPanel::BuildStageTab()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(6.f, 4.f)
		[
			SNew(SVerticalBox)

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
					"3. Set Physical Width / Depth / Height on the Stage Volume to match your real space.\n"
					"4. Optional: tick Auto-Follow Player on the Stage Volume to track the player camera."))
				.AutoWrapText(true)
				.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SpawnStage", "Spawn Stage Volume in Level"))
				.HAlign(HAlign_Center)
				.ButtonColorAndOpacity(FLinearColor(0.15f, 0.45f, 0.80f))
				.OnClicked_Lambda([]() -> FReply
				{
					if (GEditor)
					{
						UWorld* W = GEditor->GetEditorWorldContext().World();
						if (W) { W->SpawnActor<ASpatialStageVolume>(); }
					}
					return FReply::Handled();
				})
			]
		];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tab — Objects
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SSpatialFabricPanel::BuildObjectsTab()
{
	// Build the list view
	TSharedRef<SListView<TSharedPtr<int32>>> ListView =
		SAssignNew(BindingListView, SListView<TSharedPtr<int32>>)
		.ListItemsSource(&BindingRows)
		.OnGenerateRow(this, &SSpatialFabricPanel::GenerateBindingRow)
		.SelectionMode(ESelectionMode::None);

	return SNew(SVerticalBox)

		// ── Toolbar ──────────────────────────────────────────────────────
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f, 4.f, 4.f, 2.f)
		[
			SNew(SHorizontalBox)

			// Add Selected Actors button
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
					return FText::Format(LOCTEXT("BindingCount", "{0} {0}|plural(one=object,other=objects)"), N);
				})
				.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
			]

			// Remove All button
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

			// Empty state message
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

			// The list view
			+ SOverlay::Slot()
			[
				ListView
			]
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

	// ── Badge row (built before Slate DSL to avoid MSVC attribute-specifier confusion) ──
	TSharedRef<SHorizontalBox> BadgeRow = SNew(SHorizontalBox);
	for (int32 i = 0; i < SFBadge::Num; ++i)
	{
		const SFBadge::FDef& Def   = SFBadge::All[i];
		const ESpatialAdapterType BadgeType = Def.Type;
		const FLinearColor FullColor(Def.R, Def.G, Def.B, 1.f);
		const FString BadgeLabel(Def.Label);

		BadgeRow->AddSlot()
			.AutoWidth()
			.Padding(1.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(38.f)
				.HeightOverride(22.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton")
					.ButtonColorAndOpacity_Lambda([this, BIdx, BadgeType, FullColor]()
					{
						return HasAdapterTarget(BIdx, BadgeType)
							? FullColor
							: FLinearColor(FullColor.R, FullColor.G, FullColor.B, 0.22f);
					})
					.HAlign(HAlign_Center).VAlign(VAlign_Center)
					.ToolTipText(FText::Format(
						LOCTEXT("BadgeTip", "Toggle {0} routing for this object"),
						FText::FromString(BadgeLabel)))
					.OnClicked_Lambda([this, BIdx, BadgeType]() -> FReply
					{
						ToggleAdapterTarget(BIdx, BadgeType);
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(FText::FromString(BadgeLabel))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FLinearColor::White)
					]
				]
			];
	}

	// ── Row content ────────────────────────────────────────────────────────
	return SNew(STableRow<TSharedPtr<int32>>, OwnerTable)
		.Padding(FMargin(2.f, 1.f))
		[
			SNew(SHorizontalBox)

			// ── Actor name ────────────────────────────────────────────────
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
								return FLinearColor(1.0f, 0.65f, 0.1f); // orange = unresolved
							if (!M->ObjectBindings[BIdx].bEnabled)
								return FLinearColor(0.45f, 0.45f, 0.45f); // gray = disabled
						}
					return FLinearColor(0.90f, 0.90f, 0.90f); // normal
				})
				.ToolTipText(FText::FromString(
					bResolved ? Actor->GetPathName() : TEXT("Actor not found in level")))
			]

			// ── Object ID ─────────────────────────────────────────────────
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("IDLabel", "ID"))
					.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(50.f)
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

			// ── Adapter badges (pre-built above to avoid attribute-specifier conflict) ──
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f, 4.f, 0.f)
			[ BadgeRow ]

			// ── Enable toggle ─────────────────────────────────────────────
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
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

			// ── Remove button ─────────────────────────────────────────────
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f, 2.f, 0.f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.ToolTipText(LOCTEXT("RemoveBinding", "Remove this binding"))
				.OnClicked_Lambda([this, BIdx]() -> FReply
				{
					RemoveBinding(BIdx);
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("✕")))
					.ColorAndOpacity(FLinearColor(0.85f, 0.35f, 0.35f))
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

		// Auto-add a target entry for each adapter currently enabled
		for (const auto& Pair : Mgr->AdapterConfigs)
		{
			if (Pair.Value.bEnabled)
			{
				FSpatialAdapterTargetEntry T;
				T.AdapterType      = static_cast<ESpatialAdapterType>(Pair.Key);
				T.ObjectIDOverride = -1;
				Binding.Targets.Add(T);
			}
		}

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

void SSpatialFabricPanel::ToggleAdapterTarget(int32 BindingIndex, ESpatialAdapterType Type)
{
	ASpatialFabricManagerActor* Mgr = GetManager();
	if (!Mgr || !Mgr->ObjectBindings.IsValidIndex(BindingIndex)) return;

	FSpatialObjectBinding& B = Mgr->ObjectBindings[BindingIndex];

	const int32 Found = B.Targets.IndexOfByPredicate(
		[Type](const FSpatialAdapterTargetEntry& T) { return T.AdapterType == Type; });

	if (Found != INDEX_NONE)
	{
		B.Targets.RemoveAt(Found);
	}
	else
	{
		FSpatialAdapterTargetEntry T;
		T.AdapterType      = Type;
		T.ObjectIDOverride = -1;
		B.Targets.Add(T);
	}

	Mgr->MarkPackageDirty();
	if (BindingListView.IsValid())
		BindingListView->RebuildList();
}

bool SSpatialFabricPanel::HasAdapterTarget(int32 BindingIndex, ESpatialAdapterType Type) const
{
	const ASpatialFabricManagerActor* Mgr = GetManager();
	if (!Mgr || !Mgr->ObjectBindings.IsValidIndex(BindingIndex)) return false;
	const FSpatialObjectBinding& B = Mgr->ObjectBindings[BindingIndex];
	return B.Targets.ContainsByPredicate(
		[Type](const FSpatialAdapterTargetEntry& T) { return T.AdapterType == Type; });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tab — Adapters
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SSpatialFabricPanel::BuildAdaptersTab()
{
	static const struct { const TCHAR* Name; const TCHAR* Desc; int32 DefaultPort; } Info[] = {
		{ TEXT("ADM-OSC (L-ISA)"),     TEXT("Normalized XYZ  /adm/obj/{n}/xyz"),                9000  },
		{ TEXT("d&b DS100"),           TEXT("source_position or source_position_xy"),           50010 },
		{ TEXT("RTTrPM"),              TEXT("Binary UDP — Centroid Position (IEEE 754 doubles)"), 36700 },
		{ TEXT("QLab Object Audio"),   TEXT("/cue/{id}/object/{name}/position/live"),            53000 },
		{ TEXT("QLab Cue Control"),    TEXT("Cue start/stop triggers"),                         53000 },
		{ TEXT("Meyer SpaceMap Go"),   TEXT("OSC + RTTrPM passthrough"),                        38033 },
		{ TEXT("TiMax SoundHub"),      TEXT("ADM-OSC or custom address template"),               9000 },
	};

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("AdaptersHeader", "Protocol Adapters"))
		.Font(FAppStyle::GetFontStyle("BoldFont"))
	];

	Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("AdaptersHelp",
			"Enable adapters and set IP / port in the SpatialFabricManagerActor "
			"Details panel under AdapterConfigs."))
		.AutoWrapText(true)
		.ColorAndOpacity(FLinearColor(0.65f, 0.65f, 0.65f))
	];

	for (const auto& A : Info)
	{
		Box->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(FMargin(6.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[ SNew(STextBlock).Text(FText::FromString(A.Name)).Font(FAppStyle::GetFontStyle("BoldFont")) ]
					+ SVerticalBox::Slot().AutoHeight()
					[ SNew(STextBlock).Text(FText::FromString(A.Desc)).ColorAndOpacity(FLinearColor(0.6f,0.6f,0.6f)) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("PortFmt", ":{0}"), A.DefaultPort))
					.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
				]
			]
		];
	}

	TSharedRef<SScrollBox> Scroll = SNew(SScrollBox);
	Scroll->AddSlot().Padding(6.f, 4.f)[ Box ];
	return Scroll;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tab — Log
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SSpatialFabricPanel::BuildLogTab()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f, 2.f)
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

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(LogListView, SListView<TSharedPtr<FSpatialFabricLogEntry>>)
			.ListItemsSource(&LogItems)
			.OnGenerateRow(this, &SSpatialFabricPanel::GenerateLogRow)
			.SelectionMode(ESelectionMode::None)
		];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Log helpers
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
	RefreshLog();
}

EActiveTimerReturnType SSpatialFabricPanel::OnRefreshTimer(
	double InCurrentTime, float InDeltaTime)
{
	RefreshFromManager();
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

#undef LOCTEXT_NAMESPACE
