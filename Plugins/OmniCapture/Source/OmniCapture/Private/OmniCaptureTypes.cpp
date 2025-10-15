#include "OmniCaptureTypes.h"
#include "ImageCore/Public/ImagePixelData.h"  // ���� ImagePixelData

// ʾ��������һ�� FImagePixelData ����
TUniquePtr<FImagePixelData> CreateImagePixelData(const FIntPoint& Size)
{
    // ����һ�� FImagePixelData ����
    TUniquePtr<FImagePixelData> PixelData = MakeUnique<FImagePixelData>(Size);
    if (!PixelData)
    {
        return nullptr;
    }

    // ��ʼ�����ݣ�����ȫ͸����
    for (int32 i = 0; i < Size.X * Size.Y; ++i)
    {
        PixelData->Pixels[i] = FColor::Transparent;  // ȫ͸��
    }

    return PixelData;
}

// ʾ����ת��������ɫ�� FImagePixelData
TUniquePtr<FImagePixelData> ConvertLinearToImagePixelData(const TArray<FLinearColor>& LinearColors, const FIntPoint& Size)
{
    // ����һ�� FImagePixelData ����
    TUniquePtr<FImagePixelData> PixelData = MakeUnique<FImagePixelData>(Size);
    if (!PixelData)
    {
        return nullptr;
    }

    // ��������ɫת��Ϊ FColor ���� FImagePixelData
    for (int32 i = 0; i < Size.X * Size.Y; ++i)
    {
        PixelData->Pixels[i] = LinearColors[i].ToFColor(true);  // ת��Ϊ FColor
    }

    return PixelData;
}

// ʾ���������������ж�ȡ�ض���ɫ
FLinearColor ReadPixelFromImageData(const TUniquePtr<FImagePixelData>& PixelData, int32 X, int32 Y)
{
    if (!PixelData || X < 0 || Y < 0 || X >= PixelData->Size.X || Y >= PixelData->Size.Y)
    {
        return FLinearColor::Black;  // �����Ч���غ�ɫ
    }

    return PixelData->Pixels[Y * PixelData->Size.X + X].ReinterpretAsLinear();  // ת��Ϊ������ɫ
}

// ������ͼ����صĹ���ʵ�ֿ��Լ��������ﲹ��

