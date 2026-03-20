// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.
// Declares the runtime module and its link dependencies (OSC = UE's built-in plugin).

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
