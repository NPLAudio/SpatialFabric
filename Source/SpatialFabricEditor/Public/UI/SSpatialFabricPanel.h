// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "SpatialFabricTypes.h"

class ASpatialFabricManagerActor;

/**
 * SSpatialFabricPanel
 *
 * Dockable Slate editor panel for SpatialFabric.  Spawned by
 * FSpatialFabricEditorModule when the "Spatial Fabric" tab is opened.
 *
 * Layout (four tabs):
 *
 *  [Stage]    — Assign ASpatialStageVolume, configure physical dimensions,
 *               axis flips, and optional listener actor.
 *
 *  [Objects]  — List of FSpatialObjectBinding entries.  Add/remove actors
 *               from the scene, set IDs, assign adapter targets per binding.
 *
 *  [Adapters] — Per-adapter enable/disable, IP, port, send-rate controls.
 *               One row per ESpatialAdapterType.
 *
 *  [Log]      — Scrolling FSpatialFabricLogEntry list showing real-time
 *               adapter output (address, value, adapter name, timestamp).
 *
 * The panel polls the manager actor each tick via a timer.  Manager
 * resolution uses ASpatialFabricManagerActor::GetOrSpawnManager.
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

	// ── Tab construction ─────────────────────────────────────────────────────
	TSharedRef<SWidget> BuildStageTab();
	TSharedRef<SWidget> BuildObjectsTab();
	TSharedRef<SWidget> BuildAdaptersTab();
	TSharedRef<SWidget> BuildLogTab();

	// ── Log list ─────────────────────────────────────────────────────────────
	TSharedRef<ITableRow> GenerateLogRow(
		TSharedPtr<FSpatialFabricLogEntry> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	void RefreshLog();

	TArray<TSharedPtr<FSpatialFabricLogEntry>> LogItems;
	TSharedPtr<SListView<TSharedPtr<FSpatialFabricLogEntry>>> LogListView;

	// ── Timer ────────────────────────────────────────────────────────────────
	EActiveTimerReturnType OnRefreshTimer(double InCurrentTime, float InDeltaTime);
};
