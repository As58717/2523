#include "OmniCaptureTypes.h"

#include "Math/UnrealMathUtility.h"

namespace
{
    FORCEINLINE int32 AlignDimension(int32 Value, int32 Alignment)
    {
        if (Value <= 0)
        {
            return Alignment > 0 ? Alignment : 1;
        }

        if (Alignment <= 1)
        {
            return Value;
        }

        const int32 Rounded = ((Value + Alignment - 1) / Alignment) * Alignment;
        return FMath::Max(Alignment, Rounded);
    }
}

FIntPoint FOmniCaptureSettings::GetEquirectResolution() const
{
    const bool bHalfSphere = IsVR180();
    const int32 Alignment = GetEncoderAlignmentRequirement();

    FIntPoint EyeResolution(Resolution * (bHalfSphere ? 1 : 2), Resolution);
    EyeResolution.X = AlignDimension(EyeResolution.X, Alignment);
    EyeResolution.Y = AlignDimension(EyeResolution.Y, Alignment);

    FIntPoint OutputResolution = EyeResolution;

    if (IsStereo())
    {
        if (StereoLayout == EOmniCaptureStereoLayout::SideBySide)
        {
            OutputResolution.X = AlignDimension(EyeResolution.X * 2, Alignment);
            OutputResolution.Y = AlignDimension(EyeResolution.Y, Alignment);
            OutputResolution.X = AlignDimension(OutputResolution.X, 2);
        }
        else
        {
            OutputResolution.X = AlignDimension(EyeResolution.X, Alignment);
            OutputResolution.Y = AlignDimension(EyeResolution.Y * 2, Alignment);
            OutputResolution.Y = AlignDimension(OutputResolution.Y, 2);
        }
    }
    else
    {
        OutputResolution.X = AlignDimension(OutputResolution.X, Alignment);
        OutputResolution.Y = AlignDimension(OutputResolution.Y, Alignment);
    }

    OutputResolution.X = FMath::Max(2, OutputResolution.X);
    OutputResolution.Y = FMath::Max(2, OutputResolution.Y);

    return OutputResolution;
}

FIntPoint FOmniCaptureSettings::GetPerEyeOutputResolution() const
{
    const FIntPoint Output = GetEquirectResolution();
    if (!IsStereo())
    {
        return Output;
    }

    if (StereoLayout == EOmniCaptureStereoLayout::SideBySide)
    {
        return FIntPoint(FMath::Max(1, Output.X / 2), Output.Y);
    }

    return FIntPoint(Output.X, FMath::Max(1, Output.Y / 2));
}

bool FOmniCaptureSettings::IsStereo() const
{
    return Mode == EOmniCaptureMode::Stereo;
}

bool FOmniCaptureSettings::IsVR180() const
{
    return Coverage == EOmniCaptureCoverage::HalfSphere;
}

FString FOmniCaptureSettings::GetStereoModeMetadataTag() const
{
    if (!IsStereo())
    {
        return TEXT("mono");
    }

    return StereoLayout == EOmniCaptureStereoLayout::TopBottom
        ? TEXT("top-bottom")
        : TEXT("left-right");
}

int32 FOmniCaptureSettings::GetEncoderAlignmentRequirement() const
{
    if (OutputFormat == EOmniOutputFormat::NVENCHardware)
    {
        switch (NVENCColorFormat)
        {
        case EOmniCaptureColorFormat::P010:
            return 4;
        case EOmniCaptureColorFormat::BGRA:
            return 1;
        case EOmniCaptureColorFormat::NV12:
        default:
            return 2;
        }
    }

    return 2;
}

float FOmniCaptureSettings::GetHorizontalFOVDegrees() const
{
    return IsVR180() ? 180.0f : 360.0f;
}

float FOmniCaptureSettings::GetVerticalFOVDegrees() const
{
    return 180.0f;
}

float FOmniCaptureSettings::GetLongitudeSpanRadians() const
{
    return FMath::DegreesToRadians(GetHorizontalFOVDegrees() * 0.5f);
}

float FOmniCaptureSettings::GetLatitudeSpanRadians() const
{
    return FMath::DegreesToRadians(GetVerticalFOVDegrees() * 0.5f);
}
