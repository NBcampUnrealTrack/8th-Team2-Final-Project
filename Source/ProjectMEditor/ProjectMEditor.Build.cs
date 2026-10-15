using UnrealBuildTool;

public class ProjectMEditor : ModuleRules
{
	public ProjectMEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AssetRegistry",
			"BlueprintGraph",
			"Kismet",
			"KismetCompiler",
			"Paper2D",
			"PaperZD",
			"PaperZDEditor",
			"ProjectM"
		});
	}
}
