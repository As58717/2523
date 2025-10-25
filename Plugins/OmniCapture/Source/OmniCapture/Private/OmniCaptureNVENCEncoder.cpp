#include "OmniCaptureNVENCEncoder.h"

#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "OmniCaptureTypes.h"
#include "Misc/ScopeLock.h"
#include "Math/UnrealMathUtility.h"
#include "PixelFormat.h"
#include "RHI.h"
#include "UObject/UnrealType.h"
#if __has_include("RHIAdapter.h")
#include "RHIAdapter.h"
#define OMNI_HAS_RHI_ADAPTER 1
#else
#define OMNI_HAS_RHI_ADAPTER 0
#endif
#include "GenericPlatform/GenericPlatformDriver.h"

#if OMNI_WITH_AVENCODER
#include "AVEncoder/VideoEncoderFactory.h"
#include "AVEncoder/VideoEncoderCommon.h"
#include "RHIResources.h"
#include "RHICommandList.h"
#endif

namespace
{
#if PLATFORM_WINDOWS
    #define OMNI_NVENCAPI __stdcall
#else
    #define OMNI_NVENCAPI
#endif

#if OMNI_WITH_AVENCODER
    AVEncoder::EVideoFormat ToVideoFormat(EOmniCaptureColorFormat Format)
    {
        switch (Format)
        {
        case EOmniCaptureColorFormat::NV12:
            return AVEncoder::EVideoFormat::NV12;
        case EOmniCaptureColorFormat::P010:
            return AVEncoder::EVideoFormat::P010;
        case EOmniCaptureColorFormat::BGRA:
        default:
            return AVEncoder::EVideoFormat::BGRA8;
        }
    }

    struct FNVENCHardwareProbeResult
    {
        bool bDllPresent = false;
        bool bApisReady = false;
        bool bSessionOpenable = false;
        bool bSupportsH264 = false;
        bool bSupportsHEVC = false;
        bool bSupportsNV12 = false;
        bool bSupportsP010 = false;
        bool bSupportsBGRA = false;
        FString DllFailureReason;
        FString ApiFailureReason;
        FString SessionFailureReason;
        FString CodecFailureReason;
        FString FormatFailureReason;
        FString ModuleFailureReason;
        FString HardwareFailureReason;
    };

    using FNvEncodeAPIGetMaxSupportedVersion = uint32(OMNI_NVENCAPI*)(uint32*);

    FCriticalSection& GetProbeCacheMutex()
    {
        static FCriticalSection Mutex;
        return Mutex;
    }

    FCriticalSection& GetDllOverrideMutex()
    {
        static FCriticalSection Mutex;
        return Mutex;
    }

    FCriticalSection& GetModuleOverrideMutex()
    {
        static FCriticalSection Mutex;
        return Mutex;
    }

    bool& GetProbeValidFlag()
    {
        static bool bValid = false;
        return bValid;
    }

    FNVENCHardwareProbeResult& GetCachedProbe()
    {
        static FNVENCHardwareProbeResult Cached;
        return Cached;
    }

    FString& GetDllOverridePath()
    {
        static FString OverridePath;
        return OverridePath;
    }

    FString& GetModuleOverridePath()
    {
        static FString OverridePath;
        return OverridePath;
    }

    FString FetchModuleOverridePath()
    {
        FScopeLock ModuleLock(&GetModuleOverrideMutex());
        return GetModuleOverridePath();
    }

    FString ResolveModuleOverrideDirectory(const FString& OverridePath, TArray<FString>* OutFailureMessages = nullptr)
    {
        FString Candidate = OverridePath;
        Candidate.TrimStartAndEndInline();

        if (Candidate.IsEmpty())
        {
            return FString();
        }

        if (FPaths::IsRelative(Candidate))
        {
            Candidate = FPaths::ConvertRelativePathToFull(Candidate);
        }
        FPaths::MakePlatformFilename(Candidate);

        if (Candidate.IsEmpty())
        {
            return FString();
        }

        if (FPaths::DirectoryExists(Candidate))
        {
            return Candidate;
        }

        if (FPaths::FileExists(Candidate))
        {
            const FString Extension = FPaths::GetExtension(Candidate, true);
            if (Extension.Equals(TEXT(".dll"), ESearchCase::IgnoreCase) || Extension.Equals(TEXT(".modules"), ESearchCase::IgnoreCase))
            {
                FString Directory = FPaths::GetPath(Candidate);
                FPaths::MakePlatformFilename(Directory);
                if (FPaths::DirectoryExists(Directory))
                {
                    return Directory;
                }
                if (OutFailureMessages)
                {
                    OutFailureMessages->Add(FString::Printf(TEXT("AVEncoder override directory missing: %s."), *Directory));
                }
            }
            else if (OutFailureMessages)
            {
                OutFailureMessages->Add(FString::Printf(TEXT("AVEncoder override must point to a directory or AVEncoder binary: %s."), *Candidate));
            }
        }
        else if (OutFailureMessages)
        {
            OutFailureMessages->Add(FString::Printf(TEXT("AVEncoder override path not found: %s."), *Candidate));
        }

        return FString();
    }

    void AddModuleOverrideDirectoryIfPresent(const FString& OverridePath, TArray<FString>* OutFailureMessages = nullptr)
    {
        const FString Directory = ResolveModuleOverrideDirectory(OverridePath, OutFailureMessages);
        if (!Directory.IsEmpty())
        {
            FModuleManager::Get().AddBinariesDirectory(*Directory, false);
        }
    }

    bool TryCreateEncoderSession(AVEncoder::ECodec Codec, AVEncoder::EVideoFormat Format, FString& OutFailureReason)
    {
        constexpr int32 TestWidth = 256;
        constexpr int32 TestHeight = 144;

        AVEncoder::FVideoEncoderInput::FCreateParameters CreateParameters;
        CreateParameters.Width = TestWidth;
        CreateParameters.Height = TestHeight;
        CreateParameters.MaxBufferDimensions = FIntPoint(TestWidth, TestHeight);
        CreateParameters.Format = Format;
        CreateParameters.DebugName = TEXT("OmniNVENCProbe");
        CreateParameters.bAutoCopy = true;

        TSharedPtr<AVEncoder::FVideoEncoderInput> EncoderInput = AVEncoder::FVideoEncoderInput::CreateForRHI(CreateParameters);
        if (!EncoderInput.IsValid())
        {
            OutFailureReason = TEXT("Failed to create AVEncoder input for probe");
            return false;
        }

        AVEncoder::FVideoEncoder::FLayerConfig LayerConfig;
        LayerConfig.Width = TestWidth;
        LayerConfig.Height = TestHeight;
        LayerConfig.MaxFramerate = 60;
        LayerConfig.TargetBitrate = 5 * 1000 * 1000;
        LayerConfig.MaxBitrate = 10 * 1000 * 1000;

        AVEncoder::FVideoEncoder::FCodecConfig CodecConfig;
        CodecConfig.GOPLength = 30;
        CodecConfig.MaxNumBFrames = 0;
        CodecConfig.bEnableFrameReordering = false;

        AVEncoder::FVideoEncoder::FInit EncoderInit;
        EncoderInit.Codec = Codec;
        EncoderInit.CodecConfig = CodecConfig;
        EncoderInit.Layers.Add(LayerConfig);

        auto OnEncodedPacket = AVEncoder::FVideoEncoder::FOnEncodedPacket::CreateLambda([](const AVEncoder::FVideoEncoder::FEncodedPacket&)
        {
        });

        TSharedPtr<AVEncoder::FVideoEncoder> VideoEncoder = AVEncoder::FVideoEncoderFactory::Create(*EncoderInput, EncoderInit, MoveTemp(OnEncodedPacket));
        if (!VideoEncoder.IsValid())
        {
            OutFailureReason = TEXT("Failed to create AVEncoder NVENC instance");
            return false;
        }

        VideoEncoder.Reset();
        EncoderInput.Reset();

        return true;
    }

    FNVENCHardwareProbeResult RunNVENCHardwareProbe()
    {
        FNVENCHardwareProbeResult Result;

        FString OverridePath;
        {
            FScopeLock OverrideLock(&GetDllOverrideMutex());
            OverridePath = GetDllOverridePath();
        }

        OverridePath.TrimStartAndEndInline();
        FString NormalizedOverridePath = OverridePath;
        if (!NormalizedOverridePath.IsEmpty())
        {
            NormalizedOverridePath = FPaths::ConvertRelativePathToFull(NormalizedOverridePath);
            FPaths::MakePlatformFilename(NormalizedOverridePath);
        }

        FString OverrideCandidate = NormalizedOverridePath;
        if (!OverrideCandidate.IsEmpty())
        {
            const FString Extension = FPaths::GetExtension(OverrideCandidate, true);
            if (!Extension.Equals(TEXT(".dll"), ESearchCase::IgnoreCase))
            {
                OverrideCandidate = FPaths::Combine(OverrideCandidate, TEXT("nvEncodeAPI64.dll"));
            }
            FPaths::MakePlatformFilename(OverrideCandidate);
        }

        const FString ModuleOverride = FetchModuleOverridePath();
        TArray<FString> ModuleFailureMessages;
        AddModuleOverrideDirectoryIfPresent(ModuleOverride, &ModuleFailureMessages);

        void* NvencHandle = nullptr;
        FString LoadedFrom;
        TArray<FString> FailureMessages;

        if (!OverrideCandidate.IsEmpty())
        {
            if (!FPaths::FileExists(*OverrideCandidate))
            {
                FailureMessages.Add(FString::Printf(TEXT("Override path not found: %s."), *OverrideCandidate));
            }
            else
            {
                NvencHandle = FPlatformProcess::GetDllHandle(*OverrideCandidate);
                if (NvencHandle)
                {
                    LoadedFrom = OverrideCandidate;
                }
                else
                {
                    FailureMessages.Add(FString::Printf(TEXT("Failed to load override DLL: %s."), *OverrideCandidate));
                }
            }
        }

        if (!NvencHandle)
        {
            NvencHandle = FPlatformProcess::GetDllHandle(TEXT("nvEncodeAPI64.dll"));
            if (NvencHandle)
            {
                LoadedFrom = TEXT("system search paths");
            }
            else
            {
                FailureMessages.Add(TEXT("Failed to load nvEncodeAPI64.dll from system search paths."));
            }
        }

        if (NvencHandle)
        {
            Result.bDllPresent = true;
            if (!LoadedFrom.IsEmpty())
            {
                UE_LOG(LogTemp, Verbose, TEXT("NVENC probe loading nvEncodeAPI64.dll from %s"), *LoadedFrom);
            }
            FNvEncodeAPIGetMaxSupportedVersion NvEncodeAPIGetMaxSupportedVersion = reinterpret_cast<FNvEncodeAPIGetMaxSupportedVersion>(FPlatformProcess::GetDllExport(NvencHandle, TEXT("NvEncodeAPIGetMaxSupportedVersion")));
            if (NvEncodeAPIGetMaxSupportedVersion)
            {
                uint32 MaxVersion = 0;
                const uint32 NvStatus = NvEncodeAPIGetMaxSupportedVersion(&MaxVersion);
                if (NvStatus == 0 && MaxVersion != 0)
                {
                    Result.bApisReady = true;
                }
                else
                {
                    Result.ApiFailureReason = FString::Printf(TEXT("NvEncodeAPIGetMaxSupportedVersion failed (status=0x%08x, version=%u)"), NvStatus, MaxVersion);
                }
            }
            else
            {
                Result.ApiFailureReason = TEXT("NvEncodeAPIGetMaxSupportedVersion export missing in nvEncodeAPI64.dll");
            }
            FPlatformProcess::FreeDllHandle(NvencHandle);
        }
        else
        {
            Result.DllFailureReason = FailureMessages.Num() > 0
                ? FString::Join(FailureMessages, TEXT(" "))
                : TEXT("Failed to load nvEncodeAPI64.dll.");
        }

        if (!Result.bDllPresent)
        {
            Result.HardwareFailureReason = Result.DllFailureReason.IsEmpty() ? TEXT("NVENC runtime DLL missing") : Result.DllFailureReason;
            return Result;
        }

        if (!Result.bApisReady)
        {
            Result.HardwareFailureReason = Result.ApiFailureReason.IsEmpty() ? TEXT("Failed to query NVENC API version") : Result.ApiFailureReason;
            return Result;
        }

        static const FName AVEncoderModuleName(TEXT("AVEncoder"));
        if (!FModuleManager::Get().IsModuleLoaded(AVEncoderModuleName))
        {
            if (!FModuleManager::Get().LoadModule(AVEncoderModuleName))
            {
                ModuleFailureMessages.Add(TEXT("FModuleManager::LoadModule returned null for AVEncoder."));
            }
        }

        if (!FModuleManager::Get().IsModuleLoaded(AVEncoderModuleName))
        {
            if (ModuleFailureMessages.Num() == 0)
            {
                if (!ModuleOverride.IsEmpty())
                {
                    ModuleFailureMessages.Add(FString::Printf(TEXT("Failed to load AVEncoder module from %s."), *ModuleOverride));
                }
                else
                {
                    ModuleFailureMessages.Add(TEXT("AVEncoder module is not available in the current build."));
                }
            }
            Result.ModuleFailureReason = FString::Join(ModuleFailureMessages, TEXT(" "));
            Result.HardwareFailureReason = Result.ModuleFailureReason;
            return Result;
        }

        FString SessionFailure;
        if (TryCreateEncoderSession(AVEncoder::ECodec::H264, AVEncoder::EVideoFormat::NV12, SessionFailure))
        {
            Result.bSessionOpenable = true;
            Result.bSupportsH264 = true;
            Result.bSupportsNV12 = true;

            FString HevcFailure;
            if (TryCreateEncoderSession(AVEncoder::ECodec::HEVC, AVEncoder::EVideoFormat::NV12, HevcFailure))
            {
                Result.bSupportsHEVC = true;
            }
            else
            {
                Result.CodecFailureReason = HevcFailure;
            }

            FString P010Failure;
            if (TryCreateEncoderSession(AVEncoder::ECodec::HEVC, AVEncoder::EVideoFormat::P010, P010Failure))
            {
                Result.bSupportsP010 = true;
            }
            else
            {
                Result.FormatFailureReason = P010Failure;
            }

            FString BgraFailure;
            if (TryCreateEncoderSession(AVEncoder::ECodec::H264, AVEncoder::EVideoFormat::BGRA8, BgraFailure))
            {
                Result.bSupportsBGRA = true;
            }
            else if (Result.FormatFailureReason.IsEmpty())
            {
                Result.FormatFailureReason = BgraFailure;
            }
        }
        else
        {
            Result.SessionFailureReason = SessionFailure;
            Result.HardwareFailureReason = SessionFailure;
            return Result;
        }

        Result.HardwareFailureReason = TEXT("");

        UE_LOG(LogTemp, Log, TEXT("NVENC probe succeeded (NV12=%s, P010=%s, HEVC=%s, BGRA=%s)"),
            Result.bSupportsNV12 ? TEXT("Yes") : TEXT("No"),
            Result.bSupportsP010 ? TEXT("Yes") : TEXT("No"),
            Result.bSupportsHEVC ? TEXT("Yes") : TEXT("No"),
            Result.bSupportsBGRA ? TEXT("Yes") : TEXT("No"));
        return Result;
    }

    const FNVENCHardwareProbeResult& GetNVENCHardwareProbe()
    {
        FScopeLock CacheLock(&GetProbeCacheMutex());
        if (!GetProbeValidFlag())
        {
            GetCachedProbe() = RunNVENCHardwareProbe();
            GetProbeValidFlag() = true;
            const FNVENCHardwareProbeResult& CachedResult = GetCachedProbe();
            if (!CachedResult.bDllPresent || !CachedResult.bApisReady || !CachedResult.bSessionOpenable)
            {
                UE_LOG(LogTemp, Warning, TEXT("NVENC probe failed (Dll=%s, Api=%s, Session=%s). Reasons: %s | %s | %s"),
                    CachedResult.bDllPresent ? TEXT("Yes") : TEXT("No"),
                    CachedResult.bApisReady ? TEXT("Yes") : TEXT("No"),
                    CachedResult.bSessionOpenable ? TEXT("Yes") : TEXT("No"),
                    CachedResult.DllFailureReason.IsEmpty() ? TEXT("<none>") : *CachedResult.DllFailureReason,
                    CachedResult.ApiFailureReason.IsEmpty() ? TEXT("<none>") : *CachedResult.ApiFailureReason,
                    CachedResult.SessionFailureReason.IsEmpty() ? TEXT("<none>") : *CachedResult.SessionFailureReason);
            }
        }
        return GetCachedProbe();
    }
#endif
}
#if PLATFORM_WINDOWS
#undef OMNI_NVENCAPI
#endif

FOmniCaptureNVENCEncoder::FOmniCaptureNVENCEncoder()
{
}

bool FOmniCaptureNVENCEncoder::IsNVENCAvailable()
{
#if OMNI_WITH_AVENCODER && PLATFORM_WINDOWS
    const FNVENCHardwareProbeResult& Probe = GetNVENCHardwareProbe();
    return Probe.bDllPresent && Probe.bApisReady && Probe.bSessionOpenable;
#else
    return false;
#endif
}

FOmniNVENCCapabilities FOmniCaptureNVENCEncoder::QueryCapabilities()
{
    FOmniNVENCCapabilities Caps;

#if OMNI_WITH_AVENCODER && PLATFORM_WINDOWS
    const FNVENCHardwareProbeResult& Probe = GetNVENCHardwareProbe();

    Caps.bDllPresent = Probe.bDllPresent;
    Caps.bApisReady = Probe.bApisReady;
    Caps.bSessionOpenable = Probe.bSessionOpenable;
    Caps.bSupportsHEVC = Probe.bSupportsHEVC;
    Caps.bSupportsNV12 = Probe.bSupportsNV12 && SupportsColorFormat(EOmniCaptureColorFormat::NV12);
    Caps.bSupportsP010 = Probe.bSupportsP010 && SupportsColorFormat(EOmniCaptureColorFormat::P010);
    Caps.bSupportsBGRA = Probe.bSupportsBGRA && SupportsColorFormat(EOmniCaptureColorFormat::BGRA);
    Caps.bSupports10Bit = Caps.bSupportsP010;
    Caps.bHardwareAvailable = Caps.bDllPresent && Caps.bApisReady && Caps.bSessionOpenable;

    Caps.DllFailureReason = Probe.DllFailureReason;
    Caps.ApiFailureReason = Probe.ApiFailureReason;
    Caps.SessionFailureReason = Probe.SessionFailureReason;
    Caps.CodecFailureReason = Probe.CodecFailureReason;
    Caps.FormatFailureReason = Probe.FormatFailureReason;
    Caps.ModuleFailureReason = Probe.ModuleFailureReason;
    Caps.HardwareFailureReason = Probe.HardwareFailureReason;
#else
    Caps.bHardwareAvailable = false;
    Caps.DllFailureReason = TEXT("NVENC support is only available on Windows builds with AVEncoder.");
    Caps.HardwareFailureReason = Caps.DllFailureReason;
#endif

    Caps.AdapterName = FPlatformMisc::GetPrimaryGPUBrand();
#if PLATFORM_WINDOWS
    FString DeviceDescription;
    FString PrimaryBrand = FPlatformMisc::GetPrimaryGPUBrand();
#if OMNI_HAS_RHI_ADAPTER
    if (GDynamicRHI)
    {
        FRHIAdapterInfo AdapterInfo;
        GDynamicRHI->RHIGetAdapterInfo(AdapterInfo);
        DeviceDescription = AdapterInfo.Description;
    }
#else
    DeviceDescription = PrimaryBrand;
#endif
    if (DeviceDescription.IsEmpty())
    {
        DeviceDescription = PrimaryBrand;
    }
    const FGPUDriverInfo DriverInfo = FPlatformMisc::GetGPUDriverInfo(DeviceDescription);
#if UE_VERSION_NEWER_THAN(5, 5, 0)
    Caps.DriverVersion = DriverInfo.UserDriverVersion;
#else
    Caps.DriverVersion = DriverInfo.DriverVersion;
#endif
#endif

    return Caps;
}
#undef OMNI_HAS_RHI_ADAPTER

bool FOmniCaptureNVENCEncoder::SupportsColorFormat(EOmniCaptureColorFormat Format)
{
#if OMNI_WITH_AVENCODER
    switch (Format)
    {
    case EOmniCaptureColorFormat::NV12:
        return GPixelFormats[PF_NV12].Supported != 0;
    case EOmniCaptureColorFormat::P010:
#if defined(PF_P010)
        return GPixelFormats[PF_P010].Supported != 0;
#else
        return false;
#endif
    case EOmniCaptureColorFormat::BGRA:
        return GPixelFormats[PF_B8G8R8A8].Supported != 0;
    default:
        return false;
    }
#else
    return Format == EOmniCaptureColorFormat::BGRA;
#endif
}

bool FOmniCaptureNVENCEncoder::SupportsZeroCopyRHI()
{
#if PLATFORM_WINDOWS
    return GDynamicRHI &&
        (GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D11 ||
         GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12);
#else
    return false;
#endif
}

void FOmniCaptureNVENCEncoder::SetDllOverridePath(const FString& InOverridePath)
{
#if OMNI_WITH_AVENCODER
    FString NormalizedPath = InOverridePath;
    NormalizedPath.TrimStartAndEndInline();
    if (!NormalizedPath.IsEmpty())
    {
        NormalizedPath = FPaths::ConvertRelativePathToFull(NormalizedPath);
        FPaths::MakePlatformFilename(NormalizedPath);
    }

    bool bChanged = false;
    {
        FScopeLock OverrideLock(&GetDllOverrideMutex());
        if (!GetDllOverridePath().Equals(NormalizedPath, ESearchCase::CaseSensitive))
        {
            GetDllOverridePath() = NormalizedPath;
            bChanged = true;
        }
    }

    if (bChanged)
    {
        FScopeLock CacheLock(&GetProbeCacheMutex());
        GetProbeValidFlag() = false;
    }
#else
    (void)InOverridePath;
#endif
}

void FOmniCaptureNVENCEncoder::SetAVEncoderModuleOverridePath(const FString& InOverridePath)
{
#if OMNI_WITH_AVENCODER
    FString NormalizedPath = InOverridePath;
    NormalizedPath.TrimStartAndEndInline();
    if (!NormalizedPath.IsEmpty())
    {
        NormalizedPath = FPaths::ConvertRelativePathToFull(NormalizedPath);
        FPaths::MakePlatformFilename(NormalizedPath);
    }

    bool bChanged = false;
    {
        FScopeLock OverrideLock(&GetModuleOverrideMutex());
        if (!GetModuleOverridePath().Equals(NormalizedPath, ESearchCase::CaseSensitive))
        {
            GetModuleOverridePath() = NormalizedPath;
            bChanged = true;
        }
    }

    if (bChanged)
    {
        AddModuleOverrideDirectoryIfPresent(NormalizedPath);
        FScopeLock CacheLock(&GetProbeCacheMutex());
        GetProbeValidFlag() = false;
    }
#else
    (void)InOverridePath;
#endif
}

void FOmniCaptureNVENCEncoder::InvalidateCachedCapabilities()
{
#if OMNI_WITH_AVENCODER
    FScopeLock CacheLock(&GetProbeCacheMutex());
    GetProbeValidFlag() = false;
#endif
}

FOmniCaptureNVENCEncoder::~FOmniCaptureNVENCEncoder()
{
    Finalize();
}

void FOmniCaptureNVENCEncoder::Initialize(const FOmniCaptureSettings& Settings, const FString& OutputDirectory)
{
    LastErrorMessage.Reset();
    FString Directory = OutputDirectory.IsEmpty() ? (FPaths::ProjectSavedDir() / TEXT("OmniCaptures")) : OutputDirectory;
    Directory = FPaths::ConvertRelativePathToFull(Directory);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*Directory);

    RequestedCodec = Settings.Codec;
    const bool bUseHEVC = RequestedCodec == EOmniCaptureCodec::HEVC;
    OutputFilePath = Directory / (Settings.OutputFileName + (bUseHEVC ? TEXT(".h265") : TEXT(".h264")));
    ColorFormat = Settings.NVENCColorFormat;
    bZeroCopyRequested = Settings.bZeroCopy;

#if OMNI_WITH_AVENCODER
    const FIntPoint OutputSize = Settings.GetOutputResolution();
    const int32 OutputWidth = OutputSize.X;
    const int32 OutputHeight = OutputSize.Y;

    const FString ModuleOverride = FetchModuleOverridePath();
    AddModuleOverrideDirectoryIfPresent(ModuleOverride);

    if (!FModuleManager::Get().IsModuleLoaded(TEXT("AVEncoder")))
    {
        if (!FModuleManager::Get().LoadModule(TEXT("AVEncoder")))
        {
            LastErrorMessage = TEXT("Failed to load AVEncoder module for NVENC initialization.");
            UE_LOG(LogTemp, Error, TEXT("%s"), *LastErrorMessage);
            return;
        }
    }

    AVEncoder::FVideoEncoderInput::FCreateParameters CreateParameters;
    CreateParameters.Width = OutputWidth;
    CreateParameters.Height = OutputHeight;
    CreateParameters.Format = ToVideoFormat(ColorFormat);
    CreateParameters.MaxBufferDimensions = FIntPoint(OutputWidth, OutputHeight);
    CreateParameters.DebugName = TEXT("OmniCaptureNVENC");
    CreateParameters.bAutoCopy = !bZeroCopyRequested;

    EncoderInput = AVEncoder::FVideoEncoderInput::CreateForRHI(CreateParameters);
    if (!EncoderInput.IsValid())
    {
        const FString FormatName = StaticEnum<EOmniCaptureColorFormat>()->GetNameStringByValue(static_cast<int64>(ColorFormat));
        LastErrorMessage = FString::Printf(TEXT("Failed to create NVENC encoder input for %dx%d %s frames."),
            OutputWidth,
            OutputHeight,
            *FormatName);
        UE_LOG(LogTemp, Error, TEXT("%s"), *LastErrorMessage);
        return;
    }

    LayerConfig = AVEncoder::FVideoEncoder::FLayerConfig();
    LayerConfig.Width = OutputWidth;
    LayerConfig.Height = OutputHeight;
    LayerConfig.MaxFramerate = 120;
    LayerConfig.TargetBitrate = Settings.Quality.TargetBitrateKbps * 1000;
    LayerConfig.MaxBitrate = FMath::Max<int32>(LayerConfig.TargetBitrate, Settings.Quality.MaxBitrateKbps * 1000);
    LayerConfig.MinQp = 0;
    LayerConfig.MaxQp = 51;

    CodecConfig = AVEncoder::FVideoEncoder::FCodecConfig();
    CodecConfig.bLowLatency = Settings.Quality.bLowLatency;
    CodecConfig.GOPLength = Settings.Quality.GOPLength;
    CodecConfig.MaxNumBFrames = Settings.Quality.BFrames;
    CodecConfig.bEnableFrameReordering = Settings.Quality.BFrames > 0;

    AVEncoder::FVideoEncoder::FInit EncoderInit;
    EncoderInit.Codec = bUseHEVC ? AVEncoder::ECodec::HEVC : AVEncoder::ECodec::H264;
    EncoderInit.CodecConfig = CodecConfig;
    EncoderInit.Layers.Add(LayerConfig);

    auto OnEncodedPacket = AVEncoder::FVideoEncoder::FOnEncodedPacket::CreateLambda([this](const AVEncoder::FVideoEncoder::FEncodedPacket& Packet)
    {
        FScopeLock Lock(&EncoderCS);
        if (!BitstreamFile)
        {
            return;
        }

        AnnexBBuffer.Reset();
        Packet.ToAnnexB(AnnexBBuffer);
        if (AnnexBBuffer.Num() > 0)
        {
            BitstreamFile->Write(AnnexBBuffer.GetData(), AnnexBBuffer.Num());
        }
    });

    VideoEncoder = AVEncoder::FVideoEncoderFactory::Create(*EncoderInput, EncoderInit, MoveTemp(OnEncodedPacket));
    if (!VideoEncoder.IsValid())
    {
        const FString CodecName = StaticEnum<EOmniCaptureCodec>()->GetNameStringByValue(static_cast<int64>(RequestedCodec));
        LastErrorMessage = FString::Printf(TEXT("Failed to create NVENC video encoder for codec %s."), *CodecName);
        UE_LOG(LogTemp, Error, TEXT("%s"), *LastErrorMessage);
        EncoderInput.Reset();
        return;
    }

    BitstreamFile.Reset(PlatformFile.OpenWrite(*OutputFilePath, /*bAppend=*/false));
    if (!BitstreamFile)
    {
        LastErrorMessage = FString::Printf(TEXT("Unable to open NVENC bitstream output file at %s."), *OutputFilePath);
        UE_LOG(LogTemp, Warning, TEXT("%s"), *LastErrorMessage);
    }

    bInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("NVENC encoder ready (%dx%d, %s, ZeroCopy=%s)."), OutputWidth, OutputHeight, bUseHEVC ? TEXT("HEVC") : TEXT("H.264"), bZeroCopyRequested ? TEXT("Yes") : TEXT("No"));
#else
    LastErrorMessage = TEXT("NVENC is only available on Windows builds with AVEncoder support.");
    UE_LOG(LogTemp, Warning, TEXT("%s"), *LastErrorMessage);
#endif
}

void FOmniCaptureNVENCEncoder::EnqueueFrame(const FOmniCaptureFrame& Frame)
{
#if OMNI_WITH_AVENCODER
    if (!bInitialized || !VideoEncoder.IsValid() || !EncoderInput.IsValid())
    {
        return;
    }

    if (Frame.ReadyFence.IsValid())
    {
        RHIWaitGPUFence(Frame.ReadyFence);
    }

    if (Frame.bUsedCPUFallback)
    {
        UE_LOG(LogTemp, Warning, TEXT("Skipping NVENC submission because frame used CPU equirect fallback."));
        return;
    }

    if (!Frame.Texture.IsValid())
    {
        return;
    }

    TSharedPtr<AVEncoder::FVideoEncoderInputFrame> InputFrame;
    if (Frame.EncoderTextures.Num() > 0)
    {
        InputFrame = EncoderInput->CreateEncoderInputFrame();
        if (InputFrame.IsValid())
        {
            for (int32 PlaneIndex = 0; PlaneIndex < Frame.EncoderTextures.Num(); ++PlaneIndex)
            {
                if (Frame.EncoderTextures[PlaneIndex].IsValid())
                {
                    InputFrame->SetTexture(PlaneIndex, Frame.EncoderTextures[PlaneIndex]);
                }
            }
        }
    }

    if (!InputFrame.IsValid())
    {
        InputFrame = EncoderInput->CreateEncoderInputFrameFromRHITexture(Frame.Texture);
    }

    if (!InputFrame.IsValid())
    {
        return;
    }

    InputFrame->SetTimestampUs(static_cast<uint64>(Frame.Metadata.Timecode * 1'000'000.0));
    InputFrame->SetFrameIndex(Frame.Metadata.FrameIndex);
    InputFrame->SetKeyFrame(Frame.Metadata.bKeyFrame);

    VideoEncoder->Encode(InputFrame);
#else
    (void)Frame;
#endif
}

void FOmniCaptureNVENCEncoder::Finalize()
{
#if OMNI_WITH_AVENCODER
    if (!bInitialized)
    {
        LastErrorMessage.Reset();
        return;
    }

    VideoEncoder.Reset();
    EncoderInput.Reset();

    if (BitstreamFile)
    {
        BitstreamFile->Flush();
        BitstreamFile.Reset();
    }

    UE_LOG(LogTemp, Log, TEXT("NVENC finalize complete -> %s"), *OutputFilePath);
#endif
    bInitialized = false;
    LastErrorMessage.Reset();
}
