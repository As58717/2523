#pragma once
#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"
#include "ImageWriteTypes.h"
#include "Misc/EngineVersionComparison.h"
#include "ImagePixelData.h"
#include "OmniCaptureTypes.generated.h"

UENUM(BlueprintType)
enum class EOmniCaptureMode : uint8 { Mono, Stereo };

UENUM(BlueprintType)
enum class EOmniCaptureProjection : uint8 { Equirectangular, Planar2D };

UENUM(BlueprintType)
enum class EOmniCaptureCoverage : uint8 { FullSphere, HalfSphere };

UENUM(BlueprintType)
enum class EOmniCaptureStereoLayout : uint8 { TopBottom, SideBySide };

#if UE_VERSION_OLDER_THAN(5, 6, 0)
UENUM(BlueprintType)
enum class EOmniOutputFormat : uint8
{
        PNGSequence UMETA(DisplayName = "Image Sequence"),
        NVENCHardware UMETA(DisplayName = "NVENC Hardware"),
        ImageSequence UMETA(Hidden) = PNGSequence
};
#else
UENUM(BlueprintType)
enum class EOmniOutputFormat : uint8
{
        ImageSequence UMETA(DisplayName = "Image Sequence"),
        NVENCHardware UMETA(DisplayName = "NVENC Hardware"),
        PNGSequence UMETA(Hidden) = ImageSequence
};
#endif

UENUM(BlueprintType)
enum class EOmniCaptureImageFormat : uint8 { PNG, JPG, EXR };

UENUM(BlueprintType)
enum class EOmniCaptureGamma : uint8 { SRGB, Linear };

UENUM(BlueprintType)
enum class EOmniCaptureColorSpace : uint8 { BT709, BT2020, HDR10 };

UENUM(BlueprintType)
enum class EOmniCaptureCodec : uint8 { H264, HEVC };

UENUM(BlueprintType)
enum class EOmniCaptureColorFormat : uint8 { NV12, P010, BGRA };

UENUM(BlueprintType)
enum class EOmniCaptureRateControlMode : uint8 { ConstantBitrate, VariableBitrate, Lossless };

UENUM(BlueprintType)
enum class EOmniCaptureState : uint8 { Idle, Recording, Paused, DroppedFrames, Finalizing };

UENUM(BlueprintType)
enum class EOmniCaptureRingBufferPolicy : uint8 { DropOldest, BlockProducer };

UENUM(BlueprintType)
enum class EOmniCapturePreviewView : uint8 { StereoComposite, LeftEye, RightEye };

USTRUCT(BlueprintType)
struct FOmniCaptureQuality
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video") int32 TargetBitrateKbps = 60000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video") int32 MaxBitrateKbps = 80000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video") int32 GOPLength = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video") int32 BFrames = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video") bool bLowLatency = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video") EOmniCaptureRateControlMode RateControlMode = EOmniCaptureRateControlMode::ConstantBitrate;
};

USTRUCT(BlueprintType)
struct FOmniCaptureSettings
{
        GENERATED_BODY()
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture") EOmniCaptureMode Mode = EOmniCaptureMode::Mono;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture") EOmniCaptureProjection Projection = EOmniCaptureProjection::Equirectangular;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture") EOmniCaptureCoverage Coverage = EOmniCaptureCoverage::FullSphere;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture") EOmniCaptureStereoLayout StereoLayout = EOmniCaptureStereoLayout::TopBottom;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = 1024, UIMin = 1024)) int32 Resolution = 4096;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = 16, UIMin = 64)) FIntPoint PlanarResolution = FIntPoint(3840, 2160);
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = 1, UIMin = 1)) int32 PlanarIntegerScale = 1;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = 0.0, UIMin = 0.0)) float TargetFrameRate = 60.0f;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture") EOmniCaptureGamma Gamma = EOmniCaptureGamma::SRGB;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture") bool bEnablePreviewWindow = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = 0.1, UIMin = 0.1)) float PreviewScreenScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = 1.0, UIMin = 5.0, ClampMax = 240.0)) float PreviewFrameRate = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture") bool bRecordAudio = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture") float AudioGain = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture") TSoftObjectPtr<class USoundSubmix> SubmixToRecord;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture") float InterPupillaryDistanceCm = 6.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = 0.0, UIMin = 0.0)) float SegmentDurationSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = 0)) int32 SegmentSizeLimitMB = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture") bool bCreateSegmentSubfolders = true;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output") EOmniOutputFormat OutputFormat =
#if UE_VERSION_OLDER_THAN(5, 6, 0)
            EOmniOutputFormat::PNGSequence;
#else
            EOmniOutputFormat::ImageSequence;
#endif
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output") EOmniCaptureImageFormat ImageFormat = EOmniCaptureImageFormat::PNG;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output") FString OutputDirectory;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output") FString OutputFileName = TEXT("OmniCapture");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output") EOmniCaptureColorSpace ColorSpace = EOmniCaptureColorSpace::BT709;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output") bool bEnableFastStart = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output") bool bForceConstantFrameRate = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output") bool bAllowNVENCFallback = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diagnostics", meta = (ClampMin = 0)) int32 MinimumFreeDiskSpaceGB = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diagnostics", meta = (ClampMin = 0.1, ClampMax = 1.0)) float LowFrameRateWarningRatio = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output") FString PreferredFFmpegPath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = 0.0, ClampMax = 1.0)) float SeamBlend = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = 0.0, ClampMax = 1.0)) float PolarDampening = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output") FOmniCaptureQuality Quality;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVENC") EOmniCaptureCodec Codec = EOmniCaptureCodec::HEVC;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVENC") EOmniCaptureColorFormat NVENCColorFormat = EOmniCaptureColorFormat::NV12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVENC") bool bZeroCopy = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVENC", meta = (ClampMin = 0, UIMin = 0)) int32 RingBufferCapacity = 6;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NVENC") EOmniCaptureRingBufferPolicy RingBufferPolicy = EOmniCaptureRingBufferPolicy::DropOldest;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output") bool bOpenPreviewOnFinalize = false;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview") EOmniCapturePreviewView PreviewVisualization = EOmniCapturePreviewView::StereoComposite;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metadata") bool bGenerateManifest = true;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metadata") bool bWriteSpatialMetadata = true;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metadata") bool bWriteXMPMetadata = true;
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metadata") bool bInjectFFmpegMetadata = true;

        FIntPoint GetEquirectResolution() const;
        FIntPoint GetPlanarResolution() const;
        FIntPoint GetOutputResolution() const;
        FIntPoint GetPerEyeOutputResolution() const;
        bool IsStereo() const;
        bool IsVR180() const;
        bool IsPlanar() const;
        FString GetStereoModeMetadataTag() const;
        int32 GetEncoderAlignmentRequirement() const;
        float GetHorizontalFOVDegrees() const;
        float GetVerticalFOVDegrees() const;
        float GetLongitudeSpanRadians() const;
        float GetLatitudeSpanRadians() const;
        FString GetImageFileExtension() const;
};

USTRUCT()
struct FOmniAudioPacket
{
	GENERATED_BODY()
	UPROPERTY() double Timestamp = 0.0;
	UPROPERTY() int32 SampleRate = 48000;
	UPROPERTY() int32 NumChannels = 2;
	UPROPERTY() TArray<int16> PCM16;
};

USTRUCT()
struct FOmniCaptureFrameMetadata
{
	GENERATED_BODY()
	UPROPERTY() int32 FrameIndex = 0;
	UPROPERTY() double Timecode = 0.0;
	UPROPERTY() bool bKeyFrame = false;
};

struct FOmniCaptureFrame
{
	FOmniCaptureFrameMetadata Metadata;
	TUniquePtr<FImagePixelData> PixelData;
	TRefCountPtr<IPooledRenderTarget> GPUSource;
	FTextureRHIRef Texture;
	FGPUFenceRHIRef ReadyFence;
	bool bLinearColor = false;
	bool bUsedCPUFallback = false;
	TArray<FOmniAudioPacket> AudioPackets;
	TArray<FTextureRHIRef> EncoderTextures;
};

USTRUCT(BlueprintType)
struct FOmniCaptureRingBufferStats
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats") int32 PendingFrames = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats") int32 DroppedFrames = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats") int32 BlockedPushes = 0;
};

USTRUCT(BlueprintType)
struct FOmniAudioSyncStats
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") double LatestVideoTimestamp = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") double LatestAudioTimestamp = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") double DriftMilliseconds = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") double MaxObservedDriftMilliseconds = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") int32 PendingPackets = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") bool bInError = false;
};
