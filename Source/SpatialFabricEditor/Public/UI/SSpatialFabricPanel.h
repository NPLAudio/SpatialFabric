// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "SpatialFabricTypes.h"

class ASpatialFabricManagerActor;
class ASpatialStageVolume;

/**
 * SSpatialFabricPanel
 *
 * Dockable Slate editor panel for SpatialFabric.  Four tabs:
 *
 *  [Stage]    — Stage Volume setup and listener assignment.
 *  [Objects]  — Spatial object binding list with Add Selected Actors,
 *               per-binding ID editing, adapter badge toggles, and remove.
 *  [Adapters] — Per-adapter endpoint info (configure via Details panel).
 *  [Log]      — Real-time adapter output log.
 */
class SSpatialFabricPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSpatialFabricPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// ── Manager access ───────────────────────────────────────────────────────
	ASpatialFabricManagerActor* GetManager() const;
	void RefreshFromManager();

	// ── Tab switching ────────────────────────────────────────────────────────
	TSharedPtr<SWidgetSwitcher> TabSwitcher;

	FSlateColor GetTabColor(int32 TabIdx) const;
	FSlateFontInfo GetTabFont(int32 TabIdx) const;

	// ── Tab builders ─────────────────────────────────────────────────────────
	TSharedRef<SWidget> BuildStageTab();
	TSharedRef<SWidget> BuildObjectsTab();
	TSharedRef<SWidget> BuildAdaptersTab();
	TSharedRef<SWidget> BuildLogTab();

	// ── Binding list (Objects tab) ───────────────────────────────────────────

	/** Index-based list items — each int32 is a direct index into
	 *  Manager->ObjectBindings.  Rebuilt whenever bindings change. */
	TArray<TSharedPtr<int32>>                BindingRows;
	TSharedPtr<SListView<TSharedPtr<int32>>> BindingListView;

	void RebuildBindingList();
	void OnAddSelectedActors();
	int32 GetNewActorCount() const;   // selected actors not yet bound
	FText GetAddButtonText() const;

	TSharedRef<ITableRow> GenerateBindingRow(
		TSharedPtr<int32>                  RowIndex,
		const TSharedRef<STableViewBase>&  OwnerTable);

	void RemoveBinding(int32 Index);
	void ToggleAdapterTarget(int32 BindingIndex, ESpatialAdapterType Type);
	bool HasAdapterTarget(int32 BindingIndex, ESpatialAdapterType Type) const;

	// ── Log list ─────────────────────────────────────────────────────────────
	TSharedRef<ITableRow> GenerateLogRow(
		TSharedPtr<FSpatialFabricLogEntry>        Item,
		const TSharedRef<STableViewBase>&          OwnerTable);

	void RefreshLog();

	TArray<TSharedPtr<FSpatialFabricLogEntry>>             LogItems;
	TSharedPtr<SListView<TSharedPtr<FSpatialFabricLogEntry>>> LogListView;

	// ── Timer ────────────────────────────────────────────────────────────────
	EActiveTimerReturnType OnRefreshTimer(double InCurrentTime, float InDeltaTime);
};
