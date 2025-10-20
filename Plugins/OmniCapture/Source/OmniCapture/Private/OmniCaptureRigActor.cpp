#include "OmniCaptureRigActor.h"

#include "Components/SceneComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "OmniCaptureIncludeFixes.h"
#include "UObject/Package.h"
#include "Kismet/KismetMathLibrary.h"
#include "OmniCaptureTypes.h"  // 增加头文件

namespace
{
    constexpr int32 CubemapFaceCount = 6;
}

AOmniCaptureRigActor::AOmniCaptureRigActor()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    RigRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RigRoot"));
    SetRootComponent(RigRoot);

    LeftEyeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LeftEyeRoot"));
    LeftEyeRoot->SetupAttachment(RigRoot);

    RightEyeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RightEyeRoot"));
    RightEyeRoot->SetupAttachment(RigRoot);
}

void AOmniCaptureRigActor::Configure(const FOmniCaptureSettings& InSettings)
{
    CachedSettings = InSettings;

    // 清除之前的捕获组件
    for (USceneCaptureComponent2D* Capture : LeftEyeCaptures)
    {
        if (Capture)
        {
            Capture->DestroyComponent();
        }
    }

    for (USceneCaptureComponent2D* Capture : RightEyeCaptures)
    {
        if (Capture)
        {
            Capture->DestroyComponent();
        }
    }

    for (UTextureRenderTarget2D* RenderTarget : RenderTargets)
    {
        if (RenderTarget)
        {
            RenderTarget->ConditionalBeginDestroy();
        }
    }

    LeftEyeCaptures.Empty();
    RightEyeCaptures.Empty();
    RenderTargets.Empty();

    const bool bPlanar = CachedSettings.IsPlanar();
    const int32 FaceCount = bPlanar ? 1 : CubemapFaceCount;

    const float IPDHalf = CachedSettings.Mode == EOmniCaptureMode::Stereo
        ? CachedSettings.InterPupillaryDistanceCm * 0.5f
        : 0.0f;

    BuildEyeRig(EOmniCaptureEye::Left, -IPDHalf, FaceCount);

    if (CachedSettings.Mode == EOmniCaptureMode::Stereo)
    {
        BuildEyeRig(EOmniCaptureEye::Right, IPDHalf, FaceCount);
    }

    ApplyStereoParameters();
}

void AOmniCaptureRigActor::Capture(FOmniEyeCapture& OutLeftEye, FOmniEyeCapture& OutRightEye) const
{
    CaptureEye(EOmniCaptureEye::Left, OutLeftEye);

    if (CachedSettings.Mode == EOmniCaptureMode::Stereo && RightEyeCaptures.Num() > 0)
    {
        CaptureEye(EOmniCaptureEye::Right, OutRightEye);
    }
    else
    {
        OutRightEye = OutLeftEye;
    }
}

void AOmniCaptureRigActor::BuildEyeRig(EOmniCaptureEye Eye, float IPDHalfCm, int32 FaceCount)
{
    USceneComponent* EyeRoot = Eye == EOmniCaptureEye::Left ? LeftEyeRoot : RightEyeRoot;

    if (!EyeRoot)
    {
        return;
    }

    TArray<USceneCaptureComponent2D*>& TargetArray = Eye == EOmniCaptureEye::Left ? LeftEyeCaptures : RightEyeCaptures;

    const FIntPoint TargetSize = CachedSettings.IsPlanar()
        ? CachedSettings.GetPlanarResolution()
        : FIntPoint(CachedSettings.Resolution, CachedSettings.Resolution);

    for (int32 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
    {
        FString ComponentName = FString::Printf(TEXT("%s_CaptureFace_%d"), Eye == EOmniCaptureEye::Left ? TEXT("Left") : TEXT("Right"), FaceIndex);
        USceneCaptureComponent2D* CaptureComponent = NewObject<USceneCaptureComponent2D>(this, *ComponentName);
        CaptureComponent->SetupAttachment(EyeRoot);
        CaptureComponent->RegisterComponent();
        ConfigureCaptureComponent(CaptureComponent, TargetSize);

        if (!CachedSettings.IsPlanar())
        {
            FRotator FaceRotation;
            GetOrientationForFace(FaceIndex, FaceRotation);
            CaptureComponent->SetRelativeRotation(FaceRotation);
        }

        TargetArray.Add(CaptureComponent);
    }
}

void AOmniCaptureRigActor::UpdateStereoParameters(float NewIPDCm, float NewConvergenceDistanceCm)
{
    if (CachedSettings.Mode != EOmniCaptureMode::Stereo)
    {
        CachedSettings.InterPupillaryDistanceCm = 0.0f;
        CachedSettings.EyeConvergenceDistanceCm = 0.0f;
        ApplyStereoParameters();
        return;
    }

    CachedSettings.InterPupillaryDistanceCm = FMath::Max(0.0f, NewIPDCm);
    CachedSettings.EyeConvergenceDistanceCm = FMath::Max(0.0f, NewConvergenceDistanceCm);
    ApplyStereoParameters();
}

void AOmniCaptureRigActor::ApplyStereoParameters()
{
    const float HalfIPD = CachedSettings.Mode == EOmniCaptureMode::Stereo
        ? CachedSettings.InterPupillaryDistanceCm * 0.5f
        : 0.0f;

    UpdateEyeRootTransform(LeftEyeRoot, -HalfIPD, EOmniCaptureEye::Left);
    UpdateEyeRootTransform(RightEyeRoot, HalfIPD, EOmniCaptureEye::Right);
}

void AOmniCaptureRigActor::UpdateEyeRootTransform(USceneComponent* EyeRoot, float LateralOffset, EOmniCaptureEye Eye) const
{
    if (!EyeRoot)
    {
        return;
    }

    EyeRoot->SetRelativeLocation(FVector(0.0f, LateralOffset, 0.0f));

    if (!CachedSettings.IsPlanar())
    {
        EyeRoot->SetRelativeRotation(FRotator::ZeroRotator);
        return;
    }

    const float ConvergenceDistance = CachedSettings.EyeConvergenceDistanceCm;
    if (ConvergenceDistance <= KINDA_SMALL_NUMBER)
    {
        EyeRoot->SetRelativeRotation(FRotator::ZeroRotator);
        return;
    }

    const FVector EyeLocation(0.0f, LateralOffset, 0.0f);
    const FVector FocusPoint(ConvergenceDistance, 0.0f, 0.0f);
    const FRotator EyeRotation = UKismetMathLibrary::FindLookAtRotation(EyeLocation, FocusPoint);
    EyeRoot->SetRelativeRotation(EyeRotation);
}

void AOmniCaptureRigActor::ConfigureCaptureComponent(USceneCaptureComponent2D* CaptureComponent, const FIntPoint& TargetSize) const
{
    if (!CaptureComponent)
    {
        return;
    }

    CaptureComponent->FOVAngle = 90.0f;
    CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorHDR;
    CaptureComponent->bCaptureEveryFrame = false;
    CaptureComponent->bCaptureOnMovement = false;
    CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;

    // ✅ 修复：在 const 成员函数中允许修改 RenderTargets
    UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
    check(RenderTarget);

    // 设置渲染目标
    const EPixelFormat PixelFormat = PF_FloatRGBA;
    const int32 SizeX = FMath::Max(2, TargetSize.X);
    const int32 SizeY = FMath::Max(2, TargetSize.Y);
    RenderTarget->InitCustomFormat(SizeX, SizeY, PixelFormat, false);
    RenderTarget->TargetGamma = CachedSettings.Gamma == EOmniCaptureGamma::Linear ? 1.0f : 2.2f;
    RenderTarget->bAutoGenerateMips = false;
    RenderTarget->ClearColor = FLinearColor::Black;
    RenderTarget->Filter = TF_Bilinear;

    CaptureComponent->TextureTarget = RenderTarget;

    // 通过 const_cast 修改 RenderTargets 数组
    const_cast<TArray<UTextureRenderTarget2D*>&>(RenderTargets).Add(RenderTarget);
}

void AOmniCaptureRigActor::CaptureEye(EOmniCaptureEye Eye, FOmniEyeCapture& OutCapture) const
{
    const TArray<USceneCaptureComponent2D*>& CaptureComponents = Eye == EOmniCaptureEye::Left ? LeftEyeCaptures : RightEyeCaptures;

    OutCapture.ActiveFaceCount = CaptureComponents.Num();

    for (int32 FaceIndex = 0; FaceIndex < UE_ARRAY_COUNT(OutCapture.Faces); ++FaceIndex)
    {
        OutCapture.Faces[FaceIndex].RenderTarget = nullptr;
    }

    for (int32 FaceIndex = 0; FaceIndex < CaptureComponents.Num(); ++FaceIndex)
    {
        if (USceneCaptureComponent2D* CaptureComponent = CaptureComponents[FaceIndex])
        {
            CaptureComponent->CaptureScene();

            UTextureRenderTarget2D* RenderTarget = Cast<UTextureRenderTarget2D>(CaptureComponent->TextureTarget);
            OutCapture.Faces[FaceIndex].RenderTarget = RenderTarget;
        }
    }
}

void AOmniCaptureRigActor::GetOrientationForFace(int32 FaceIndex, FRotator& OutRotation)
{
    switch (FaceIndex)
    {
    case 0: // +X
        OutRotation = FRotator(0.0f, 90.0f, 0.0f);
        break;
    case 1: // -X
        OutRotation = FRotator(0.0f, -90.0f, 0.0f);
        break;
    case 2: // +Y
        OutRotation = FRotator(-90.0f, 0.0f, 0.0f);
        break;
    case 3: // -Y
        OutRotation = FRotator(90.0f, 0.0f, 0.0f);
        break;
    case 4: // +Z
        OutRotation = FRotator(0.0f, 0.0f, 0.0f);
        break;
    case 5: // -Z
        OutRotation = FRotator(0.0f, 180.0f, 0.0f);
        break;
    default:
        OutRotation = FRotator::ZeroRotator;
        break;
    }
}

