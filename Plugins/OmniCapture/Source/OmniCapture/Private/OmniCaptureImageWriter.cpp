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

#if __has_include("ImageWriteQueue/Public/ImageWriteQueueModule.h")
#include "ImageWriteQueue/Public/ImageWriteQueueModule.h"
#define OMNICAPTURE_USE_IMAGEWRITE_MODULE 1
#else
#define OMNICAPTURE_USE_IMAGEWRITE_MODULE 0
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

#if OMNICAPTURE_USE_IMAGEWRITE_MODULE && UE_VERSION_NEWER_THAN(5, 5, 0)
    ImageWriteQueue = &UE::ImageWriteQueue::IImageWriteQueueModule::Get().GetWriteQueue();
    OwnedQueue.Reset();
#else
    OwnedQueue = MakeUnique<FImageWriteQueueType>();
    ImageWriteQueue = OwnedQueue.Get();
#endif
}

void FOmniCaptureImageWriter::EnqueueFrame(TUniquePtr<FOmniCaptureFrame>&& Frame, const FString& FrameFileName)
{
    if (ImageWriteQueue == nullptr || !Frame.IsValid())
    {
        return;
    }

    TUniquePtr<FImageWriteTaskType> Task = MakeUnique<FImageWriteTaskType>();
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
    if (ImageWriteQueue != nullptr)
    {
        ImageWriteQueue->Flush();
        ImageWriteQueue = nullptr;
#if !(OMNICAPTURE_USE_IMAGEWRITE_MODULE && UE_VERSION_NEWER_THAN(5, 5, 0))
        OwnedQueue.Reset();
#endif
    }
}

TArray<FOmniCaptureFrameMetadata> FOmniCaptureImageWriter::ConsumeCapturedFrames()
{
    FScopeLock Lock(&MetadataCS);
    TArray<FOmniCaptureFrameMetadata> Result = MoveTemp(CapturedMetadata);
    CapturedMetadata.Reset();
    return Result;
}

#undef OMNICAPTURE_USE_IMAGEWRITE_MODULE
