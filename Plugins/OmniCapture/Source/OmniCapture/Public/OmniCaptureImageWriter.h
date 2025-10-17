#pragma once

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"
#include "Misc/EngineVersionComparison.h"

namespace UE::ImageWriteQueue
{
#if UE_VERSION_NEWER_THAN(5, 5, 0)
    class FImageWriteQueue;
    class FImageWriteTask;
#endif
}

#if UE_VERSION_NEWER_THAN(5, 5, 0)
using FImageWriteQueueType = UE::ImageWriteQueue::FImageWriteQueue;
using FImageWriteTaskType = UE::ImageWriteQueue::FImageWriteTask;
#else
class FImageWriteQueue;
class FImageWriteTask;
using FImageWriteQueueType = FImageWriteQueue;
using FImageWriteTaskType = FImageWriteTask;
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
    TUniquePtr<FImageWriteQueueType> OwnedQueue;
    FImageWriteQueueType* ImageWriteQueue = nullptr;
    FString OutputDirectory;
    FString SequenceBaseName;
    EOmniCaptureImageFormat TargetFormat = EOmniCaptureImageFormat::PNG;

    TArray<FOmniCaptureFrameMetadata> CapturedMetadata;
    FCriticalSection MetadataCS;
};

