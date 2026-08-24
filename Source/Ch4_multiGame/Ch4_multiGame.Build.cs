// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Ch4_multiGame : ModuleRules
{
	public Ch4_multiGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"ModelViewViewModel",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Ch4_multiGame",
			"Ch4_multiGame/Variant_Platforming",
			"Ch4_multiGame/Variant_Platforming/Animation",
			"Ch4_multiGame/Variant_Combat",
			"Ch4_multiGame/Variant_Combat/AI",
			"Ch4_multiGame/Variant_Combat/Animation",
			"Ch4_multiGame/Variant_Combat/Gameplay",
			"Ch4_multiGame/Variant_Combat/Interfaces",
			"Ch4_multiGame/Variant_Combat/UI",
			"Ch4_multiGame/Variant_SideScrolling",
			"Ch4_multiGame/Variant_SideScrolling/AI",
			"Ch4_multiGame/Variant_SideScrolling/Gameplay",
			"Ch4_multiGame/Variant_SideScrolling/Interfaces",
			"Ch4_multiGame/Variant_SideScrolling/UI",
			"Ch4_multiGame/UI/MainMenu",
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
