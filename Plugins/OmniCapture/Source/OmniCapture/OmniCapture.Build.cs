using UnrealBuildTool;
using System.IO;

public class OmniCapture : ModuleRules
{
    public OmniCapture(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",
            "RHI",
            "Projects",
            "Slate",
            "SlateCore",
            "UMG",
            "ImageWriteQueue",
            "Json",
            "JsonUtilities"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "CinematicCamera",
            "InputCore",
            "HeadMountedDisplay",
            "AudioMixer",
            "AudioExtensions",
            "DeveloperSettings",
            "ImageCore",
            "ImageWrapper",
            "Renderer"
        });

        PrivateDefinitions.Add("WITH_OMNI_NVENC=0");
    }
}

