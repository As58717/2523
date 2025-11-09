
#pragma once

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"

#undef OMNI_WITH_AVENCODER

#if PLATFORM_WINDOWS && WITH_OMNI_NVENC
    #define OMNI_WITH_AVENCODER 1

    // UE 5.5 and older ship the AVEncoder headers directly under AVEncoder/.
    #if __has_include("AVEncoder/VideoEncoderFactory.h")
        #include "AVEncoder/VideoEncoder.h"
        #include "AVEncoder/VideoEncoderInput.h"
        #include "AVEncoder/VideoEncoderFactory.h"
        #include "AVEncoder/VideoEncoderCommon.h"
        namespace OmniAVEncoder = AVEncoder;

    // Some engine distributions move the headers beneath AVEncoder/Public/.
    #elif __has_include("AVEncoder/Public/VideoEncoderFactory.h")
        #include "AVEncoder/Public/VideoEncoder.h"
        #include "AVEncoder/Public/VideoEncoderInput.h"
        #include "AVEncoder/Public/VideoEncoderFactory.h"
        #include "AVEncoder/Public/VideoEncoderCommon.h"
        namespace OmniAVEncoder = AVEncoder;

    // UE 5.6 reorganised the AVEncoder public API into nested Video directories and the UE::AVEncoder namespace.
    #elif __has_include("AVEncoder/Public/Video/EncoderFactory.h") || __has_include("AVEncoder/Public/Video/VideoEncoderFactory.h")
        #if __has_include("AVEncoder/Public/Video/Encoder.h")
            #include "AVEncoder/Public/Video/Encoder.h"
        #else
            #include "AVEncoder/Public/Video/VideoEncoder.h"
        #endif
        #if __has_include("AVEncoder/Public/Video/EncoderInput.h")
            #include "AVEncoder/Public/Video/EncoderInput.h"
        #else
            #include "AVEncoder/Public/Video/VideoEncoderInput.h"
        #endif
        #if __has_include("AVEncoder/Public/Video/EncoderFactory.h")
            #include "AVEncoder/Public/Video/EncoderFactory.h"
        #else
            #include "AVEncoder/Public/Video/VideoEncoderFactory.h"
        #endif
        #if __has_include("AVEncoder/Public/Video/EncoderCommon.h")
            #include "AVEncoder/Public/Video/EncoderCommon.h"
        #else
            #include "AVEncoder/Public/Video/VideoEncoderCommon.h"
        #endif
        namespace OmniAVEncoder = UE::AVEncoder;

    // If none of the known layouts exist, disable NVENC support and fall back to PNG captures.
    #else
        #undef OMNI_WITH_AVENCODER
        #define OMNI_WITH_AVENCODER 0
    #endif
#else
    #define OMNI_WITH_AVENCODER 0
#endif

#if OMNI_WITH_AVENCODER
    // The NVENC capability helpers moved a few times between engine releases. Try to locate
    // whichever layout is available so we can alias a consistent namespace for the call sites.
    #if __has_include("AVEncoder/Public/Video/Encoders/NVENC/NVENCCommon.h")
        #include "AVEncoder/Public/Video/Encoders/NVENC/NVENCCommon.h"
        namespace OmniNVENC = OmniAVEncoder::NVENC;
        #define OMNI_WITH_NVENC_CAPABILITIES 1
        #define OMNI_DEFINED_NVENC_NAMESPACE 1
    #elif __has_include("AVEncoder/Public/Video/Encoders/Nvenc/NvencCommon.h")
        #include "AVEncoder/Public/Video/Encoders/Nvenc/NvencCommon.h"
        namespace OmniNVENC = OmniAVEncoder::NVENC;
        #define OMNI_WITH_NVENC_CAPABILITIES 1
        #define OMNI_DEFINED_NVENC_NAMESPACE 1
    #elif __has_include("AVEncoder/NVENCCommon.h")
        #include "AVEncoder/NVENCCommon.h"
        namespace OmniNVENC = OmniAVEncoder;
        #define OMNI_WITH_NVENC_CAPABILITIES 1
        #define OMNI_DEFINED_NVENC_NAMESPACE 1
    #elif __has_include("AVEncoder/NvencCommon.h")
        #include "AVEncoder/NvencCommon.h"
        namespace OmniNVENC = OmniAVEncoder;
        #define OMNI_WITH_NVENC_CAPABILITIES 1
        #define OMNI_DEFINED_NVENC_NAMESPACE 1
    #elif __has_include("AVEncoder/Public/NVENCCommon.h")
        #include "AVEncoder/Public/NVENCCommon.h"
        namespace OmniNVENC = OmniAVEncoder;
        #define OMNI_WITH_NVENC_CAPABILITIES 1
        #define OMNI_DEFINED_NVENC_NAMESPACE 1
    #elif __has_include("AVEncoder/Public/NvencCommon.h")
        #include "AVEncoder/Public/NvencCommon.h"
        namespace OmniNVENC = OmniAVEncoder;
        #define OMNI_WITH_NVENC_CAPABILITIES 1
        #define OMNI_DEFINED_NVENC_NAMESPACE 1
    #else
        #define OMNI_WITH_NVENC_CAPABILITIES 0
    #endif
#endif

#if !defined(OMNI_WITH_NVENC_CAPABILITIES)
    #define OMNI_WITH_NVENC_CAPABILITIES 0
#endif

#if OMNI_WITH_AVENCODER && !defined(OMNI_DEFINED_NVENC_NAMESPACE)
    namespace OmniNVENC = OmniAVEncoder;
#endif

#if defined(OMNI_DEFINED_NVENC_NAMESPACE)
    #undef OMNI_DEFINED_NVENC_NAMESPACE
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
    FString NV12FailureReason;
    FString P010FailureReason;
    FString BGRAFailureReason;
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
    static void SetModuleOverridePath(const FString& InOverridePath);
    static void SetDllOverridePath(const FString& InOverridePath);
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
    TSharedPtr<OmniAVEncoder::FVideoEncoder> VideoEncoder;
    TSharedPtr<OmniAVEncoder::FVideoEncoderInput> EncoderInput;
    OmniAVEncoder::FVideoEncoder::FLayerConfig LayerConfig;
    OmniAVEncoder::FVideoEncoder::FCodecConfig CodecConfig;
    FCriticalSection EncoderCS;
    TArray<uint8> AnnexBBuffer;
    TUniquePtr<IFileHandle> BitstreamFile;
#endif
};
