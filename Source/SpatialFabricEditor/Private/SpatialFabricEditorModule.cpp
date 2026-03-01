// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "SpatialFabricEditorModule.h"
#include "UI/SSpatialFabricPanel.h"
#include "SpatialFabricManagerActor.h"

#include "Editor.h"
#include "LevelEditor.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "TickableEditorObject.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "SpatialFabricEditor"

const FName FSpatialFabricEditorModule::SpatialFabricTabName = TEXT("SpatialFabricPanel");

// ─── Editor tickable ──────────────────────────────────────────────────────

/**
 * Drives ProcessFrame when in the Editor (not PIE) so the editor panel
 * shows live data as actor positions change, without needing to press Play.
 */
class FSpatialFabricEditorTickable : public FTickableEditorObject
{
public:
	virtual void Tick(float DeltaTime) override
	{
		if (!GEditor || GEditor->PlayWorld) { return; }
		UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
		if (!EditorWorld) { return; }

		ASpatialFabricManagerActor* Manager =
			ASpatialFabricManagerActor::GetOrSpawnManager(EditorWorld);
		if (Manager)
		{
			Manager->ProcessFrame(DeltaTime);
		}
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSpatialFabricEditorTickable, STATGROUP_Tickables);
	}

	virtual bool IsTickable() const override
	{
		return GEditor != nullptr && !GEditor->PlayWorld;
	}
};

static TSharedPtr<FSpatialFabricEditorTickable> GEditorTickable;

// ─── Module lifecycle ─────────────────────────────────────────────────────

void FSpatialFabricEditorModule::StartupModule()
{
	GEditorTickable = MakeShared<FSpatialFabricEditorTickable>();

	RegisterTab();

	// Ensure manager exists when PIE starts
	PIEStartedHandle = FEditorDelegates::PostPIEStarted.AddLambda([](bool)
	{
		if (GEditor && GEditor->PlayWorld)
		{
			ASpatialFabricManagerActor::GetOrSpawnManager(GEditor->PlayWorld);
		}
	});
}

void FSpatialFabricEditorModule::ShutdownModule()
{
	GEditorTickable.Reset();
	FEditorDelegates::PostPIEStarted.Remove(PIEStartedHandle);

	if (FGlobalTabmanager::Get()->HasTabSpawner(SpatialFabricTabName))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SpatialFabricTabName);
	}
}

void FSpatialFabricEditorModule::RegisterTab()
{
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		SpatialFabricTabName,
		FOnSpawnTab::CreateRaw(this, &FSpatialFabricEditorModule::SpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Spatial Fabric"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Spatial audio show-control — multi-protocol spatial object routing"))
		.SetGroup(MenuStructure.GetLevelEditorCategory());
}

TSharedRef<SDockTab> FSpatialFabricEditorModule::SpawnTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SSpatialFabricPanel)
		];
}

void FSpatialFabricEditorModule::OpenPanel()
{
	FGlobalTabmanager::Get()->TryInvokeTab(SpatialFabricTabName);
}

IMPLEMENT_MODULE(FSpatialFabricEditorModule, SpatialFabricEditor)

#undef LOCTEXT_NAMESPACE
