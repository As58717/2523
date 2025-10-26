#pragma once

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"
#include "Async/Future.h"
#include "Templates/Function.h"

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
    bool WritePNGRaw(const FString& FilePath, const FIntPoint& Size, const void* RawData, int64 RawSizeInBytes, ERGBFormat Format, int32 BitDepth) const;
    bool WritePNGWithRowSource(const FString& FilePath, const FIntPoint& Size, ERGBFormat Format, int32 BitDepth, TFunctionRef<void(int32 RowStart, int32 RowCount, int64 BytesPerRow, TArray64<uint8>& TempBuffer, TArray<uint8*>& RowPointers)> PrepareRows) const;
    bool WritePNG(const TImagePixelData<FColor>& PixelData, const FString& FilePath) const;
    bool WritePNGFromLinear(const TImagePixelData<FFloat16Color>& PixelData, const FString& FilePath) const;
    bool WriteBMP(const TImagePixelData<FColor>& PixelData, const FString& FilePath) const;
    bool WriteBMPFromLinear(const TImagePixelData<FFloat16Color>& PixelData, const FString& FilePath) const;
    bool WriteJPEG(const TImagePixelData<FColor>& PixelData, const FString& FilePath) const;
    bool WriteJPEGFromLinear(const TImagePixelData<FFloat16Color>& PixelData, const FString& FilePath) const;
    bool WriteEXR(const TImagePixelData<FFloat16Color>& PixelData, const FString& FilePath) const;
    bool WriteEXRFromColor(const TImagePixelData<FColor>& PixelData, const FString& FilePath) const;
    void TrackPendingTask(TFuture<bool>&& TaskFuture);
    void PruneCompletedTasks();
    void EnforcePendingTaskLimit();
    void WaitForAllTasks();

    bool bInitialized = false;
    FString OutputDirectory;
    FString SequenceBaseName;
    EOmniCaptureImageFormat TargetFormat = EOmniCaptureImageFormat::PNG;
    EOmniCapturePNGBitDepth TargetPNGBitDepth = EOmniCapturePNGBitDepth::BitDepth32;
    int32 MaxPendingTasks = 8;

    TArray<FOmniCaptureFrameMetadata> CapturedMetadata;
    FCriticalSection MetadataCS;

    TArray<TFuture<bool>> PendingTasks;
    FCriticalSection PendingTasksCS;
};

