// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "UI/SSpatialFabricPanel.h"
#include "SpatialFabricManagerActor.h"
#include "SpatialStageVolume.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SSpatialFabricPanel"

// ─────────────────────────────────────────────────────────────────────────────

void SSpatialFabricPanel::Construct(const FArguments& InArgs)
{
	// Register a recurring timer to refresh the log
	RegisterActiveTimer(0.25f,
		FWidgetActiveTimerDelegate::CreateSP(this, &SSpatialFabricPanel::OnRefreshTimer));

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(4.f))
		[
			SNew(SVerticalBox)

			// ── Header ───────────────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Header", "Spatial Fabric — Multi-Protocol Spatial Control"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			// ── Tab strip (manual tabs via SWidgetSwitcher) ──────────────────
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SSplitter)
				.Orientation(EOrientation::Orient_Vertical)

				+ SSplitter::Slot()
				.Value(0.6f)
				[
					SNew(SVerticalBox)

					// Quick-nav buttons
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("StageTab", "Stage"))
							.ToolTipText(LOCTEXT("StageTip",
								"Configure the Stage Volume and listener actor"))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("ObjectsTab", "Objects"))
							.ToolTipText(LOCTEXT("ObjectsTip",
								"Manage spatial object bindings"))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("AdaptersTab", "Adapters"))
							.ToolTipText(LOCTEXT("AdaptersTip",
								"Configure protocol adapter endpoints"))
						]
					]

					// Content area — Stage tab shown by default
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							BuildStageTab()
						]
					]
				]

				// ── Log ───────────────────────────────────────────────────────
				+ SSplitter::Slot()
				.Value(0.4f)
				[
					BuildLogTab()
				]
			]
		]
	];
}

// ─── Tab builders ─────────────────────────────────────────────────────────

TSharedRef<SWidget> SSpatialFabricPanel::BuildStageTab()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StageHeader", "Stage Volume"))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 2.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StageHelp",
				"Place an ASpatialStageVolume actor in the level and assign it via the "
				"StageVolume property on the SpatialFabricManagerActor Details panel.\n\n"
				"The box defines your physical audio space — all object coordinates "
				"are normalized to [-1, 1] within its bounds.\n\n"
				"Assign a ListenerActor (player pawn or camera) on the stage volume "
				"for listener-relative / binaural coordinate output."))
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 2.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("SpawnStage", "Spawn Stage Volume in Level"))
			.OnClicked_Lambda([]() -> FReply
			{
				if (GEditor)
				{
					UWorld* W = GEditor->GetEditorWorldContext().World();
					if (W) { W->SpawnActor<ASpatialStageVolume>(); }
				}
				return FReply::Handled();
			})
		];
}

TSharedRef<SWidget> SSpatialFabricPanel::BuildObjectsTab()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ObjectsHeader", "Object Bindings"))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 2.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ObjectsHelp",
				"Add FSpatialObjectBinding entries to the SpatialFabricManagerActor's "
				"ObjectBindings array via the Details panel.  Set each binding's "
				"TargetActor, DefaultObjectID, and add FSpatialAdapterTargetEntry items "
				"for each protocol adapter that should receive this object's position."))
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SSpatialFabricPanel::BuildAdaptersTab()
{
	// Names for display
	static const TArray<FText> AdapterLabels = {
		LOCTEXT("ADM",    "ADM-OSC (L-ISA)"),
		LOCTEXT("DS100",  "d&b DS100"),
		LOCTEXT("RTTrPM", "RTTrPM"),
		LOCTEXT("QLabObj","QLab Object"),
		LOCTEXT("QLabCue","QLab Cue"),
		LOCTEXT("SMG",    "SpaceMap Go"),
		LOCTEXT("TiMax",  "TiMax"),
	};

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	Box->AddSlot().AutoHeight().Padding(4.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("AdaptersHeader", "Adapter Endpoints"))
		.Font(FAppStyle::GetFontStyle("BoldFont"))
	];

	Box->AddSlot().AutoHeight().Padding(4.f, 2.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("AdaptersHelp",
			"Configure IP, port, and enable state for each adapter in the "
			"SpatialFabricManagerActor's AdapterConfigs map via the Details panel."))
		.AutoWrapText(true)
	];

	for (const FText& Label : AdapterLabels)
	{
		Box->AddSlot().AutoHeight().Padding(2.f, 1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[ SNew(STextBlock).Text(Label) ]
		];
	}

	return Box;
}

TSharedRef<SWidget> SSpatialFabricPanel::BuildLogTab()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LogHeader", "Protocol Log"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearLog", "Clear"))
				.OnClicked_Lambda([this]() -> FReply
				{
					LogItems.Empty();
					if (LogListView.IsValid()) { LogListView->RequestListRefresh(); }
					return FReply::Handled();
				})
			]
		]

		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(LogListView, SListView<TSharedPtr<FSpatialFabricLogEntry>>)
			.ListItemsSource(&LogItems)
			.OnGenerateRow(this, &SSpatialFabricPanel::GenerateLogRow)
			.SelectionMode(ESelectionMode::None)
		];
}

// ─── Log helpers ─────────────────────────────────────────────────────────

TSharedRef<ITableRow> SSpatialFabricPanel::GenerateLogRow(
	TSharedPtr<FSpatialFabricLogEntry> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	if (!Item.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FSpatialFabricLogEntry>>, OwnerTable);
	}

	return SNew(STableRow<TSharedPtr<FSpatialFabricLogEntry>>, OwnerTable)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Timestamp))
				.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Adapter))
				.ColorAndOpacity(FLinearColor(0.4f, 0.8f, 1.f))
			]

			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Address))
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->ValueStr))
				.ColorAndOpacity(FLinearColor(0.8f, 1.f, 0.4f))
			]
		];
}

void SSpatialFabricPanel::RefreshLog()
{
	ASpatialFabricManagerActor* Manager = GetManager();
	if (!Manager) { return; }

	const TArray<FSpatialFabricLogEntry> Recent = Manager->GetRecentLog(100);

	LogItems.Empty(Recent.Num());
	for (const FSpatialFabricLogEntry& Entry : Recent)
	{
		LogItems.Add(MakeShared<FSpatialFabricLogEntry>(Entry));
	}

	if (LogListView.IsValid())
	{
		LogListView->RequestListRefresh();
		LogListView->ScrollToBottom();
	}
}

// ─── Manager resolution ───────────────────────────────────────────────────

ASpatialFabricManagerActor* SSpatialFabricPanel::GetManager() const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { return nullptr; }

	for (TActorIterator<ASpatialFabricManagerActor> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void SSpatialFabricPanel::RefreshFromManager()
{
	RefreshLog();
}

EActiveTimerReturnType SSpatialFabricPanel::OnRefreshTimer(
	double InCurrentTime, float InDeltaTime)
{
	RefreshFromManager();
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
