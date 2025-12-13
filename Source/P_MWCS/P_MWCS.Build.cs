using UnrealBuildTool;

public class P_MWCS : ModuleRules
{
    public P_MWCS(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "UMG",
                "Json",
                "JsonUtilities"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "ToolMenus",
                "Settings",
                "DeveloperSettings",
                "UnrealEd",
                "AssetTools",
                "Kismet",
                "KismetCompiler",
                "BlueprintGraph",
                "UMGEditor",
                "Blutility"
            }
        );
    }
}
