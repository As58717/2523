#include "OmniCaptureImageWriter.h"


#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

THIRD_PARTY_INCLUDES_START
#include "png.h"
THIRD_PARTY_INCLUDES_END

namespace
{
    constexpr int32 DefaultJpegQuality = 85;

    TSharedPtr<IImageWrapper> CreateImageWrapper(EImageFormat Format)
    {
        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        return ImageWrapperModule.CreateImageWrapper(Format);
    }

    bool WritePNGWithImageWrapper(const FString& FilePath, const FIntPoint& Size, const void* RawData, int64 RawSizeInBytes, ERGBFormat Format, int32 BitDepth)
    {
        if (!RawData || RawSizeInBytes <= 0 || Size.X <= 0 || Size.Y <= 0)
        {
            return false;
        }

        const TSharedPtr<IImageWrapper> ImageWrapper = CreateImageWrapper(EImageFormat::PNG);
        if (!ImageWrapper.IsValid())
        {
            return false;
        }

        if (!ImageWrapper->SetRaw(static_cast<const uint8*>(RawData), RawSizeInBytes, Size.X, Size.Y, Format, BitDepth))
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

    FString NormalizeFilePath(const FString& InPath)
    {
        FString Normalized = InPath;
        FPaths::MakeStandardFilename(Normalized);
        return Normalized;
    }

    int32 GetChannelCountForFormat(ERGBFormat Format)
    {
        switch (Format)
        {
        case ERGBFormat::Gray:
        case ERGBFormat::GrayF:
            return 1;
        case ERGBFormat::RGBA:
        case ERGBFormat::BGRA:
        case ERGBFormat::RGBAF:
            return 4;
        default:
            return 0;
        }
    }

    int32 GetPngColorType(ERGBFormat Format)
    {
        switch (Format)
        {
        case ERGBFormat::Gray:
        case ERGBFormat::GrayF:
            return PNG_COLOR_TYPE_GRAY;
        case ERGBFormat::RGBA:
        case ERGBFormat::BGRA:
        case ERGBFormat::RGBAF:
            return PNG_COLOR_TYPE_RGBA;
        default:
            return -1;
        }
    }

    void PngWriteDataCallback(png_structp PngPtr, png_bytep Data, png_size_t Length)
    {
        FArchive* Archive = static_cast<FArchive*>(png_get_io_ptr(PngPtr));
        if (!Archive)
        {
            png_error(PngPtr, "Invalid archive writer");
            return;
        }

        Archive->Serialize(Data, Length);
        if (Archive->IsError())
        {
            png_error(PngPtr, "Failed to write PNG data");
        }
    }

    void PngFlushCallback(png_structp PngPtr)
    {
        FArchive* Archive = static_cast<FArchive*>(png_get_io_ptr(PngPtr));
        if (Archive)
        {
            Archive->Flush();
        }
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
    MaxPendingTasks = FMath::Max(1, Settings.MaxPendingImageTasks);
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
    EnforcePendingTaskLimit();

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
    const int32 Channels = GetChannelCountForFormat(Format);
    if (Channels <= 0 || Size.X <= 0 || Size.Y <= 0)
    {
        return false;
    }

    const int32 BytesPerChannel = BitDepth / 8;
    if (BytesPerChannel <= 0)
    {
        return false;
    }

    const int64 BytesPerRow = static_cast<int64>(Size.X) * Channels * BytesPerChannel;
    if (BytesPerRow <= 0)
    {
        return false;
    }

    if (RawSizeInBytes < BytesPerRow * Size.Y)
    {
        return false;
    }

    const uint8* BasePtr = static_cast<const uint8*>(RawData);
    auto PrepareRows = [BasePtr, BytesPerRow](int32 RowStart, int32 RowCount, int64, TArray64<uint8>& TempBuffer, TArray<uint8*>& RowPointers)
    {
        (void)TempBuffer;
        for (int32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
        {
            RowPointers[RowIndex] = const_cast<uint8*>(BasePtr + (static_cast<int64>(RowStart + RowIndex) * BytesPerRow));
        }
    };

    if (WritePNGWithRowSource(FilePath, Size, Format, BitDepth, PrepareRows))
    {
        return true;
    }

    if (BitDepth == 8)
    {
        return WritePNGWithImageWrapper(FilePath, Size, RawData, RawSizeInBytes, Format, BitDepth);
    }

    return false;
}

bool FOmniCaptureImageWriter::WritePNGWithRowSource(const FString& FilePath, const FIntPoint& Size, ERGBFormat Format, int32 BitDepth, TFunctionRef<void(int32 RowStart, int32 RowCount, int64 BytesPerRow, TArray64<uint8>& TempBuffer, TArray<uint8*>& RowPointers)> PrepareRows) const
{
#if WITH_LIBPNG
    const int32 Channels = GetChannelCountForFormat(Format);
    if (Channels <= 0 || BitDepth <= 0 || Size.X <= 0 || Size.Y <= 0)
    {
        return false;
    }

    const int32 ColorType = GetPngColorType(Format);
    if (ColorType < 0)
    {
        return false;
    }

    const int32 BytesPerChannel = BitDepth / 8;
    if (BytesPerChannel <= 0)
    {
        return false;
    }

    const int64 BytesPerRow = static_cast<int64>(Size.X) * Channels * BytesPerChannel;
    if (BytesPerRow <= 0)
    {
        return false;
    }

    IFileManager::Get().Delete(*FilePath, false, true, false);
    TUniquePtr<FArchive> Archive(IFileManager::Get().CreateFileWriter(*FilePath));
    if (!Archive.IsValid())
    {
        return false;
    }

    png_structp PngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!PngPtr)
    {
        Archive->Close();
        return false;
    }

    png_infop InfoPtr = png_create_info_struct(PngPtr);
    if (!InfoPtr)
    {
        png_destroy_write_struct(&PngPtr, nullptr);
        Archive->Close();
        return false;
    }

    if (setjmp(png_jmpbuf(PngPtr)))
    {
        png_destroy_write_struct(&PngPtr, &InfoPtr);
        Archive->Close();
        IFileManager::Get().Delete(*FilePath, false, true, true);
        return false;
    }

    png_set_write_fn(PngPtr, Archive.Get(), PngWriteDataCallback, PngFlushCallback);
    png_set_IHDR(PngPtr, InfoPtr, Size.X, Size.Y, BitDepth, ColorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    if (BitDepth == 16)
    {
        png_set_swap(PngPtr);
    }

    if (Format == ERGBFormat::BGRA)
    {
        png_set_bgr(PngPtr);
    }

    png_write_info(PngPtr, InfoPtr);

    const int64 DesiredChunkBytes = 64ll * 1024ll * 1024ll;
    const int64 SafeRowSize = FMath::Max<int64>(BytesPerRow, 1);
    const int32 MaxRowsPerChunk = FMath::Max<int32>(1, static_cast<int32>(FMath::Min<int64>(Size.Y, DesiredChunkBytes / SafeRowSize)));

    TArray64<uint8> TempBuffer;
    TArray<uint8*> RowPointers;
    RowPointers.Reserve(MaxRowsPerChunk);

    int32 RowIndex = 0;
    while (RowIndex < Size.Y)
    {
        const int32 RowsThisPass = FMath::Min(MaxRowsPerChunk, Size.Y - RowIndex);
        RowPointers.SetNum(RowsThisPass, EAllowShrinking::No);
        PrepareRows(RowIndex, RowsThisPass, BytesPerRow, TempBuffer, RowPointers);
        png_write_rows(PngPtr, reinterpret_cast<png_bytep*>(RowPointers.GetData()), RowsThisPass);
        RowIndex += RowsThisPass;
    }

    png_write_end(PngPtr, InfoPtr);
    png_destroy_write_struct(&PngPtr, &InfoPtr);

    Archive->Close();
    return !Archive->IsError();
#else
    return false;
#endif
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
        auto PrepareRows = [&Pixels, &Size](int32 RowStart, int32 RowCount, int64 BytesPerRow, TArray64<uint8>& TempBuffer, TArray<uint8*>& RowPointers)
        {
            const int64 RequiredSize = BytesPerRow * RowCount;
            TempBuffer.SetNum(RequiredSize, EAllowShrinking::No);

            for (int32 Row = 0; Row < RowCount; ++Row)
            {
                uint8* RowData = TempBuffer.GetData() + BytesPerRow * Row;
                RowPointers[Row] = RowData;
                uint16* Dest = reinterpret_cast<uint16*>(RowData);
                const int64 PixelRowStart = static_cast<int64>(RowStart + Row) * Size.X;
                for (int32 Column = 0; Column < Size.X; ++Column)
                {
                    const FColor& Pixel = Pixels[PixelRowStart + Column];
                    *Dest++ = static_cast<uint16>(Pixel.B) * 257u;
                    *Dest++ = static_cast<uint16>(Pixel.G) * 257u;
                    *Dest++ = static_cast<uint16>(Pixel.R) * 257u;
                    *Dest++ = static_cast<uint16>(Pixel.A) * 257u;
                }
            }
        };

        return WritePNGWithRowSource(FilePath, Size, ERGBFormat::BGRA, 16, PrepareRows);
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
        auto PrepareRows = [&PixelData, &Size](int32 RowStart, int32 RowCount, int64 BytesPerRow, TArray64<uint8>& TempBuffer, TArray<uint8*>& RowPointers)
        {
            const int64 RequiredSize = BytesPerRow * RowCount;
            TempBuffer.SetNum(RequiredSize, EAllowShrinking::No);

            const auto ToUInt16 = [](float Value) -> uint16
            {
                const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
                return static_cast<uint16>(FMath::RoundToInt(Clamped * 65535.0f));
            };

            for (int32 Row = 0; Row < RowCount; ++Row)
            {
                uint8* RowData = TempBuffer.GetData() + BytesPerRow * Row;
                RowPointers[Row] = RowData;
                uint16* Dest = reinterpret_cast<uint16*>(RowData);
                const int64 PixelRowStart = static_cast<int64>(RowStart + Row) * Size.X;
                for (int32 Column = 0; Column < Size.X; ++Column)
                {
                    const FFloat16Color& Pixel = PixelData.Pixels[PixelRowStart + Column];
                    *Dest++ = ToUInt16(Pixel.B.GetFloat());
                    *Dest++ = ToUInt16(Pixel.G.GetFloat());
                    *Dest++ = ToUInt16(Pixel.R.GetFloat());
                    *Dest++ = ToUInt16(Pixel.A.GetFloat());
                }
            }
        };

        return WritePNGWithRowSource(FilePath, Size, ERGBFormat::BGRA, 16, PrepareRows);
    }

    const int64 PixelCount = static_cast<int64>(Size.X) * Size.Y;
    const int64 BytesPerRow = static_cast<int64>(Size.X) * 4;
    TArray64<uint8> ConvertedPixels;
    ConvertedPixels.SetNum(PixelCount * 4, EAllowShrinking::No);

    for (int64 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
    {
        const FFloat16Color& Pixel = PixelData.Pixels[PixelIndex];
        const FLinearColor Linear(
            Pixel.R.GetFloat(),
            Pixel.G.GetFloat(),
            Pixel.B.GetFloat(),
            Pixel.A.GetFloat());
        const FColor Converted = Linear.ToFColor(true);
        const int64 Offset = PixelIndex * 4;
        ConvertedPixels[Offset + 0] = Converted.B;
        ConvertedPixels[Offset + 1] = Converted.G;
        ConvertedPixels[Offset + 2] = Converted.R;
        ConvertedPixels[Offset + 3] = Converted.A;
    }

    uint8* ConvertedBasePtr = ConvertedPixels.GetData();
    auto PrepareRows = [ConvertedBasePtr, BytesPerRow](int32 RowStart, int32 RowCount, int64, TArray64<uint8>& TempBuffer, TArray<uint8*>& RowPointers)
    {
        (void)TempBuffer;
        for (int32 Row = 0; Row < RowCount; ++Row)
        {
            RowPointers[Row] = ConvertedBasePtr + BytesPerRow * (RowStart + Row);
        }
    };

    if (WritePNGWithRowSource(FilePath, Size, ERGBFormat::BGRA, 8, PrepareRows))
    {
        return true;
    }

    return WritePNGWithImageWrapper(FilePath, Size, ConvertedPixels.GetData(), ConvertedPixels.Num(), ERGBFormat::BGRA, 8);
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

void FOmniCaptureImageWriter::EnforcePendingTaskLimit()
{
    if (MaxPendingTasks <= 0)
    {
        return;
    }

    while (true)
    {
        TFuture<bool> TaskToWait;
        {
            FScopeLock Lock(&PendingTasksCS);
            if (PendingTasks.Num() <= MaxPendingTasks)
            {
                break;
            }

            TaskToWait = MoveTemp(PendingTasks[0]);
            PendingTasks.RemoveAt(0, 1, EAllowShrinking::No);
        }

        if (TaskToWait.IsValid())
        {
            const bool bResult = TaskToWait.Get();
            if (!bResult)
            {
                UE_LOG(LogTemp, Warning, TEXT("OmniCapture image write task failed"));
            }
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
