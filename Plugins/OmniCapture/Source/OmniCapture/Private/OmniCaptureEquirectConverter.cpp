#include "OmniCaptureEquirectConverter.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTarget.h"
#if __has_include("Rendering/TextureRenderTargetResource.h")
#include "Rendering/TextureRenderTargetResource.h"
#elif __has_include("Engine/TextureRenderTargetResource.h")
#include "Engine/TextureRenderTargetResource.h"
#elif __has_include("TextureRenderTargetResource.h")
#include "TextureRenderTargetResource.h"
#else
class FTextureRenderTargetResource;
#endif
#include "OmniCaptureTypes.h"

#include "GlobalShader.h"
#include "PixelShaderUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "RHIStaticStates.h"
#include "RenderTargetPool.h"
#if __has_include("RenderGraphUtils/Public/ComputeShaderUtils.h")
#include "RenderGraphUtils/Public/ComputeShaderUtils.h"
#elif __has_include("RenderGraphUtils/ComputeShaderUtils.h")
#include "RenderGraphUtils/ComputeShaderUtils.h"
#elif __has_include("ComputeShaderUtils.h")
#include "ComputeShaderUtils.h"
#endif
#include "RHICommandList.h"
#include "HAL/PlatformProcess.h"

namespace
{
    struct FCPUFaceData
    {
        int32 Resolution = 0;
        TArray<FFloat16Color> Pixels;

        bool IsValid() const
        {
            return Resolution > 0 && Pixels.Num() == Resolution * Resolution;
        }
    };

    struct FCPUCubemap
    {
        FCPUFaceData Faces[6];

        bool IsValid() const
        {
            for (int32 Index = 0; Index < 6; ++Index)
            {
                if (!Faces[Index].IsValid())
                {
                    return false;
                }
            }

            return true;
        }
    };

    class FOmniEquirectCS final : public FGlobalShader
    {
    public:
        DECLARE_GLOBAL_SHADER(FOmniEquirectCS);
        SHADER_USE_PARAMETER_STRUCT(FOmniEquirectCS, FGlobalShader);

        BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
            SHADER_PARAMETER(FVector2f, OutputResolution)
            SHADER_PARAMETER(int32, FaceResolution)
            SHADER_PARAMETER(int32, bStereo)
            SHADER_PARAMETER(float, SeamStrength)
            SHADER_PARAMETER(float, PolarStrength)
            SHADER_PARAMETER(int32, StereoLayout)
            SHADER_PARAMETER(int32, bHalfSphere)
            SHADER_PARAMETER(float, Padding)
            SHADER_PARAMETER(float, LongitudeSpan)
            SHADER_PARAMETER(float, LatitudeSpan)
            SHADER_PARAMETER_SAMPLER(SamplerState, FaceSampler)
            SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float4>, LeftFaces)
            SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float4>, RightFaces)
            SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
        END_SHADER_PARAMETER_STRUCT()

        static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
        {
            return true;
        }
    };

    IMPLEMENT_GLOBAL_SHADER(FOmniEquirectCS, "/Plugin/OmniCapture/Private/OmniEquirectCS.usf", "MainCS", SF_Compute);

    class FOmniConvertToYUVLumaCS final : public FGlobalShader
    {
    public:
        DECLARE_GLOBAL_SHADER(FOmniConvertToYUVLumaCS);
        SHADER_USE_PARAMETER_STRUCT(FOmniConvertToYUVLumaCS, FGlobalShader);

        BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
            SHADER_PARAMETER(FVector2f, OutputSize)
            SHADER_PARAMETER(FVector2f, ChromaSize)
            SHADER_PARAMETER(int32, Format)
            SHADER_PARAMETER(int32, ColorSpace)
            SHADER_PARAMETER(int32, bLinearInput)
            SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SourceTexture)
            SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
            SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, LumaOutput)
        END_SHADER_PARAMETER_STRUCT()

        static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
        {
            return true;
        }
    };

    IMPLEMENT_GLOBAL_SHADER(FOmniConvertToYUVLumaCS, "/Plugin/OmniCapture/Private/OmniColorConvertCS.usf", "ConvertLuma", SF_Compute);

    class FOmniConvertToYUVChromaCS final : public FGlobalShader
    {
    public:
        DECLARE_GLOBAL_SHADER(FOmniConvertToYUVChromaCS);
        SHADER_USE_PARAMETER_STRUCT(FOmniConvertToYUVChromaCS, FGlobalShader);

        BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
            SHADER_PARAMETER(FVector2f, OutputSize)
            SHADER_PARAMETER(FVector2f, ChromaSize)
            SHADER_PARAMETER(int32, Format)
            SHADER_PARAMETER(int32, ColorSpace)
            SHADER_PARAMETER(int32, bLinearInput)
            SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SourceTexture)
            SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
            SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint2>, ChromaOutput)
        END_SHADER_PARAMETER_STRUCT()

        static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
        {
            return true;
        }
    };

    IMPLEMENT_GLOBAL_SHADER(FOmniConvertToYUVChromaCS, "/Plugin/OmniCapture/Private/OmniColorConvertCS.usf", "ConvertChroma", SF_Compute);

    class FOmniConvertToBGRACS final : public FGlobalShader
    {
    public:
        DECLARE_GLOBAL_SHADER(FOmniConvertToBGRACS);
        SHADER_USE_PARAMETER_STRUCT(FOmniConvertToBGRACS, FGlobalShader);

        BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
            SHADER_PARAMETER(FVector2f, OutputSize)
            SHADER_PARAMETER(FVector2f, ChromaSize)
            SHADER_PARAMETER(int32, Format)
            SHADER_PARAMETER(int32, ColorSpace)
            SHADER_PARAMETER(int32, bLinearInput)
            SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SourceTexture)
            SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
            SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, OutputTexture)
        END_SHADER_PARAMETER_STRUCT()

        static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
        {
            return true;
        }
    };

    IMPLEMENT_GLOBAL_SHADER(FOmniConvertToBGRACS, "/Plugin/OmniCapture/Private/OmniColorConvertCS.usf", "ConvertBGRA", SF_Compute);

    bool ReadFaceData(UTextureRenderTarget2D* RenderTarget, FCPUFaceData& OutFace)
    {
        if (!RenderTarget)
        {
            return false;
        }

        FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
        if (!Resource)
        {
            return false;
        }

        const int32 SizeX = RenderTarget->SizeX;
        const int32 SizeY = RenderTarget->SizeY;
        if (SizeX <= 0 || SizeY <= 0 || SizeX != SizeY)
        {
            return false;
        }

        OutFace.Pixels.Reset();
        FReadSurfaceDataFlags Flags(RCM_MinMax);
        Flags.SetLinearToGamma(false);
        if (!Resource->ReadFloat16Pixels(OutFace.Pixels, Flags, FIntRect()))
        {
            return false;
        }

        OutFace.Resolution = SizeX;
        return OutFace.IsValid();
    }

    bool BuildCPUCubemap(const FOmniEyeCapture& Eye, FCPUCubemap& OutCubemap)
    {
        for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
        {
            if (!ReadFaceData(Eye.Faces[FaceIndex].RenderTarget, OutCubemap.Faces[FaceIndex]))
            {
                return false;
            }
        }

        return OutCubemap.IsValid();
    }

    FVector DirectionFromEquirectPixelCPU(const FIntPoint& Pixel, const FIntPoint& EyeResolution, double LongitudeSpan, double LatitudeSpan, float& OutLatitude)
    {
        const FVector2D UV((static_cast<double>(Pixel.X) + 0.5) / EyeResolution.X, (static_cast<double>(Pixel.Y) + 0.5) / EyeResolution.Y);
        const double Longitude = (UV.X * 2.0 - 1.0) * LongitudeSpan;
        const double Latitude = (0.5 - UV.Y) * LatitudeSpan * 2.0;
        OutLatitude = static_cast<float>(Latitude);

        const double CosLat = FMath::Cos(Latitude);
        const double SinLat = FMath::Sin(Latitude);
        const double CosLon = FMath::Cos(Longitude);
        const double SinLon = FMath::Sin(Longitude);

        FVector Direction;
        Direction.X = CosLat * CosLon;
        Direction.Y = SinLat;
        Direction.Z = CosLat * SinLon;
        return Direction.GetSafeNormal();
    }

    void DirectionToFaceUVCPU(const FVector& Direction, uint32& OutFaceIndex, FVector2D& OutUV, int32 FaceResolution, float SeamStrength)
    {
        const FVector AbsDir = Direction.GetAbs();

        if (AbsDir.X >= AbsDir.Y && AbsDir.X >= AbsDir.Z)
        {
            if (Direction.X > 0.0f)
            {
                OutFaceIndex = 0;
                OutUV = FVector2D(-Direction.Z, Direction.Y) / AbsDir.X;
            }
            else
            {
                OutFaceIndex = 1;
                OutUV = FVector2D(Direction.Z, Direction.Y) / AbsDir.X;
            }
        }
        else if (AbsDir.Y >= AbsDir.X && AbsDir.Y >= AbsDir.Z)
        {
            if (Direction.Y > 0.0f)
            {
                OutFaceIndex = 2;
                OutUV = FVector2D(Direction.X, -Direction.Z) / AbsDir.Y;
            }
            else
            {
                OutFaceIndex = 3;
                OutUV = FVector2D(Direction.X, Direction.Z) / AbsDir.Y;
            }
        }
        else
        {
            if (Direction.Z > 0.0f)
            {
                OutFaceIndex = 4;
                OutUV = FVector2D(Direction.X, Direction.Y) / AbsDir.Z;
            }
            else
            {
                OutFaceIndex = 5;
                OutUV = FVector2D(-Direction.X, Direction.Y) / AbsDir.Z;
            }
        }

        OutUV = (OutUV + FVector2D(1.0, 1.0)) * 0.5f;

        const double Resolution = static_cast<double>(FMath::Max(1, FaceResolution));
        const double Scale = FMath::Lerp(1.0, (Resolution - 1.0) / Resolution, SeamStrength);
        const double Bias = (0.5 / Resolution) * SeamStrength;
        OutUV = FVector2D(OutUV.X * Scale + Bias, OutUV.Y * Scale + Bias);
        OutUV.X = FMath::Clamp(OutUV.X, 0.0f, 1.0f);
        OutUV.Y = FMath::Clamp(OutUV.Y, 0.0f, 1.0f);
    }

    FLinearColor SampleCubemapCPU(const FCPUCubemap& Cubemap, const FVector& Direction, int32 FaceResolution, float SeamStrength)
    {
        uint32 FaceIndex = 0;
        FVector2D FaceUV = FVector2D::ZeroVector;
        DirectionToFaceUVCPU(Direction, FaceIndex, FaceUV, FaceResolution, SeamStrength);

        const FCPUFaceData& Face = Cubemap.Faces[FaceIndex];
        const int32 SampleX = FMath::Clamp(static_cast<int32>(FaceUV.X * (Face.Resolution - 1)), 0, Face.Resolution - 1);
        const int32 SampleY = FMath::Clamp(static_cast<int32>(FaceUV.Y * (Face.Resolution - 1)), 0, Face.Resolution - 1);
        const int32 SampleIndex = SampleY * Face.Resolution + SampleX;

        return Face.Pixels.IsValidIndex(SampleIndex)
            ? FLinearColor(Face.Pixels[SampleIndex])
            : FLinearColor::Black;
    }

    void ApplyPolarMitigation(float PolarStrength, float Latitude, FVector& Direction)
    {
        if (PolarStrength <= 0.0f)
        {
            return;
        }

        double PoleFactor = FMath::Clamp(FMath::Abs(Latitude) / (PI * 0.5), 0.0, 1.0);
        PoleFactor = FMath::Pow(PoleFactor, 4.0);
        const double Blend = PoleFactor * PolarStrength;
        if (Blend <= 0.0)
        {
            return;
        }

        const FVector PoleVector(0.0f, Latitude >= 0.0f ? 1.0f : -1.0f, 0.0f);
        Direction = FVector(FMath::Lerp(Direction.X, PoleVector.X, Blend),
            FMath::Lerp(Direction.Y, PoleVector.Y, Blend),
            FMath::Lerp(Direction.Z, PoleVector.Z, Blend));
        Direction.Normalize();
    }

    void AddYUVConversionPasses(
        FRDGBuilder& GraphBuilder,
        const FOmniCaptureSettings& Settings,
        bool bSourceLinear,
        int32 OutputWidth,
        int32 OutputHeight,
        FRDGTextureRef SourceTexture,
        FRDGTextureRef& OutLuma,
        FRDGTextureRef& OutChroma)
    {
        if (!SourceTexture)
        {
            return;
        }

        const bool bNV12 = Settings.NVENCColorFormat == EOmniCaptureColorFormat::NV12;
        const bool bP010 = Settings.NVENCColorFormat == EOmniCaptureColorFormat::P010;
        if (!bNV12 && !bP010)
        {
            return;
        }

        const EPixelFormat LumaFormat = bNV12 ? PF_R8 : PF_R16_UINT;
        const EPixelFormat ChromaFormat = bNV12 ? PF_R8G8 : PF_R16G16_UINT;

        FRDGTextureDesc LumaDesc = FRDGTextureDesc::Create2D(FIntPoint(OutputWidth, OutputHeight), LumaFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
        FRDGTextureDesc ChromaDesc = FRDGTextureDesc::Create2D(FIntPoint(FMath::Max(OutputWidth / 2, 1), FMath::Max(OutputHeight / 2, 1)), ChromaFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);

        OutLuma = GraphBuilder.CreateTexture(LumaDesc, TEXT("OmniNVENC_Luma"));
        OutChroma = GraphBuilder.CreateTexture(ChromaDesc, TEXT("OmniNVENC_Chroma"));

        FOmniConvertToYUVLumaCS::FParameters* LumaParameters = GraphBuilder.AllocParameters<FOmniConvertToYUVLumaCS::FParameters>();
        LumaParameters->OutputSize = FVector2f(OutputWidth, OutputHeight);
        LumaParameters->ChromaSize = FVector2f(ChromaDesc.Extent.X, ChromaDesc.Extent.Y);
        LumaParameters->Format = bNV12 ? 0 : 1;
        LumaParameters->ColorSpace = static_cast<int32>(Settings.ColorSpace);
        LumaParameters->bLinearInput = bSourceLinear ? 1 : 0;
        LumaParameters->SourceTexture = SourceTexture;
        LumaParameters->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        LumaParameters->LumaOutput = GraphBuilder.CreateUAV(OutLuma);

        TShaderMapRef<FOmniConvertToYUVLumaCS> LumaShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        const FIntVector LumaGroupCount(
            FMath::DivideAndRoundUp(OutputWidth, 8),
            FMath::DivideAndRoundUp(OutputHeight, 8),
            1);

        FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("OmniCapture::YUVLuma"), LumaShader, LumaParameters, LumaGroupCount);

        FOmniConvertToYUVChromaCS::FParameters* ChromaParameters = GraphBuilder.AllocParameters<FOmniConvertToYUVChromaCS::FParameters>();
        ChromaParameters->OutputSize = FVector2f(OutputWidth, OutputHeight);
        ChromaParameters->ChromaSize = FVector2f(ChromaDesc.Extent.X, ChromaDesc.Extent.Y);
        ChromaParameters->Format = bNV12 ? 0 : 1;
        ChromaParameters->ColorSpace = static_cast<int32>(Settings.ColorSpace);
        ChromaParameters->bLinearInput = bSourceLinear ? 1 : 0;
        ChromaParameters->SourceTexture = SourceTexture;
        ChromaParameters->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        ChromaParameters->ChromaOutput = GraphBuilder.CreateUAV(OutChroma);

        TShaderMapRef<FOmniConvertToYUVChromaCS> ChromaShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        const FIntVector ChromaGroupCount(
            FMath::DivideAndRoundUp(ChromaDesc.Extent.X, 8),
            FMath::DivideAndRoundUp(ChromaDesc.Extent.Y, 8),
            1);

        FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("OmniCapture::YUVChroma"), ChromaShader, ChromaParameters, ChromaGroupCount);
    }

    FRDGTextureRef AddBGRAPackingPass(
        FRDGBuilder& GraphBuilder,
        const FOmniCaptureSettings& Settings,
        bool bSourceLinear,
        int32 OutputWidth,
        int32 OutputHeight,
        FRDGTextureRef SourceTexture)
    {
        if (!SourceTexture)
        {
            return nullptr;
        }

        FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(FIntPoint(OutputWidth, OutputHeight), PF_B8G8R8A8, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_UAV);
        FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(Desc, TEXT("OmniNVENC_BGRA"));

        FOmniConvertToBGRACS::FParameters* Parameters = GraphBuilder.AllocParameters<FOmniConvertToBGRACS::FParameters>();
        Parameters->OutputSize = FVector2f(OutputWidth, OutputHeight);
        Parameters->ChromaSize = FVector2f(OutputWidth * 0.5f, OutputHeight * 0.5f);
        Parameters->Format = 0;
        Parameters->ColorSpace = static_cast<int32>(Settings.ColorSpace);
        Parameters->bLinearInput = bSourceLinear ? 1 : 0;
        Parameters->SourceTexture = SourceTexture;
        Parameters->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        Parameters->OutputTexture = GraphBuilder.CreateUAV(OutputTexture);

        TShaderMapRef<FOmniConvertToBGRACS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        const FIntVector GroupCount(
            FMath::DivideAndRoundUp(OutputWidth, 8),
            FMath::DivideAndRoundUp(OutputHeight, 8),
            1);

        FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("OmniCapture::BGRAPack"), Shader, Parameters, GroupCount);

        return OutputTexture;
    }

    FRDGTextureRef BuildFaceArray(FRDGBuilder& GraphBuilder, const TArray<FTextureRHIRef, TInlineAllocator<6>>& Faces, int32 FaceResolution, const TCHAR* DebugName)
    {
        if (Faces.Num() == 0)
        {
            return nullptr;
        }

        FRDGTextureDesc ArrayDesc = FRDGTextureDesc::Create2DArray(FIntPoint(FaceResolution, FaceResolution), PF_FloatRGBA, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_UAV, Faces.Num());
        FRDGTextureRef ArrayTexture = GraphBuilder.CreateTexture(ArrayDesc, DebugName);

        for (int32 Index = 0; Index < Faces.Num(); ++Index)
        {
            if (!Faces[Index].IsValid())
            {
                continue;
            }

            FRDGTextureRef SourceTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Faces[Index], *FString::Printf(TEXT("%sFace%d"), DebugName, Index)));
            FRHICopyTextureInfo CopyInfo;
            CopyInfo.SourceSliceIndex = 0;
            CopyInfo.DestSliceIndex = Index;
            CopyInfo.NumSlices = 1;

            AddCopyTexturePass(GraphBuilder, SourceTexture, ArrayTexture, CopyInfo);
        }

        return ArrayTexture;
    }

    void ConvertOnRenderThread(const FOmniCaptureSettings Settings, const TArray<FTextureRHIRef, TInlineAllocator<6>> LeftFaces, const TArray<FTextureRHIRef, TInlineAllocator<6>> RightFaces, FOmniCaptureEquirectResult& OutResult)
    {
        const int32 FaceResolution = Settings.Resolution;
        const bool bStereo = Settings.Mode == EOmniCaptureMode::Stereo;
        const bool bSideBySide = bStereo && Settings.StereoLayout == EOmniCaptureStereoLayout::SideBySide;
        const FIntPoint OutputSize = Settings.GetEquirectResolution();
        const int32 OutputWidth = OutputSize.X;
        const int32 OutputHeight = OutputSize.Y;
        const bool bUseLinear = Settings.Gamma == EOmniCaptureGamma::Linear;
        const float LongitudeSpan = Settings.GetLongitudeSpanRadians();
        const float LatitudeSpan = Settings.GetLatitudeSpanRadians();
        const bool bHalfSphere = Settings.IsVR180();

        FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
        FRDGBuilder GraphBuilder(RHICmdList);

        FRDGTextureRef LeftArray = BuildFaceArray(GraphBuilder, LeftFaces, FaceResolution, TEXT("OmniLeftFaces"));
        FRDGTextureRef RightArray = bStereo ? BuildFaceArray(GraphBuilder, RightFaces, FaceResolution, TEXT("OmniRightFaces")) : LeftArray;

        if (!LeftArray)
        {
            GraphBuilder.Execute();
            return;
        }

        FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(FIntPoint(OutputWidth, OutputHeight), PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
        FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("OmniEquirectOutput"));

        FOmniEquirectCS::FParameters* Parameters = GraphBuilder.AllocParameters<FOmniEquirectCS::FParameters>();
        Parameters->OutputResolution = FVector2f(OutputWidth, OutputHeight);
        Parameters->FaceResolution = FaceResolution;
        Parameters->bStereo = bStereo ? 1 : 0;
        Parameters->SeamStrength = Settings.SeamBlend;
        Parameters->PolarStrength = Settings.PolarDampening;
        Parameters->StereoLayout = Settings.StereoLayout == EOmniCaptureStereoLayout::TopBottom ? 0 : 1;
        Parameters->Padding = 0.0f;
        Parameters->LongitudeSpan = LongitudeSpan;
        Parameters->LatitudeSpan = LatitudeSpan;
        Parameters->bHalfSphere = bHalfSphere ? 1 : 0;
        Parameters->LeftFaces = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(LeftArray));
        Parameters->RightFaces = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(RightArray));
        Parameters->FaceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
        Parameters->OutputTexture = GraphBuilder.CreateUAV(OutputTexture);

        TShaderMapRef<FOmniEquirectCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        const FIntVector GroupCount(
            FMath::DivideAndRoundUp(OutputWidth, 8),
            FMath::DivideAndRoundUp(OutputHeight, 8),
            1);

        FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("OmniCapture::Equirect"), ComputeShader, Parameters, GroupCount);

        FRDGTextureRef LumaTexture = nullptr;
        FRDGTextureRef ChromaTexture = nullptr;
        FRDGTextureRef BGRATexture = nullptr;
        if (Settings.OutputFormat == EOmniOutputFormat::NVENCHardware)
        {
            if (Settings.NVENCColorFormat == EOmniCaptureColorFormat::BGRA)
            {
                BGRATexture = AddBGRAPackingPass(GraphBuilder, Settings, bUseLinear, OutputWidth, OutputHeight, OutputTexture);
            }
            else
            {
                AddYUVConversionPasses(GraphBuilder, Settings, bUseLinear, OutputWidth, OutputHeight, OutputTexture, LumaTexture, ChromaTexture);
            }
        }

        TRefCountPtr<IPooledRenderTarget> ExtractedOutput;
        TRefCountPtr<IPooledRenderTarget> ExtractedLuma;
        TRefCountPtr<IPooledRenderTarget> ExtractedChroma;
        TRefCountPtr<IPooledRenderTarget> ExtractedBGRA;
        GraphBuilder.QueueTextureExtraction(OutputTexture, &ExtractedOutput);
        if (LumaTexture)
        {
            GraphBuilder.QueueTextureExtraction(LumaTexture, &ExtractedLuma);
        }
        if (ChromaTexture)
        {
            GraphBuilder.QueueTextureExtraction(ChromaTexture, &ExtractedChroma);
        }
        if (BGRATexture)
        {
            GraphBuilder.QueueTextureExtraction(BGRATexture, &ExtractedBGRA);
        }
        GraphBuilder.Execute();

        if (!ExtractedOutput.IsValid())
        {
            return;
        }

        OutResult.bUsedCPUFallback = false;
        OutResult.OutputTarget = ExtractedOutput;
        if (FRHITexture* OutputRHI = ExtractedOutput->GetRHI())
        {
            OutResult.Texture = OutputRHI;
        }
        OutResult.Size = FIntPoint(OutputWidth, OutputHeight);
        OutResult.bIsLinear = bUseLinear;

        if (ExtractedLuma.IsValid())
        {
            OutResult.EncoderPlanes.Add(ExtractedLuma);
        }
        if (ExtractedChroma.IsValid())
        {
            OutResult.EncoderPlanes.Add(ExtractedChroma);
        }
        if (ExtractedBGRA.IsValid())
        {
            OutResult.EncoderPlanes.Add(ExtractedBGRA);

            if (FRHITexture* BGRATextureRHI = ExtractedBGRA->GetRHI())
            {
                OutResult.Texture = BGRATextureRHI;
            }
        }

        if (OutResult.Texture.IsValid())
        {
            FGPUFenceRHIRef Fence = RHICreateGPUFence(TEXT("OmniEquirectFence"));
            if (Fence.IsValid())
            {
                RHICmdList.WriteGPUFence(Fence);
                OutResult.ReadyFence = Fence;
            }
        }

        FRHITexture* OutputTextureRHI = ExtractedOutput->GetRHI();
        if (!OutputTextureRHI)
        {
            return;
        }

        FRHIGPUTextureReadback Readback(TEXT("OmniEquirectReadback"));
        Readback.EnqueueCopy(RHICmdList, OutputTextureRHI, FResolveRect(0, 0, OutputWidth, OutputHeight));
        RHICmdList.SubmitCommandsAndFlushGPU();

        while (!Readback.IsReady())
        {
            FPlatformProcess::SleepNoStats(0.001f);
        }

        const uint32 PixelCount = OutputWidth * OutputHeight;
        const uint32 BytesPerPixel = sizeof(FFloat16Color);
        int32 RowPitchInPixels = 0;
        const uint8* RawData = static_cast<const uint8*>(Readback.Lock(RowPitchInPixels));

        if (RawData)
        {
            const uint32 RowPitch = RowPitchInPixels > 0 ? static_cast<uint32>(RowPitchInPixels) : static_cast<uint32>(OutputWidth);
            if (bUseLinear)
            {
                TUniquePtr<TImagePixelData<FFloat16Color>> PixelData = MakeUnique<TImagePixelData<FFloat16Color>>(FIntPoint(OutputWidth, OutputHeight));
                PixelData->Pixels.SetNum(PixelCount);

                FFloat16Color* DestData = PixelData->Pixels.GetData();
                const FFloat16Color* SourcePixels = reinterpret_cast<const FFloat16Color*>(RawData);
                for (int32 Row = 0; Row < OutputHeight; ++Row)
                {
                    const FFloat16Color* SourceRow = SourcePixels + RowPitch * Row;
                    FMemory::Memcpy(DestData + Row * OutputWidth, SourceRow, OutputWidth * BytesPerPixel);
                }

                OutResult.PixelData = MoveTemp(PixelData);

                OutResult.PreviewPixels.SetNum(PixelCount);
                const TImagePixelData<FFloat16Color>* FloatData = static_cast<const TImagePixelData<FFloat16Color>*>(OutResult.PixelData.Get());
                if (FloatData)
                {
                    for (uint32 Index = 0; Index < PixelCount; ++Index)
                    {
                        const FFloat16Color& Source = FloatData->Pixels[Index];
                        const FLinearColor Linear(Source.R.GetFloat(), Source.G.GetFloat(), Source.B.GetFloat(), Source.A.GetFloat());
                        OutResult.PreviewPixels[Index] = Linear.ToFColor(true);
                    }
                }
            }
            else
            {
                TUniquePtr<TImagePixelData<FColor>> PixelData = MakeUnique<TImagePixelData<FColor>>(FIntPoint(OutputWidth, OutputHeight));
                PixelData->Pixels.SetNum(PixelCount);
                OutResult.PreviewPixels.SetNum(PixelCount);

                const FFloat16Color* SourcePixels = reinterpret_cast<const FFloat16Color*>(RawData);
                for (int32 Row = 0; Row < OutputHeight; ++Row)
                {
                    const FFloat16Color* SourceRow = SourcePixels + RowPitch * Row;
                    FColor* DestRow = PixelData->Pixels.GetData() + Row * OutputWidth;
                    for (int32 Column = 0; Column < OutputWidth; ++Column)
                    {
                        const FFloat16Color& Pixel = SourceRow[Column];
                        const FLinearColor Linear(Pixel.R.GetFloat(), Pixel.G.GetFloat(), Pixel.B.GetFloat(), Pixel.A.GetFloat());
                        const FColor SRGB = Linear.ToFColor(true);
                        DestRow[Column] = SRGB;
                        OutResult.PreviewPixels[Row * OutputWidth + Column] = SRGB;
                    }
                }

                OutResult.PixelData = MoveTemp(PixelData);
            }
        }

        Readback.Unlock();
    }
}

    void ConvertPlanarOnRenderThread(const FOmniCaptureSettings Settings, FTextureRHIRef SourceTexture, const FIntPoint OutputSize, bool bSourceLinear, FOmniCaptureEquirectResult& OutResult)
    {
        if (!SourceTexture.IsValid())
        {
            return;
        }

        const int32 OutputWidth = OutputSize.X;
        const int32 OutputHeight = OutputSize.Y;

        FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
        FRDGBuilder GraphBuilder(RHICmdList);

        FRDGTextureRef Source = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(SourceTexture, TEXT("OmniPlanarSource")));

        FRDGTextureRef LumaTexture = nullptr;
        FRDGTextureRef ChromaTexture = nullptr;
        FRDGTextureRef BGRATexture = nullptr;

        if (Settings.OutputFormat == EOmniOutputFormat::NVENCHardware)
        {
            if (Settings.NVENCColorFormat == EOmniCaptureColorFormat::BGRA)
            {
                BGRATexture = AddBGRAPackingPass(GraphBuilder, Settings, bSourceLinear, OutputWidth, OutputHeight, Source);
            }
            else
            {
                AddYUVConversionPasses(GraphBuilder, Settings, bSourceLinear, OutputWidth, OutputHeight, Source, LumaTexture, ChromaTexture);
            }
        }

        TRefCountPtr<IPooledRenderTarget> ExtractedSource;
        TRefCountPtr<IPooledRenderTarget> ExtractedLuma;
        TRefCountPtr<IPooledRenderTarget> ExtractedChroma;
        TRefCountPtr<IPooledRenderTarget> ExtractedBGRA;

        GraphBuilder.QueueTextureExtraction(Source, &ExtractedSource);
        if (LumaTexture)
        {
            GraphBuilder.QueueTextureExtraction(LumaTexture, &ExtractedLuma);
        }
        if (ChromaTexture)
        {
            GraphBuilder.QueueTextureExtraction(ChromaTexture, &ExtractedChroma);
        }
        if (BGRATexture)
        {
            GraphBuilder.QueueTextureExtraction(BGRATexture, &ExtractedBGRA);
        }

        GraphBuilder.Execute();

        OutResult.OutputTarget = ExtractedSource;
        OutResult.GPUSource = ExtractedSource;
        OutResult.Texture = SourceTexture;
        OutResult.Size = OutputSize;
        OutResult.bIsLinear = bSourceLinear;
        OutResult.bUsedCPUFallback = false;
        OutResult.EncoderPlanes.Reset();
        OutResult.ReadyFence.SafeRelease();

        if (Settings.OutputFormat == EOmniOutputFormat::NVENCHardware)
        {
            if (Settings.NVENCColorFormat == EOmniCaptureColorFormat::BGRA)
            {
                if (ExtractedBGRA.IsValid())
                {
                    OutResult.EncoderPlanes.Add(ExtractedBGRA);
                    if (FRHITexture* BGRAResource = ExtractedBGRA->GetRHI())
                    {
                        OutResult.Texture = BGRAResource;
                    }
                }
            }
            else
            {
                if (ExtractedLuma.IsValid())
                {
                    OutResult.EncoderPlanes.Add(ExtractedLuma);
                }
                if (ExtractedChroma.IsValid())
                {
                    OutResult.EncoderPlanes.Add(ExtractedChroma);
                }
            }

            if (OutResult.Texture.IsValid())
            {
                FGPUFenceRHIRef Fence = RHICreateGPUFence(TEXT("OmniPlanarFence"));
                if (Fence.IsValid())
                {
                    RHICmdList.WriteGPUFence(Fence);
                    OutResult.ReadyFence = Fence;
                }
            }
        }
    }

namespace
{
    void ConvertOnCPU(const FOmniCaptureSettings& Settings, const FOmniEyeCapture& LeftEye, const FOmniEyeCapture& RightEye, FOmniCaptureEquirectResult& OutResult)
    {
        FCPUCubemap LeftCubemap;
        if (!BuildCPUCubemap(LeftEye, LeftCubemap))
        {
            return;
        }

        FCPUCubemap RightCubemap;
        if (Settings.Mode == EOmniCaptureMode::Stereo)
        {
            if (!BuildCPUCubemap(RightEye, RightCubemap))
            {
                return;
            }
        }

        const bool bStereo = Settings.Mode == EOmniCaptureMode::Stereo;
        const bool bSideBySide = bStereo && Settings.StereoLayout == EOmniCaptureStereoLayout::SideBySide;
        const int32 FaceResolution = LeftCubemap.Faces[0].Resolution;
        const FIntPoint OutputSize = Settings.GetEquirectResolution();
        const int32 OutputWidth = OutputSize.X;
        const int32 OutputHeight = OutputSize.Y;
        const double LongitudeSpan = Settings.GetLongitudeSpanRadians();
        const double LatitudeSpan = Settings.GetLatitudeSpanRadians();
        const bool bHalfSphere = Settings.IsVR180();

        OutResult.Size = FIntPoint(OutputWidth, OutputHeight);
        OutResult.bIsLinear = Settings.Gamma == EOmniCaptureGamma::Linear;
        OutResult.bUsedCPUFallback = true;
        OutResult.OutputTarget.SafeRelease();
        OutResult.Texture.SafeRelease();
        OutResult.ReadyFence.SafeRelease();
        OutResult.EncoderPlanes.Reset();

        const int32 PixelCount = OutputWidth * OutputHeight;
        OutResult.PreviewPixels.SetNum(PixelCount);

        auto ProcessPixel = [&](auto& PixelArray, auto ConvertColor)
        {
            for (int32 Y = 0; Y < OutputHeight; ++Y)
            {
                for (int32 X = 0; X < OutputWidth; ++X)
                {
                    const int32 Index = Y * OutputWidth + X;

                    FIntPoint EyePixel(X, Y);
                    FIntPoint EyeResolution(OutputWidth, OutputHeight);
                    bool bRightEye = false;

                    if (bStereo)
                    {
                        if (bSideBySide)
                        {
                            const int32 EyeWidth = OutputWidth / 2;
                            bRightEye = X >= EyeWidth;
                            EyePixel.X = X % EyeWidth;
                            EyeResolution = FIntPoint(EyeWidth, OutputHeight);
                        }
                        else
                        {
                            const int32 EyeHeight = OutputHeight / 2;
                            bRightEye = Y >= EyeHeight;
                            EyePixel.Y = Y % EyeHeight;
                            EyeResolution = FIntPoint(OutputWidth, EyeHeight);
                        }
                    }

                    float Latitude = 0.0f;
                    FVector Direction = DirectionFromEquirectPixelCPU(EyePixel, EyeResolution, LongitudeSpan, LatitudeSpan, Latitude);
                    ApplyPolarMitigation(Settings.PolarDampening, Latitude, Direction);

                    if (bHalfSphere && Direction.X < 0.0f)
                    {
                        PixelArray[Index] = ConvertColor(FLinearColor::Transparent);
                        OutResult.PreviewPixels[Index] = FColor::Transparent;
                        continue;
                    }

                    const FLinearColor LinearColor = SampleCubemapCPU(
                        (bStereo && bRightEye) ? RightCubemap : LeftCubemap,
                        Direction,
                        FaceResolution,
                        Settings.SeamBlend);

                    PixelArray[Index] = ConvertColor(LinearColor);
                    OutResult.PreviewPixels[Index] = LinearColor.ToFColor(true);
                }
            }
        };

        if (OutResult.bIsLinear)
        {
            TUniquePtr<TImagePixelData<FFloat16Color>> PixelData = MakeUnique<TImagePixelData<FFloat16Color>>(OutResult.Size);
            PixelData->Pixels.SetNum(PixelCount);
            ProcessPixel(PixelData->Pixels, [](const FLinearColor& Linear) { return FFloat16Color(Linear); });
            OutResult.PixelData = MoveTemp(PixelData);
        }
        else
        {
            TUniquePtr<TImagePixelData<FColor>> PixelData = MakeUnique<TImagePixelData<FColor>>(OutResult.Size);
            PixelData->Pixels.SetNum(PixelCount);
            ProcessPixel(PixelData->Pixels, [](const FLinearColor& Linear) { return Linear.ToFColor(true); });
            OutResult.PixelData = MoveTemp(PixelData);
        }
    }
}

FOmniCaptureEquirectResult FOmniCaptureEquirectConverter::ConvertToEquirectangular(const FOmniCaptureSettings& Settings, const FOmniEyeCapture& LeftEye, const FOmniEyeCapture& RightEye)
{
    FOmniCaptureEquirectResult Result;

    if (Settings.Resolution <= 0)
    {
        return Result;
    }

    TArray<FTextureRHIRef, TInlineAllocator<6>> LeftFaces;
    TArray<FTextureRHIRef, TInlineAllocator<6>> RightFaces;

    for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
    {
        if (UTextureRenderTarget2D* LeftTarget = LeftEye.Faces[FaceIndex].RenderTarget)
        {
            if (FTextureRenderTargetResource* Resource = LeftTarget->GameThread_GetRenderTargetResource())
            {
                if (FTextureRHIRef Texture = Resource->GetTextureRHI())
                {
                    LeftFaces.Add(Texture);
                }
            }
        }

        if (Settings.Mode == EOmniCaptureMode::Stereo)
        {
            if (UTextureRenderTarget2D* RightTarget = RightEye.Faces[FaceIndex].RenderTarget)
            {
                if (FTextureRenderTargetResource* Resource = RightTarget->GameThread_GetRenderTargetResource())
                {
                    if (FTextureRHIRef Texture = Resource->GetTextureRHI())
                    {
                        RightFaces.Add(Texture);
                    }
                }
            }
        }
    }

    if (LeftFaces.Num() != 6)
    {
        return Result;
    }

    if (Settings.Mode == EOmniCaptureMode::Stereo && RightFaces.Num() != 6)
    {
        return Result;
    }

    bool bSupportsCompute = GDynamicRHI != nullptr;
#if defined(GRHISupportsComputeShaders)
    bSupportsCompute = bSupportsCompute && GRHISupportsComputeShaders;
#elif defined(GSupportsComputeShaders)
    bSupportsCompute = bSupportsCompute && GSupportsComputeShaders;
#else
    bSupportsCompute = false;
#endif
    if (!bSupportsCompute)
    {
        ConvertOnCPU(Settings, LeftEye, RightEye, Result);
        return Result;
    }

    FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool();

    ENQUEUE_RENDER_COMMAND(OmniCaptureEquirect)([Settings, LeftFaces, RightFaces, &Result, CompletionEvent](FRHICommandListImmediate&)
    {
        ConvertOnRenderThread(Settings, LeftFaces, RightFaces, Result);
        CompletionEvent->Trigger();
    });

    CompletionEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);

    if (!Result.PixelData.IsValid() && (!Result.Texture.IsValid() || !Result.OutputTarget.IsValid()))
    {
        ConvertOnCPU(Settings, LeftEye, RightEye, Result);
    }

    return Result;
}

FOmniCaptureEquirectResult FOmniCaptureEquirectConverter::ConvertToPlanar(const FOmniCaptureSettings& Settings, const FOmniEyeCapture& SourceEye)
{
    FOmniCaptureEquirectResult Result;

    if (!Settings.IsPlanar())
    {
        return Result;
    }

    UTextureRenderTarget2D* RenderTarget = SourceEye.GetPrimaryRenderTarget();
    if (!RenderTarget)
    {
        return Result;
    }

    FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return Result;
    }

    const FIntPoint OutputSize(RenderTarget->SizeX, RenderTarget->SizeY);
    if (OutputSize.X <= 0 || OutputSize.Y <= 0)
    {
        return Result;
    }

    Result.Size = OutputSize;
    Result.bIsLinear = Settings.Gamma == EOmniCaptureGamma::Linear;
    Result.bUsedCPUFallback = false;
    Result.ReadyFence.SafeRelease();
    Result.EncoderPlanes.Reset();
    Result.OutputTarget.SafeRelease();
    Result.GPUSource.SafeRelease();

    const int32 PixelCount = OutputSize.X * OutputSize.Y;

    if (Result.bIsLinear)
    {
        TUniquePtr<TImagePixelData<FFloat16Color>> PixelData = MakeUnique<TImagePixelData<FFloat16Color>>(OutputSize);
        TArray<FFloat16Color> TempPixels;
        FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
        if (!Resource->ReadFloat16Pixels(TempPixels, ReadFlags, FIntRect()))
        {
            PixelData.Reset();
        }
        else
        {
            PixelData->Pixels = MoveTemp(TempPixels);
            Result.PixelData = MoveTemp(PixelData);
        }
    }
    else
    {
        TUniquePtr<TImagePixelData<FColor>> PixelData = MakeUnique<TImagePixelData<FColor>>(OutputSize);
        TArray<FColor> TempPixels;
        FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
        if (!Resource->ReadPixels(TempPixels, ReadFlags, FIntRect()))
        {
            PixelData.Reset();
        }
        else
        {
            PixelData->Pixels = MoveTemp(TempPixels);
            Result.PixelData = MoveTemp(PixelData);
        }
    }

    if (!Result.PixelData.IsValid())
    {
        Result.PreviewPixels.Reset();
        return Result;
    }

    Result.PreviewPixels.SetNum(PixelCount);
    if (Result.bIsLinear)
    {
        const TImagePixelData<FFloat16Color>* FloatData = static_cast<const TImagePixelData<FFloat16Color>*>(Result.PixelData.Get());
        if (FloatData)
        {
            for (int32 Index = 0; Index < PixelCount; ++Index)
            {
                Result.PreviewPixels[Index] = FLinearColor(FloatData->Pixels[Index]).ToFColor(true);
            }
        }
    }
    else
    {
        const TImagePixelData<FColor>* ColorData = static_cast<const TImagePixelData<FColor>*>(Result.PixelData.Get());
        if (ColorData)
        {
            Result.PreviewPixels = ColorData->Pixels;
        }
    }

    if (Result.PreviewPixels.Num() != PixelCount)
    {
        Result.PreviewPixels.Init(FColor::Black, PixelCount);
    }

    Result.Texture = Resource->GetRenderTargetTexture();

    if (Result.Texture.IsValid() && Settings.OutputFormat == EOmniOutputFormat::NVENCHardware)
    {
        FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool();
        ENQUEUE_RENDER_COMMAND(OmniCapturePlanarConvert)([Settings, SourceTexture = Result.Texture, OutputSize, bLinear = Result.bIsLinear, &Result, CompletionEvent](FRHICommandListImmediate& RHICmdList)
        {
            ConvertPlanarOnRenderThread(Settings, SourceTexture, OutputSize, bLinear, Result);
            CompletionEvent->Trigger();
        });

        CompletionEvent->Wait();
        FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
    }

    return Result;
}
