// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClimbingSystem : ModuleRules
{
	public ClimbingSystem(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ClimbingSystem",
			"ClimbingSystem/Variant_Platforming",
			"ClimbingSystem/Variant_Platforming/Animation",
			"ClimbingSystem/Variant_Combat",
			"ClimbingSystem/Variant_Combat/AI",
			"ClimbingSystem/Variant_Combat/Animation",
			"ClimbingSystem/Variant_Combat/Gameplay",
			"ClimbingSystem/Variant_Combat/Interfaces",
			"ClimbingSystem/Variant_Combat/UI",
			"ClimbingSystem/Variant_SideScrolling",
			"ClimbingSystem/Variant_SideScrolling/AI",
			"ClimbingSystem/Variant_SideScrolling/Gameplay",
			"ClimbingSystem/Variant_SideScrolling/Interfaces",
			"ClimbingSystem/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
