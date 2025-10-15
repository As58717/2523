using UnrealBuildTool;
using System.IO;

public class OmniCapture : ModuleRules
{
    public OmniCapture(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
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
            "ImageCore"
        });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
            bool HasVideoEncoderHeaders = File.Exists(Path.Combine(
                EngineDir, "Source", "Runtime", "AVEncoder", "Public", "VideoEncoder.h"));

            if (HasVideoEncoderHeaders)
            {
                // 如果 `VideoEncoder.h` 存在，则启用 NVENC
                PrivateDefinitions.Add("WITH_OMNI_NVENC=1");
                PublicDependencyModuleNames.AddRange(new string[]
                {
                    "D3D11RHI",
                    "D3D12RHI",
                    "AVEncoder" // 保留在需要时才加
                });
            }
            else
            {
                // 否则禁用 NVENC
                PrivateDefinitions.Add("WITH_OMNI_NVENC=0");
            }
        }
        else
        {
            PrivateDefinitions.Add("WITH_OMNI_NVENC=0");
        }
    }
}

