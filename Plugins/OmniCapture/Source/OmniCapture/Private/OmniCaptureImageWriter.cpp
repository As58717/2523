#include "OmniCaptureImageWriter.h"


#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace
{
    constexpr int32 DefaultJpegQuality = 85;

    TSharedPtr<IImageWrapper> CreateImageWrapper(EImageFormat Format)
    {
        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        return ImageWrapperModule.CreateImageWrapper(Format);
    }

    FString NormalizeFilePath(const FString& InPath)
    {
        FString Normalized = InPath;
        FPaths::MakeStandardFilename(Normalized);
        return Normalized;
    }
}

FOmniCaptureImageWriter::FOmniCaptureImageWriter() {}
FOmniCaptureImageWriter::~FOmniCaptureImageWriter() { Flush(); }

void FOmniCaptureImageWriter::Initialize(const FOmniCaptureSettings& Settings, const FString& InOutputDirectory)
{
    OutputDirectory = InOutputDirectory.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("OmniCaptures") : InOutputDirectory;
    SequenceBaseName = Settings.OutputFileName;
    OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    TargetFormat = Settings.ImageFormat;
    TargetPNGBitDepth = Settings.PNGBitDepth;
    bInitialized = true;
}

void FOmniCaptureImageWriter::EnqueueFrame(TUniquePtr<FOmniCaptureFrame>&& Frame, const FString& FrameFileName)
{
    if (!bInitialized || !Frame.IsValid())
    {
        return;
    }

    FString TargetPath = NormalizeFilePath(OutputDirectory / FrameFileName);
    FOmniCaptureFrameMetadata Metadata = Frame->Metadata;
    bool bIsLinear = Frame->bLinearColor;

    TUniquePtr<FImagePixelData> PixelData = MoveTemp(Frame->PixelData);
    if (!PixelData.IsValid())
    {
        return;
    }

    TFuture<bool> Future = Async(EAsyncExecution::ThreadPool, [this, FilePath = MoveTemp(TargetPath), Format = TargetFormat, bIsLinear, PixelData = MoveTemp(PixelData)]() mutable
    {
        return WritePixelDataToDisk(MoveTemp(PixelData), FilePath, Format, bIsLinear);
    });

    TrackPendingTask(MoveTemp(Future));
    PruneCompletedTasks();

    {
        FScopeLock Lock(&MetadataCS);
        CapturedMetadata.Add(Metadata);
    }
}

void FOmniCaptureImageWriter::Flush()
{
    WaitForAllTasks();
    bInitialized = false;
}

TArray<FOmniCaptureFrameMetadata> FOmniCaptureImageWriter::ConsumeCapturedFrames()
{
    FScopeLock Lock(&MetadataCS);
    TArray<FOmniCaptureFrameMetadata> Result = MoveTemp(CapturedMetadata);
    CapturedMetadata.Reset();
    return Result;
}

bool FOmniCaptureImageWriter::WritePixelDataToDisk(TUniquePtr<FImagePixelData> PixelData, const FString& FilePath, EOmniCaptureImageFormat Format, bool bIsLinear) const
{
    if (!PixelData.IsValid())
    {
        return false;
    }

    bool bWriteSuccessful = false;

    switch (Format)
    {
    case EOmniCaptureImageFormat::JPG:
        if (bIsLinear)
        {
            const TImagePixelData<FFloat16Color>* FloatData = static_cast<const TImagePixelData<FFloat16Color>*>(PixelData.Get());
            bWriteSuccessful = WriteJPEGFromLinear(*FloatData, FilePath);
        }
        else
        {
            const TImagePixelData<FColor>* ColorData = static_cast<const TImagePixelData<FColor>*>(PixelData.Get());
            bWriteSuccessful = WriteJPEG(*ColorData, FilePath);
        }
        break;
    case EOmniCaptureImageFormat::EXR:
        if (bIsLinear)
        {
            const TImagePixelData<FFloat16Color>* LinearData = static_cast<const TImagePixelData<FFloat16Color>*>(PixelData.Get());
            bWriteSuccessful = WriteEXR(*LinearData, FilePath);
        }
        else
        {
            const TImagePixelData<FColor>* SRGBData = static_cast<const TImagePixelData<FColor>*>(PixelData.Get());
            bWriteSuccessful = WriteEXRFromColor(*SRGBData, FilePath);
        }
        break;
    case EOmniCaptureImageFormat::BMP:
        if (bIsLinear)
        {
            const TImagePixelData<FFloat16Color>* LinearBMP = static_cast<const TImagePixelData<FFloat16Color>*>(PixelData.Get());
            bWriteSuccessful = WriteBMPFromLinear(*LinearBMP, FilePath);
        }
        else
        {
            const TImagePixelData<FColor>* BmpColor = static_cast<const TImagePixelData<FColor>*>(PixelData.Get());
            bWriteSuccessful = WriteBMP(*BmpColor, FilePath);
        }
        break;
    case EOmniCaptureImageFormat::PNG:
    default:
        if (bIsLinear)
        {
            const TImagePixelData<FFloat16Color>* LinearData = static_cast<const TImagePixelData<FFloat16Color>*>(PixelData.Get());
            bWriteSuccessful = WritePNGFromLinear(*LinearData, FilePath);
        }
        else
        {
            const TImagePixelData<FColor>* PngData = static_cast<const TImagePixelData<FColor>*>(PixelData.Get());
            bWriteSuccessful = WritePNG(*PngData, FilePath);
        }
        break;
    }

    if (!bWriteSuccessful)
    {
        UE_LOG(LogTemp, Warning, TEXT("Unsupported pixel data type for OmniCapture image export (%s)"), *FilePath);
    }

    return bWriteSuccessful;
}

bool FOmniCaptureImageWriter::WritePNGRaw(const FString& FilePath, const FIntPoint& Size, const void* RawData, int64 RawSizeInBytes, ERGBFormat Format, int32 BitDepth) const
{
    const TSharedPtr<IImageWrapper> ImageWrapper = CreateImageWrapper(EImageFormat::PNG);
    if (!ImageWrapper.IsValid())
    {
        return false;
    }

    if (!ImageWrapper->SetRaw(static_cast<const uint8*>(RawData), RawSizeInBytes, Size.X, Size.Y, Format, BitDepth))
    {
        return false;
    }

    const TArray64<uint8> CompressedData = ImageWrapper->GetCompressed();
    if (CompressedData.Num() == 0)
    {
        return false;
    }

    IFileManager::Get().Delete(*FilePath, false, true, false);
    return FFileHelper::SaveArrayToFile(CompressedData, *FilePath);
}

bool FOmniCaptureImageWriter::WritePNG(const TImagePixelData<FColor>& PixelData, const FString& FilePath) const
{
    const FIntPoint Size = PixelData.GetSize();
    const TArray64<FColor>& Pixels = PixelData.Pixels;
    if (Pixels.Num() != Size.X * Size.Y)
    {
        return false;
    }

    if (TargetPNGBitDepth == EOmniCapturePNGBitDepth::BitDepth16)
    {
        const int64 PixelCount = Pixels.Num();
        TArray64<uint16> Expanded;
        Expanded.SetNum(PixelCount * 4);

        const auto ExpandChannel = [](uint8 Value) -> uint16
        {
            return static_cast<uint16>(Value) * 257u;
        };

        for (int64 Index = 0; Index < PixelCount; ++Index)
        {
            const FColor& Pixel = Pixels[Index];
            const int64 BaseIndex = Index * 4;
            Expanded[BaseIndex + 0] = ExpandChannel(Pixel.B);
            Expanded[BaseIndex + 1] = ExpandChannel(Pixel.G);
            Expanded[BaseIndex + 2] = ExpandChannel(Pixel.R);
            Expanded[BaseIndex + 3] = ExpandChannel(Pixel.A);
        }

        return WritePNGRaw(FilePath, Size, Expanded.GetData(), Expanded.Num() * sizeof(uint16), ERGBFormat::BGRA, 16);
    }

    return WritePNGRaw(FilePath, Size, Pixels.GetData(), Pixels.Num() * sizeof(FColor), ERGBFormat::BGRA, 8);
}

bool FOmniCaptureImageWriter::WriteBMP(const TImagePixelData<FColor>& PixelData, const FString& FilePath) const
{
    const TSharedPtr<IImageWrapper> ImageWrapper = CreateImageWrapper(EImageFormat::BMP);
    if (!ImageWrapper.IsValid())
    {
        return false;
    }

    const FIntPoint Size = PixelData.GetSize();
    const TArray64<FColor>& Pixels = PixelData.Pixels;
    if (Pixels.Num() != Size.X * Size.Y)
    {
        return false;
    }

    if (!ImageWrapper->SetRaw(reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FColor), Size.X, Size.Y, ERGBFormat::BGRA, 8))
    {
        return false;
    }

    const TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(0);
    if (CompressedData.Num() == 0)
    {
        return false;
    }

    IFileManager::Get().Delete(*FilePath, false, true, false);
    return FFileHelper::SaveArrayToFile(CompressedData, *FilePath);
}

bool FOmniCaptureImageWriter::WritePNGFromLinear(const TImagePixelData<FFloat16Color>& PixelData, const FString& FilePath) const
{
    const FIntPoint Size = PixelData.GetSize();
    const int32 ExpectedCount = Size.X * Size.Y;
    if (PixelData.Pixels.Num() != ExpectedCount)
    {
        return false;
    }

    if (TargetPNGBitDepth == EOmniCapturePNGBitDepth::BitDepth16)
    {
        TArray64<uint16> Expanded;
        Expanded.SetNum(static_cast<int64>(ExpectedCount) * 4);

        const auto ToUInt16 = [](float Value) -> uint16
        {
            const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
            return static_cast<uint16>(FMath::RoundToInt(Clamped * 65535.0f));
        };

        for (int32 Index = 0; Index < ExpectedCount; ++Index)
        {
            const FFloat16Color& Pixel = PixelData.Pixels[Index];
            const int64 BaseIndex = static_cast<int64>(Index) * 4;
            Expanded[BaseIndex + 0] = ToUInt16(Pixel.B.GetFloat());
            Expanded[BaseIndex + 1] = ToUInt16(Pixel.G.GetFloat());
            Expanded[BaseIndex + 2] = ToUInt16(Pixel.R.GetFloat());
            Expanded[BaseIndex + 3] = ToUInt16(Pixel.A.GetFloat());
        }

        return WritePNGRaw(FilePath, Size, Expanded.GetData(), Expanded.Num() * sizeof(uint16), ERGBFormat::BGRA, 16);
    }

    TArray<FColor> Converted;
    Converted.SetNum(ExpectedCount);
    for (int32 Index = 0; Index < ExpectedCount; ++Index)
    {
        const FFloat16Color& Pixel = PixelData.Pixels[Index];
        const FLinearColor Linear(
            Pixel.R.GetFloat(),
            Pixel.G.GetFloat(),
            Pixel.B.GetFloat(),
            Pixel.A.GetFloat());
        Converted[Index] = Linear.ToFColor(true);
    }

    TUniquePtr<TImagePixelData<FColor>> TempData = MakeUnique<TImagePixelData<FColor>>(Size);
    TempData->Pixels = MoveTemp(Converted);
    return WritePNG(*TempData, FilePath);
}

bool FOmniCaptureImageWriter::WriteBMPFromLinear(const TImagePixelData<FFloat16Color>& PixelData, const FString& FilePath) const
{
    const FIntPoint Size = PixelData.GetSize();
    const int32 ExpectedCount = Size.X * Size.Y;
    if (PixelData.Pixels.Num() != ExpectedCount)
    {
        return false;
    }

    TArray<FColor> Converted;
    Converted.SetNum(ExpectedCount);
    for (int32 Index = 0; Index < ExpectedCount; ++Index)
    {
        const FFloat16Color& Pixel = PixelData.Pixels[Index];
        const FLinearColor Linear(
            Pixel.R.GetFloat(),
            Pixel.G.GetFloat(),
            Pixel.B.GetFloat(),
            Pixel.A.GetFloat());
        Converted[Index] = Linear.ToFColor(true);
    }

    TUniquePtr<TImagePixelData<FColor>> TempData = MakeUnique<TImagePixelData<FColor>>(Size);
    TempData->Pixels = MoveTemp(Converted);
    return WriteBMP(*TempData, FilePath);
}

bool FOmniCaptureImageWriter::WriteJPEG(const TImagePixelData<FColor>& PixelData, const FString& FilePath) const
{
    const TSharedPtr<IImageWrapper> ImageWrapper = CreateImageWrapper(EImageFormat::JPEG);
    if (!ImageWrapper.IsValid())
    {
        return false;
    }

    const FIntPoint Size = PixelData.GetSize();
    const TArray64<FColor>& Pixels = PixelData.Pixels;
    if (Pixels.Num() != Size.X * Size.Y)
    {
        return false;
    }

    if (!ImageWrapper->SetRaw(reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FColor), Size.X, Size.Y, ERGBFormat::BGRA, 8))
    {
        return false;
    }

    const TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(DefaultJpegQuality);
    if (CompressedData.Num() == 0)
    {
        return false;
    }

    IFileManager::Get().Delete(*FilePath, false, true, false);
    return FFileHelper::SaveArrayToFile(CompressedData, *FilePath);
}

bool FOmniCaptureImageWriter::WriteJPEGFromLinear(const TImagePixelData<FFloat16Color>& PixelData, const FString& FilePath) const
{
    const FIntPoint Size = PixelData.GetSize();
    const int32 ExpectedCount = Size.X * Size.Y;
    if (PixelData.Pixels.Num() != ExpectedCount)
    {
        return false;
    }

    TArray<FColor> Converted;
    Converted.SetNum(ExpectedCount);
    for (int32 Index = 0; Index < ExpectedCount; ++Index)
    {
        const FFloat16Color& Pixel = PixelData.Pixels[Index];
        const FLinearColor Linear(
            Pixel.R.GetFloat(),
            Pixel.G.GetFloat(),
            Pixel.B.GetFloat(),
            Pixel.A.GetFloat());
        Converted[Index] = Linear.ToFColor(true);
    }

    TUniquePtr<TImagePixelData<FColor>> TempData = MakeUnique<TImagePixelData<FColor>>(Size);
    TempData->Pixels = MoveTemp(Converted);
    return WriteJPEG(*TempData, FilePath);
}

bool FOmniCaptureImageWriter::WriteEXR(const TImagePixelData<FFloat16Color>& PixelData, const FString& FilePath) const
{
    const TSharedPtr<IImageWrapper> ImageWrapper = CreateImageWrapper(EImageFormat::EXR);
    if (!ImageWrapper.IsValid())
    {
        return false;
    }

    const FIntPoint Size = PixelData.GetSize();
    const TArray64<FFloat16Color>& Pixels = PixelData.Pixels;
    if (Pixels.Num() != Size.X * Size.Y)
    {
        return false;
    }

    if (!ImageWrapper->SetRaw(reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FFloat16Color), Size.X, Size.Y, ERGBFormat::RGBA, 16))
    {
        return false;
    }

    const TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(0);
    if (CompressedData.Num() == 0)
    {
        return false;
    }

    IFileManager::Get().Delete(*FilePath, false, true, false);
    return FFileHelper::SaveArrayToFile(CompressedData, *FilePath);
}

bool FOmniCaptureImageWriter::WriteEXRFromColor(const TImagePixelData<FColor>& PixelData, const FString& FilePath) const
{
    const FIntPoint Size = PixelData.GetSize();
    const int32 ExpectedCount = Size.X * Size.Y;
    if (PixelData.Pixels.Num() != ExpectedCount)
    {
        return false;
    }

    TArray<FFloat16Color> Converted;
    Converted.SetNum(ExpectedCount);
    for (int32 Index = 0; Index < ExpectedCount; ++Index)
    {
        Converted[Index] = FFloat16Color(PixelData.Pixels[Index].ReinterpretAsLinear());
    }

    TUniquePtr<TImagePixelData<FFloat16Color>> TempData = MakeUnique<TImagePixelData<FFloat16Color>>(Size);
    TempData->Pixels = MoveTemp(Converted);
    return WriteEXR(*TempData, FilePath);
}

void FOmniCaptureImageWriter::TrackPendingTask(TFuture<bool>&& TaskFuture)
{
    FScopeLock Lock(&PendingTasksCS);
    PendingTasks.Add(MoveTemp(TaskFuture));
}

void FOmniCaptureImageWriter::PruneCompletedTasks()
{
    FScopeLock Lock(&PendingTasksCS);
    for (int32 Index = PendingTasks.Num() - 1; Index >= 0; --Index)
    {
        if (PendingTasks[Index].IsReady())
        {
            const bool bResult = PendingTasks[Index].Get();
            if (!bResult)
            {
                UE_LOG(LogTemp, Warning, TEXT("OmniCapture image write task failed"));
            }
            PendingTasks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
        }
    }
}

void FOmniCaptureImageWriter::WaitForAllTasks()
{
    TArray<TFuture<bool>> TasksToWait;
    {
        FScopeLock Lock(&PendingTasksCS);
        TasksToWait = MoveTemp(PendingTasks);
        PendingTasks.Reset();
    }

    for (TFuture<bool>& Task : TasksToWait)
    {
        const bool bResult = Task.Get();
        if (!bResult)
        {
            UE_LOG(LogTemp, Warning, TEXT("OmniCapture image write task failed"));
        }
    }
}
