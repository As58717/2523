#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OmniCaptureTypes.h"
#include "OmniCaptureRigActor.generated.h"

class USceneComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UENUM()
enum class EOmniCaptureEye : uint8
{
    Left,
    Right
};

struct FOmniCaptureFaceResources
{
    UTextureRenderTarget2D* RenderTarget = nullptr;
    TMap<EOmniCaptureAuxiliaryPassType, UTextureRenderTarget2D*> AuxiliaryTargets;
};

struct FOmniEyeCapture
{
    FOmniCaptureFaceResources Faces[6];
    int32 ActiveFaceCount = 0;

    UTextureRenderTarget2D* GetPrimaryRenderTarget() const
    {
        return ActiveFaceCount > 0 ? Faces[0].RenderTarget : nullptr;
    }
};

UCLASS(NotBlueprintable)
class OMNICAPTURE_API AOmniCaptureRigActor final : public AActor
{
    GENERATED_BODY()

public:
    AOmniCaptureRigActor();

    void Configure(const FOmniCaptureSettings& InSettings);
    void Capture(FOmniEyeCapture& OutLeftEye, FOmniEyeCapture& OutRightEye) const;
    void UpdateStereoParameters(float NewIPDCm, float NewConvergenceDistanceCm);

    FORCEINLINE const FTransform& GetRigTransform() const { return RigRoot->GetComponentTransform(); }

private:
    void BuildEyeRig(EOmniCaptureEye Eye, float IPDHalfCm, int32 FaceCount);
    void ConfigureCaptureComponent(USceneCaptureComponent2D* CaptureComponent, const FIntPoint& TargetSize) const;
    USceneCaptureComponent2D* CreateAuxiliaryCaptureComponent(const FString& ComponentName, EOmniCaptureAuxiliaryPassType PassType, const FIntPoint& TargetSize) const;
    void ConfigureAuxiliaryTargets(EOmniCaptureEye Eye, int32 FaceCount, const FIntPoint& TargetSize);
    void CaptureEye(EOmniCaptureEye Eye, FOmniEyeCapture& OutCapture) const;
    void ApplyStereoParameters();
    void UpdateEyeRootTransform(USceneComponent* EyeRoot, float LateralOffset, EOmniCaptureEye Eye) const;

    static void GetOrientationForFace(int32 FaceIndex, FRotator& OutRotation);

private:
    UPROPERTY()
    USceneComponent* RigRoot;

    UPROPERTY()
    USceneComponent* LeftEyeRoot;

    UPROPERTY()
    USceneComponent* RightEyeRoot;

    UPROPERTY()
    TArray<USceneCaptureComponent2D*> LeftEyeCaptures;

    UPROPERTY()
    TArray<USceneCaptureComponent2D*> RightEyeCaptures;

    UPROPERTY()
    TMap<EOmniCaptureAuxiliaryPassType, TArray<USceneCaptureComponent2D*>> LeftAuxiliaryCaptures;

    UPROPERTY()
    TMap<EOmniCaptureAuxiliaryPassType, TArray<USceneCaptureComponent2D*>> RightAuxiliaryCaptures;

    UPROPERTY(Transient)
    TArray<UTextureRenderTarget2D*> RenderTargets;

    FOmniCaptureSettings CachedSettings;
};

