// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.
// Editor-only module: depends on SpatialFabric runtime + UnrealEd / Slate for the panel.

using UnrealBuildTool;

public class SpatialFabricEditor : ModuleRules
{
	public SpatialFabricEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"SpatialFabric",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EditorSubsystem",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"EditorStyle",
			"LevelEditor",
			"OSC",
			"WorkspaceMenuStructure",
		});
	}
}
