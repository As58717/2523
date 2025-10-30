using System.Collections.Generic;
using System.IO;
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

        bool bHasOpenEXR = false;
        List<string> OpenExrCandidates = new List<string>
        {
            Path.Combine(Target.UEThirdPartySourceDirectory, "OpenEXR"),
            Path.Combine(Target.UEThirdPartySourceDirectory, "OpenExr"),
            Path.Combine(Target.UEThirdPartySourceDirectory, "OpenEXR-3"),
        };

        if (Directory.Exists(Target.UEThirdPartySourceDirectory))
        {
            OpenExrCandidates.AddRange(Directory.GetDirectories(Target.UEThirdPartySourceDirectory, "OpenEXR*", SearchOption.TopDirectoryOnly));
            OpenExrCandidates.AddRange(Directory.GetDirectories(Target.UEThirdPartySourceDirectory, "OpenExr*", SearchOption.TopDirectoryOnly));
        }

        foreach (string Candidate in OpenExrCandidates)
        {
            if (!Directory.Exists(Candidate))
            {
                continue;
            }

            if (Directory.GetFiles(Candidate, "*OpenEXR*.Build.cs", SearchOption.AllDirectories).Length > 0 ||
                Directory.GetFiles(Candidate, "*OpenExr*.Build.cs", SearchOption.AllDirectories).Length > 0)
            {
                bHasOpenEXR = true;
                break;
            }
        }

        if (bHasOpenEXR)
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenEXR", "Imath");
        }

        PrivateDefinitions.Add($"WITH_OMNICAPTURE_OPENEXR={(bHasOpenEXR ? 1 : 0)}");

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

