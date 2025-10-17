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

    FORCEINLINE FIntPoint AlignPoint(const FIntPoint& Value, int32 Alignment)
    {
        return FIntPoint(AlignDimension(Value.X, Alignment), AlignDimension(Value.Y, Alignment));
    }
}

FIntPoint FOmniCaptureSettings::GetEquirectResolution() const
{
    if (IsPlanar())
    {
        return GetPlanarResolution();
    }

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

FIntPoint FOmniCaptureSettings::GetPlanarResolution() const
{
    FIntPoint Base = PlanarResolution;
    Base.X = FMath::Max(1, Base.X);
    Base.Y = FMath::Max(1, Base.Y);

    const int32 Scale = FMath::Max(1, PlanarIntegerScale);
    Base.X *= Scale;
    Base.Y *= Scale;

    const int32 Alignment = GetEncoderAlignmentRequirement();
    Base = AlignPoint(Base, Alignment);

    Base.X = FMath::Max(2, Base.X);
    Base.Y = FMath::Max(2, Base.Y);

    return Base;
}

FIntPoint FOmniCaptureSettings::GetOutputResolution() const
{
    return IsPlanar() ? GetPlanarResolution() : GetEquirectResolution();
}

FIntPoint FOmniCaptureSettings::GetPerEyeOutputResolution() const
{
    if (IsPlanar())
    {
        return GetPlanarResolution();
    }

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

bool FOmniCaptureSettings::IsPlanar() const
{
    return Projection == EOmniCaptureProjection::Planar2D;
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
    int32 Alignment = 2;

    if (OutputFormat == EOmniOutputFormat::NVENCHardware)
    {
        Alignment = FMath::Lcm(Alignment, 64);

        switch (NVENCColorFormat)
        {
        case EOmniCaptureColorFormat::P010:
            Alignment = FMath::Lcm(Alignment, 4);
            break;
        case EOmniCaptureColorFormat::BGRA:
            Alignment = FMath::Max(1, Alignment);
            break;
        default:
            break;
        }
    }

    return FMath::Max(1, Alignment);
}

float FOmniCaptureSettings::GetHorizontalFOVDegrees() const
{
    if (IsPlanar())
    {
        return 90.0f;
    }
    return IsVR180() ? 180.0f : 360.0f;
}

float FOmniCaptureSettings::GetVerticalFOVDegrees() const
{
    if (IsPlanar())
    {
        return 90.0f;
    }
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

FString FOmniCaptureSettings::GetImageFileExtension() const
{
    switch (ImageFormat)
    {
    case EOmniCaptureImageFormat::JPG:
        return TEXT(".jpg");
    case EOmniCaptureImageFormat::EXR:
        return TEXT(".exr");
    case EOmniCaptureImageFormat::PNG:
    default:
        return TEXT(".png");
    }
}
