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
            "JsonUtilities"
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
            "MediaUtils",
            "Renderer"
        });

        AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib", "UElibPNG");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDependencyModuleNames.Add("AVEncoder");

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "D3D11RHI",
                "D3D12RHI"
            });

            PrivateDefinitions.Add("WITH_OMNI_NVENC=1");
        }
        else
        {
            PrivateDefinitions.Add("WITH_OMNI_NVENC=0");
        }
    }
}

