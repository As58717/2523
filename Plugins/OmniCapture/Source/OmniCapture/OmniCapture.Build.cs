using UnrealBuildTool;

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
            "JsonUtilities",
            "AVEncoder"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "AudioExtensions",
            "AudioMixer",
            "CinematicCamera",
            "DeveloperSettings",
            "HeadMountedDisplay",
            "ImageCore",
            "ImageWrapper",
            "InputCore",
            "Renderer"
        });

        AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib", "UElibPNG");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("D3D12RHI");
        }

        PrivateDefinitions.Add("WITH_OMNI_NVENC=0");
    }
}

