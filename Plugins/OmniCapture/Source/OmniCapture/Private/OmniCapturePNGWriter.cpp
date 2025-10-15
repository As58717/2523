#include "OmniCapturePNGWriter.h"

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

FOmniCapturePNGWriter::FOmniCapturePNGWriter() {}
FOmniCapturePNGWriter::~FOmniCapturePNGWriter() { Flush(); }

void FOmniCapturePNGWriter::Initialize(const FOmniCaptureSettings& Settings, const FString& InOutputDirectory)
{
    OutputDirectory = InOutputDirectory.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("OmniCaptures") : InOutputDirectory;
    SequenceBaseName = Settings.OutputFileName;
    OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);

    ImageWriteQueue = MakeUnique<FImageWriteQueue>();
}

void FOmniCapturePNGWriter::EnqueueFrame(TUniquePtr<FOmniCaptureFrame>&& Frame, const FString& FrameFileName)
{
    if (!ImageWriteQueue.IsValid() || !Frame.IsValid())
    {
        return;
    }

    TUniquePtr<FImageWriteTask> Task = MakeUnique<FImageWriteTask>();
    Task->Format = EImageFormat::PNG;
    Task->Filename = OutputDirectory / FrameFileName;
    Task->CompressionQuality = static_cast<int32>(EImageCompressionQuality::Uncompressed);
    Task->bOverwriteFile = true;
    Task->PixelData = MoveTemp(Frame->PixelData);

    ImageWriteQueue->Enqueue(MoveTemp(Task));

    FScopeLock Lock(&MetadataCS);
    CapturedMetadata.Add(Frame->Metadata);
}

void FOmniCapturePNGWriter::Flush()
{
    if (ImageWriteQueue.IsValid())
    {
        ImageWriteQueue->Flush();
        ImageWriteQueue.Reset();
    }
}

TArray<FOmniCaptureFrameMetadata> FOmniCapturePNGWriter::ConsumeCapturedFrames()
{
    FScopeLock Lock(&MetadataCS);
    TArray<FOmniCaptureFrameMetadata> Result = MoveTemp(CapturedMetadata);
    CapturedMetadata.Reset();
    return Result;
}
