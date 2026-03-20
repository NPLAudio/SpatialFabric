// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "Modules/ModuleManager.h"

/**
 * Runtime module entry point. IMPLEMENT_MODULE in the .cpp registers this DLL with
 * the engine; StartupModule/ShutdownModule run when the plugin loads/unloads.
 */
class FSpatialFabricModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
