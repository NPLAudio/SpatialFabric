// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"

/**
 * FSpatialFabricEditorModule
 *
 * Editor modules load only in Unreal Editor — not in packaged games. This one
 * registers a Window tab and a tick object so the panel updates without PIE.
 *
 * Editor-only module for SpatialFabric.
 *
 * Responsibilities:
 *  - Register a nomad dockable tab ("SpatialFabricPanel") under Window menu
 *  - Spawn SSpatialFabricPanel inside that tab
 *  - Drive ProcessFrame in editor (not PIE) via FSpatialFabricEditorTickable
 *  - Ensure ASpatialFabricManagerActor exists in PIE world on play start
 */
class FSpatialFabricEditorModule : public IModuleInterface
{
public:
	static const FName SpatialFabricTabName;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Open (or bring to front) the SpatialFabric panel tab. */
	static void OpenPanel();

private:
	void RegisterTab();
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);

	TSharedPtr<class FTabManager::FLayout> TabLayout;

	FDelegateHandle PIEStartedHandle;
};
