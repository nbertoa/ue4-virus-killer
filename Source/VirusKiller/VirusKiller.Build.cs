// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirusKiller : ModuleRules
{
    public VirusKiller(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "UMG"           // Required for UUserWidget (PlayerHUD, GameOverHUD)
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",        // Required internally by UMG
            "SlateCore"
        });
    }
}
