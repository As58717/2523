#include "OmniCaptureImageWriter.h"

#if __has_include("ImageWriteQueue/Public/ImageWriteQueue.h")
#include "ImageWriteQueue/Public/ImageWriteQueue.h"
#include "ImageWriteQueue/Public/ImageWriteTask.h"
#include "ImageWriteQueue/Public/ImageWriteTypes.h"
#else
#include "ImageWriteQueue.h"
#include "ImageWriteTask.h"
#include "ImageWriteTypes.h"
#endif

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

FOmniCaptureImageWriter::FOmniCaptureImageWriter() {}
FOmniCaptureImageWriter::~FOmniCaptureImageWriter() { Flush(); }

void FOmniCaptureImageWriter::Initialize(const FOmniCaptureSettings& Settings, const FString& InOutputDirectory)
{
    OutputDirectory = InOutputDirectory.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("OmniCaptures") : InOutputDirectory;
    SequenceBaseName = Settings.OutputFileName;
    OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    TargetFormat = Settings.ImageFormat;

    ImageWriteQueue = MakeUnique<FImageWriteQueue>();
}

void FOmniCaptureImageWriter::EnqueueFrame(TUniquePtr<FOmniCaptureFrame>&& Frame, const FString& FrameFileName)
{
    if (!ImageWriteQueue.IsValid() || !Frame.IsValid())
    {
        return;
    }

    TUniquePtr<FImageWriteTask> Task = MakeUnique<FImageWriteTask>();
    Task->Filename = OutputDirectory / FrameFileName;
    Task->bOverwriteFile = true;
    Task->PixelData = MoveTemp(Frame->PixelData);

    switch (TargetFormat)
    {
    case EOmniCaptureImageFormat::JPG:
        Task->Format = EImageFormat::JPEG;
        Task->CompressionQuality = static_cast<int32>(EImageCompressionQuality::Default);
#if UE_VERSION_OLDER_THAN(5, 6, 0)
        Task->bWriteGammaCorrectedToSRGB = true;
#endif
        break;
    case EOmniCaptureImageFormat::EXR:
        Task->Format = EImageFormat::EXR;
        Task->CompressionQuality = static_cast<int32>(EImageCompressionQuality::Uncompressed);
#if UE_VERSION_OLDER_THAN(5, 6, 0)
        Task->bWriteGammaCorrectedToSRGB = false;
#endif
        break;
    case EOmniCaptureImageFormat::PNG:
    default:
        Task->Format = EImageFormat::PNG;
        Task->CompressionQuality = static_cast<int32>(EImageCompressionQuality::Uncompressed);
#if UE_VERSION_OLDER_THAN(5, 6, 0)
        Task->bWriteGammaCorrectedToSRGB = true;
#endif
        break;
    }

    ImageWriteQueue->Enqueue(MoveTemp(Task));

    FScopeLock Lock(&MetadataCS);
    CapturedMetadata.Add(Frame->Metadata);
}

void FOmniCaptureImageWriter::Flush()
{
    if (ImageWriteQueue.IsValid())
    {
        ImageWriteQueue->Flush();
        ImageWriteQueue.Reset();
    }
}

TArray<FOmniCaptureFrameMetadata> FOmniCaptureImageWriter::ConsumeCapturedFrames()
{
    FScopeLock Lock(&MetadataCS);
    TArray<FOmniCaptureFrameMetadata> Result = MoveTemp(CapturedMetadata);
    CapturedMetadata.Reset();
    return Result;
}
