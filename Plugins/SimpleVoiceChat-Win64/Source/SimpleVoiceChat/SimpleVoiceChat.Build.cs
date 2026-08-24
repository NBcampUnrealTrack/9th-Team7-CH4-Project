using UnrealBuildTool;

public class SimpleVoiceChat : ModuleRules
{
    public SimpleVoiceChat(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "DeveloperSettings",
                "Engine",
                "InputCore"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "CoreOnline",
                "OnlineSubsystem",
                "OnlineSubsystemUtils",
                "Slate",
                "SlateCore"
            });
    }
}
