
#pragma once

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"

#undef OMNI_WITH_AVENCODER

#if PLATFORM_WINDOWS && WITH_OMNI_NVENC
    #if __has_include("AVEncoder/VideoEncoder.h") && __has_include("AVEncoder/VideoEncoderInput.h") && __has_include("AVEncoder/VideoEncoderFactory.h") && __has_include("AVEncoder/VideoEncoderCommon.h")
        #define OMNI_WITH_AVENCODER 1
        #include "AVEncoder/VideoEncoder.h"
        #include "AVEncoder/VideoEncoderInput.h"
        using namespace AVEncoder;
    #else
        #define OMNI_WITH_AVENCODER 0
    #endif
#else
    #define OMNI_WITH_AVENCODER 0
#endif


struct FOmniNVENCCapabilities
{
    bool bHardwareAvailable = false;
    bool bDllPresent = false;
    bool bApisReady = false;
    bool bSessionOpenable = false;
    bool bSupportsNV12 = false;
    bool bSupportsP010 = false;
    bool bSupportsHEVC = false;
    bool bSupports10Bit = false;
    bool bSupportsBGRA = false;
    FString DllFailureReason;
    FString ApiFailureReason;
    FString SessionFailureReason;
    FString CodecFailureReason;
    FString FormatFailureReason;
    FString ModuleFailureReason;
    FString HardwareFailureReason;
    FString AdapterName;
    FString DriverVersion;
};

class OMNICAPTURE_API FOmniCaptureNVENCEncoder
{
public:
    FOmniCaptureNVENCEncoder();
    ~FOmniCaptureNVENCEncoder();

    void Initialize(const FOmniCaptureSettings& Settings, const FString& OutputDirectory);
    void EnqueueFrame(const FOmniCaptureFrame& Frame);
    void Finalize();

    static bool IsNVENCAvailable();
    static FOmniNVENCCapabilities QueryCapabilities();
    static bool SupportsColorFormat(EOmniCaptureColorFormat Format);
    static bool SupportsZeroCopyRHI();
    static void SetDllOverridePath(const FString& InOverridePath);
    static void SetAVEncoderModuleOverridePath(const FString& InOverridePath);
    static void InvalidateCachedCapabilities();

    bool IsInitialized() const { return bInitialized; }
    FString GetOutputFilePath() const { return OutputFilePath; }
    const FString& GetLastError() const { return LastErrorMessage; }

private:
    FString OutputFilePath;
    bool bInitialized = false;
    EOmniCaptureColorFormat ColorFormat = EOmniCaptureColorFormat::NV12;
    bool bZeroCopyRequested = true;
    EOmniCaptureCodec RequestedCodec = EOmniCaptureCodec::HEVC;
    FString LastErrorMessage;

#if OMNI_WITH_AVENCODER
    TSharedPtr<AVEncoder::FVideoEncoder> VideoEncoder;
    TSharedPtr<AVEncoder::FVideoEncoderInput> EncoderInput;
    AVEncoder::FVideoEncoder::FLayerConfig LayerConfig;
    AVEncoder::FVideoEncoder::FCodecConfig CodecConfig;
    FCriticalSection EncoderCS;
    TArray<uint8> AnnexBBuffer;
    TUniquePtr<IFileHandle> BitstreamFile;
#endif
};
