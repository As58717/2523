#pragma once

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"

class FImageWriteQueue;
class FImageWriteTask;

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

