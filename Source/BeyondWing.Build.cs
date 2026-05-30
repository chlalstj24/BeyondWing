using UnrealBuildTool;
using System.IO;

public class BeyondWing : ModuleRules
{
    public BeyondWing(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "EnhancedInput",
                "UMG",
                "Slate",
                "SlateCore"
            });

        string BasePath = Path.Combine(Target.ProjectFile.Directory.FullName, "Source", "BeyondWing");

        PublicIncludePaths.AddRange(new string[]
        {
            Path.Combine(BasePath, "Core", "Public"),
            Path.Combine(BasePath, "Grid", "Public"),
            Path.Combine(BasePath, "Character", "Public"),
            Path.Combine(BasePath, "Combat", "Public"),
            Path.Combine(BasePath, "UI", "Public")
        });

        PrivateIncludePaths.AddRange(new string[]
        {
            Path.Combine(BasePath, "Core", "Private"),
            Path.Combine(BasePath, "Grid", "Private"),
            Path.Combine(BasePath, "Character", "Private"),
            Path.Combine(BasePath, "Combat", "Private"),
            Path.Combine(BasePath, "UI", "Private")
        });
    }
}