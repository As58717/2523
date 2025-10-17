#pragma once
#include "ImageWriteTypes.h"

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"
#include "OmniCaptureRigActor.h"

struct FOmniCaptureEquirectResult
{
    TUniquePtr<FImagePixelData> PixelData;
    TArray<FColor> PreviewPixels;
    FIntPoint Size = FIntPoint::ZeroValue;
    bool bIsLinear = false;
    bool bUsedCPUFallback = false;
    TRefCountPtr<IPooledRenderTarget> OutputTarget;
    TRefCountPtr<IPooledRenderTarget> GPUSource;
    FTextureRHIRef Texture;
    FGPUFenceRHIRef ReadyFence;
    TArray<TRefCountPtr<IPooledRenderTarget>> EncoderPlanes;
};

class OMNICAPTURE_API FOmniCaptureEquirectConverter
{
public:
    static FOmniCaptureEquirectResult ConvertToEquirectangular(const FOmniCaptureSettings& Settings, const FOmniEyeCapture& LeftEye, const FOmniEyeCapture& RightEye);
    static FOmniCaptureEquirectResult ConvertToPlanar(const FOmniCaptureSettings& Settings, const FOmniEyeCapture& SourceEye);
};

