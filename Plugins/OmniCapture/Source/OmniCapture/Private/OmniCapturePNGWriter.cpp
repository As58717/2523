#include "OmniCapturePNGWriter.h"
#include "ImageWriteQueue.h"
#include "ImageWriteTypes.h"
#include "ImageWriteTask.h"

#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// 检测模块头是否存在
#if __has_include("IImageWriteQueueModule.h")
#include "IImageWriteQueueModule.h"
#define OMNI_HAS_IIMAGEWRITEQUEUEMODULE 1
#elif __has_include("ImageWriteQueue/Public/IImageWriteQueueModule.h")
#include "ImageWriteQueue/Public/IImageWriteQueueModule.h"
#define OMNI_HAS_IIMAGEWRITEQUEUEMODULE 1
#else
#define OMNI_HAS_IIMAGEWRITEQUEUEMODULE 0
#endif

FOmniCapturePNGWriter::FOmniCapturePNGWriter() {}
FOmniCapturePNGWriter::~FOmniCapturePNGWriter() { Flush(); }

void FOmniCapturePNGWriter::Initialize(const FOmniCaptureSettings& Settings, const FString& InOutputDirectory)
{
    OutputDirectory = InOutputDirectory.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("OmniCaptures") : InOutputDirectory;
    SequenceBaseName = Settings.OutputFileName;
    OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);

    // 使用模块接口加载队列
    if (OMNI_HAS_IIMAGEWRITEQUEUEMODULE)
    {
        ImageWriteQueue = &FModuleManager::LoadModuleChecked<IImageWriteQueueModule>(TEXT("ImageWriteQueue")).GetImageWriteQueue();
    }
    else
    {
        // 备用队列
        static FImageWriteQueue LocalQueue;
        ImageWriteQueue = &LocalQueue;
    }
}

void FOmniCapturePNGWriter::EnqueueFrame(TUniquePtr<FOmniCaptureFrame>&& Frame, const FString& FrameFileName)
{
    if (!ImageWriteQueue || !Frame.IsValid()) { return; }

    TUniquePtr<FImageWriteTask> Task = MakeUnique<FImageWriteTask>();
    Task->Format = EImageFormat::PNG;
    Task->Filename = OutputDirectory / FrameFileName;
    Task->CompressionQuality = static_cast<int32>(EImageCompressionQuality::Uncompressed);
    Task->bOverwriteFile = true;
    Task->PixelData = MoveTemp(Frame->PixelData);
    Task->bSupports16Bit = Frame->bLinearColor;

    ImageWriteQueue->Enqueue(MoveTemp(Task));

    FScopeLock Lock(&MetadataCS);
    CapturedMetadata.Add(Frame->Metadata);
}

void FOmniCapturePNGWriter::Flush()
{
    if (ImageWriteQueue) { ImageWriteQueue->Flush(); ImageWriteQueue = nullptr; }
}

TArray<FOmniCaptureFrameMetadata> FOmniCapturePNGWriter::ConsumeCapturedFrames()
{
    FScopeLock Lock(&MetadataCS);
    TArray<FOmniCaptureFrameMetadata> Result = MoveTemp(CapturedMetadata);
    CapturedMetadata.Reset();
    return Result;
}

