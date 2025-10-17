#pragma once

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"
#include "Async/Future.h"

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
    bool WritePixelDataToDisk(TUniquePtr<FImagePixelData> PixelData, const FString& FilePath, EOmniCaptureImageFormat Format, bool bIsLinear) const;
    bool WritePNG(const TImagePixelData<FColor>& PixelData, const FString& FilePath) const;
    bool WritePNGFromLinear(const TImagePixelData<FFloat16Color>& PixelData, const FString& FilePath) const;
    bool WriteJPEG(const TImagePixelData<FColor>& PixelData, const FString& FilePath) const;
    bool WriteJPEGFromLinear(const TImagePixelData<FFloat16Color>& PixelData, const FString& FilePath) const;
    bool WriteEXR(const TImagePixelData<FFloat16Color>& PixelData, const FString& FilePath) const;
    bool WriteEXRFromColor(const TImagePixelData<FColor>& PixelData, const FString& FilePath) const;
    void TrackPendingTask(TFuture<bool>&& TaskFuture);
    void PruneCompletedTasks();
    void WaitForAllTasks();

    bool bInitialized = false;
    FString OutputDirectory;
    FString SequenceBaseName;
    EOmniCaptureImageFormat TargetFormat = EOmniCaptureImageFormat::PNG;

    TArray<FOmniCaptureFrameMetadata> CapturedMetadata;
    FCriticalSection MetadataCS;

    TArray<TFuture<bool>> PendingTasks;
    FCriticalSection PendingTasksCS;
};

