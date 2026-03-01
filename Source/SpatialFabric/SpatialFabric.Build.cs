// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

using UnrealBuildTool;

public class SpatialFabric : ModuleRules
{
	public SpatialFabric(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OSC",
			"LiveOSC",
			"Sockets",
			"Networking",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
		});
	}
}
