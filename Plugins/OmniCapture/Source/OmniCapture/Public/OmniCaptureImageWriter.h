#pragma once

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"
#include "Misc/EngineVersionComparison.h"

#if UE_VERSION_NEWER_THAN(5, 5, 0)
namespace UE::ImageWriteQueue
{
    class FImageWriteQueue;
    class FImageWriteTask;
}
using FImageWriteQueue = UE::ImageWriteQueue::FImageWriteQueue;
using FImageWriteTask = UE::ImageWriteQueue::FImageWriteTask;
#else
class FImageWriteQueue;
class FImageWriteTask;
#endif

class OMNICAPTURE_API FOmniCaptureImageWriter
{
public:
    FOmniCaptureImageWriter();
    ~FOmniCaptureImageWriter();

    void Initialize(const FOmniCaptureSettings& Settings, const FString& InOutputDirectory);
    void EnqueueFrame(TUniquePtr<FOmniCaptureFrame>&& Frame, const FString& FrameFileName);
    void Flush();
    const TArray<FOmniCaptureFrameMetadata>& GetCapturedFrames() const { return CapturedMetadata; }
    TArray<FOmniCaptureFrameMetadata> ConsumeCapturedFrames();

private:
    TUniquePtr<FImageWriteQueue> ImageWriteQueue;
    FString OutputDirectory;
    FString SequenceBaseName;
    EOmniCaptureImageFormat TargetFormat = EOmniCaptureImageFormat::PNG;

    TArray<FOmniCaptureFrameMetadata> CapturedMetadata;
    FCriticalSection MetadataCS;
};

