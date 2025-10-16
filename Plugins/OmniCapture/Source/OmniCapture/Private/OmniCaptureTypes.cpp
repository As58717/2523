#include "OmniCaptureTypes.h"
#include "ImagePixelData.h"  //  ImagePixelData

FIntPoint FOmniCaptureSettings::GetEquirectResolution() const
{
    const bool bStereo = Mode == EOmniCaptureMode::Stereo;
    const bool bSideBySide = bStereo && StereoLayout == EOmniCaptureStereoLayout::SideBySide;
    const bool bHalfSphere = Coverage == EOmniCaptureCoverage::HalfSphere;

    int32 OutputWidth = Resolution * (bHalfSphere ? 1 : 2);
    int32 OutputHeight = Resolution;

    if (bStereo)
    {
        if (bSideBySide)
        {
            OutputWidth *= 2;
        }
        else
        {
            OutputHeight *= 2;
        }
    }

    return FIntPoint(OutputWidth, OutputHeight);
}

float FOmniCaptureSettings::GetLongitudeSpanRadians() const
{
    return Coverage == EOmniCaptureCoverage::HalfSphere ? PI * 0.5f : PI;
}

// 示例：创建一个 FImagePixelData 对象
TUniquePtr<FImagePixelData> CreateImagePixelData(const FIntPoint& Size)
{ 
    // 创建一个 FImagePixelData 对象
    TUniquePtr<FImagePixelData> PixelData = MakeUnique<FImagePixelData>(Size);
    if (!PixelData)
    {
        return nullptr;
    }

    // 初始化数据（例如全透明）
    for (int32 i = 0; i < Size.X * Size.Y; ++i)
    {
        PixelData->Pixels[i] = FColor::Transparent;  // 全透明
    }

    return PixelData;
}

// 示例：转换线性颜色到 FImagePixelData
TUniquePtr<FImagePixelData> ConvertLinearToImagePixelData(const TArray<FLinearColor>& LinearColors, const FIntPoint& Size)
{
    // 创建一个 FImagePixelData 对象
    TUniquePtr<FImagePixelData> PixelData = MakeUnique<FImagePixelData>(Size);
    if (!PixelData)
    {
        return nullptr;
    }

    // 将线性颜色转换为 FColor 存入 FImagePixelData
    for (int32 i = 0; i < Size.X * Size.Y; ++i)
    {
        PixelData->Pixels[i] = LinearColors[i].ToFColor(true);  // 转换为 FColor
    }

    return PixelData;
}

// 示例：从像素数据中读取特定颜色
FLinearColor ReadPixelFromImageData(const TUniquePtr<FImagePixelData>& PixelData, int32 X, int32 Y)
{
    if (!PixelData || X < 0 || Y < 0 || X >= PixelData->Size.X || Y >= PixelData->Size.Y)
    {
        return FLinearColor::Black;  // 如果无效返回黑色
    }

    return PixelData->Pixels[Y * PixelData->Size.X + X].ReinterpretAsLinear();  // 转换为线性颜色
}

// 其他与图像相关的功能实现可以继续在这里补充

