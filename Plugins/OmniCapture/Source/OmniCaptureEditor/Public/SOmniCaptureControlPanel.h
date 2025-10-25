#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "OmniCaptureTypes.h"

template <typename EnumType>
struct TEnumOptionValue
{
    explicit TEnumOptionValue(EnumType InValue)
        : Value(InValue)
    {
    }

    EnumType GetValue() const
    {
        return Value;
    }

    EnumType Value;
};

template <typename EnumType>
using TEnumOptionPtr = TSharedPtr<TEnumOptionValue<EnumType>>;

class SListViewBase;
template<typename ItemType> class SListView;
class IDetailsView;
class SImage;
struct FSlateDynamicImageBrush;
class UOmniCaptureEditorSettings;
class UOmniCaptureSubsystem;
class UTexture2D;

class OMNICAPTUREEDITOR_API SOmniCaptureControlPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SOmniCaptureControlPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    struct FFeatureToggleState
    {
        bool bAvailable = true;
        FText Reason;
    };

    struct FFeatureAvailabilityState
    {
        FFeatureToggleState NVENC;
        FFeatureToggleState NVENCHEVC;
        FFeatureToggleState NVENCNV12;
        FFeatureToggleState NVENCP010;
        FFeatureToggleState ZeroCopy;
        FFeatureToggleState FFmpeg;
    };

    struct FDiagnosticListItem
    {
        FText Timestamp;
        FText RelativeTime;
        FText Step;
        FText Message;
        EOmniCaptureDiagnosticLevel Level = EOmniCaptureDiagnosticLevel::Info;
        bool bIsPlaceholder = false;
    };

    enum class EMetadataToggle : uint8
    {
        Manifest,
        SpatialJson,
        XMP,
        FFmpeg
    };

    FReply OnStartCapture();
    FReply OnStopCapture();
    FReply OnCaptureStill();
    FReply OnTogglePause();
    FReply OnOpenLastOutput();
    FReply OnBrowseOutputDirectory();
    bool CanStartCapture() const;
    bool CanStopCapture() const;
    bool CanCaptureStill() const;
    bool CanPauseCapture() const;
    bool CanResumeCapture() const;
    bool CanOpenLastOutput() const;
    FText GetPauseButtonText() const;
    bool IsPauseButtonEnabled() const;

    UOmniCaptureSubsystem* GetSubsystem() const;
    EActiveTimerReturnType HandleActiveTimer(double InCurrentTime, float InDeltaTime);
    void RefreshStatus();
    void UpdateOutputDirectoryDisplay();
    void RefreshConfigurationSummary();
    void UpdatePreviewTextureDisplay();
    void RefreshDiagnosticLog();

    void ModifyCaptureSettings(TFunctionRef<void(FOmniCaptureSettings&)> Mutator);
    FOmniCaptureSettings GetSettingsSnapshot() const;
    void ApplyVRMode(bool bVR180);
    void ApplyStereoMode(bool bStereo);
    void ApplyStereoLayout(EOmniCaptureStereoLayout Layout);
    void ApplyPerEyeWidth(int32 NewWidth);
    void ApplyPerEyeHeight(int32 NewHeight);
    void ApplyPlanarWidth(int32 NewWidth);
    void ApplyPlanarHeight(int32 NewHeight);
    void ApplyPlanarScale(int32 NewScale);
    void ApplyFisheyeWidth(int32 NewWidth);
    void ApplyFisheyeHeight(int32 NewHeight);
    void ApplyFisheyeFOV(float NewFov);
    void ApplyFisheyeType(EOmniCaptureFisheyeType Type);
    void ApplyFisheyeConvert(bool bEnable);
    void ApplyProjection(EOmniCaptureProjection Projection);
    void ApplyOutputFormat(EOmniOutputFormat Format);
    void ApplyCodec(EOmniCaptureCodec Codec);
    void ApplyColorFormat(EOmniCaptureColorFormat Format);
    void ApplyImageFormat(EOmniCaptureImageFormat Format);
    void ApplyPNGBitDepth(EOmniCapturePNGBitDepth BitDepth);
    void ApplyMetadataToggle(EMetadataToggle Toggle, bool bEnabled);

    ECheckBoxState GetVRModeCheckState(bool bVR180) const;
    void HandleVRModeChanged(ECheckBoxState NewState, bool bVR180);
    ECheckBoxState GetStereoModeCheckState(bool bStereo) const;
    void HandleStereoModeChanged(ECheckBoxState NewState, bool bStereo);
    FText GetStereoLayoutDisplayText() const;
    void HandleStereoLayoutChanged(TEnumOptionPtr<EOmniCaptureStereoLayout> NewValue, ESelectInfo::Type SelectInfo);
    TSharedRef<SWidget> GenerateStereoLayoutOption(TEnumOptionPtr<EOmniCaptureStereoLayout> InValue) const;
    void HandleProjectionChanged(TEnumOptionPtr<EOmniCaptureProjection> NewProjection, ESelectInfo::Type SelectInfo);
    TSharedRef<SWidget> GenerateProjectionOption(TEnumOptionPtr<EOmniCaptureProjection> InValue) const;
    void HandleImageFormatChanged(TEnumOptionPtr<EOmniCaptureImageFormat> NewFormat, ESelectInfo::Type SelectInfo);
    TSharedRef<SWidget> GenerateImageFormatOption(TEnumOptionPtr<EOmniCaptureImageFormat> InValue) const;
    void HandlePNGBitDepthChanged(TEnumOptionPtr<EOmniCapturePNGBitDepth> NewValue, ESelectInfo::Type SelectInfo);
    TSharedRef<SWidget> GeneratePNGBitDepthOption(TEnumOptionPtr<EOmniCapturePNGBitDepth> InValue) const;

    int32 GetPerEyeWidthValue() const;
    int32 GetPerEyeHeightValue() const;
    TOptional<int32> GetPerEyeDimensionMaxValue() const;
    int32 GetPlanarWidthValue() const;
    int32 GetPlanarHeightValue() const;
    int32 GetPlanarScaleValue() const;
    int32 GetFisheyeWidthValue() const;
    int32 GetFisheyeHeightValue() const;
    float GetFisheyeFOVValue() const;
    void HandlePerEyeWidthCommitted(int32 NewValue, ETextCommit::Type CommitType);
    void HandlePerEyeHeightCommitted(int32 NewValue, ETextCommit::Type CommitType);
    void HandlePlanarWidthCommitted(int32 NewValue, ETextCommit::Type CommitType);
    void HandlePlanarHeightCommitted(int32 NewValue, ETextCommit::Type CommitType);
    void HandlePlanarScaleCommitted(int32 NewValue, ETextCommit::Type CommitType);
    void HandleFisheyeWidthCommitted(int32 NewValue, ETextCommit::Type CommitType);
    void HandleFisheyeHeightCommitted(int32 NewValue, ETextCommit::Type CommitType);
    void HandleFisheyeFOVCommitted(float NewValue, ETextCommit::Type CommitType);
    void HandleFisheyeTypeChanged(TEnumOptionPtr<EOmniCaptureFisheyeType> NewValue, ESelectInfo::Type SelectInfo);
    TSharedRef<SWidget> GenerateFisheyeTypeOption(TEnumOptionPtr<EOmniCaptureFisheyeType> InValue) const;
    ECheckBoxState GetFisheyeConvertState() const;
    void HandleFisheyeConvertChanged(ECheckBoxState NewState);

    ECheckBoxState GetMetadataToggleState(EMetadataToggle Toggle) const;
    void HandleMetadataToggleChanged(ECheckBoxState NewState, EMetadataToggle Toggle);

    ECheckBoxState GetPreviewViewCheckState(EOmniCapturePreviewView View) const;
    void HandlePreviewViewChanged(ECheckBoxState NewState, EOmniCapturePreviewView View);

    TEnumOptionPtr<EOmniCaptureStereoLayout> FindStereoLayoutOption(EOmniCaptureStereoLayout Layout) const;
    TEnumOptionPtr<EOmniOutputFormat> FindOutputFormatOption(EOmniOutputFormat Format) const;
    TEnumOptionPtr<EOmniCaptureCodec> FindCodecOption(EOmniCaptureCodec Codec) const;
    TEnumOptionPtr<EOmniCaptureColorFormat> FindColorFormatOption(EOmniCaptureColorFormat Format) const;
    TEnumOptionPtr<EOmniCaptureProjection> FindProjectionOption(EOmniCaptureProjection Projection) const;
    TEnumOptionPtr<EOmniCaptureFisheyeType> FindFisheyeTypeOption(EOmniCaptureFisheyeType Type) const;
    TEnumOptionPtr<EOmniCaptureImageFormat> FindImageFormatOption(EOmniCaptureImageFormat Format) const;
    TEnumOptionPtr<EOmniCapturePNGBitDepth> FindPNGBitDepthOption(EOmniCapturePNGBitDepth BitDepth) const;

    void HandleOutputFormatChanged(TEnumOptionPtr<EOmniOutputFormat> NewFormat, ESelectInfo::Type SelectInfo);
    void HandleCodecChanged(TEnumOptionPtr<EOmniCaptureCodec> NewCodec, ESelectInfo::Type SelectInfo);
    void HandleColorFormatChanged(TEnumOptionPtr<EOmniCaptureColorFormat> NewFormat, ESelectInfo::Type SelectInfo);

    int32 GetTargetBitrate() const;
    int32 GetMaxBitrate() const;
    int32 GetGOPLength() const;
    int32 GetBFrameCount() const;
    void HandleTargetBitrateCommitted(int32 NewValue, ETextCommit::Type CommitType);
    void HandleMaxBitrateCommitted(int32 NewValue, ETextCommit::Type CommitType);
    void HandleGOPCommitted(int32 NewValue, ETextCommit::Type CommitType);
    void HandleBFramesCommitted(int32 NewValue, ETextCommit::Type CommitType);

    ECheckBoxState GetZeroCopyState() const;
    void HandleZeroCopyChanged(ECheckBoxState NewState);
    ECheckBoxState GetFastStartState() const;
    void HandleFastStartChanged(ECheckBoxState NewState);
    ECheckBoxState GetConstantFrameRateState() const;
    void HandleConstantFrameRateChanged(ECheckBoxState NewState);

    void RebuildWarningList(const TArray<FString>& Warnings);
    TSharedRef<ITableRow> GenerateWarningRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable);
    TSharedRef<ITableRow> GenerateDiagnosticRow(TSharedPtr<FDiagnosticListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
    FSlateColor GetDiagnosticLevelColor(EOmniCaptureDiagnosticLevel Level) const;
    FReply OnClearDiagnostics();
    bool CanClearDiagnostics() const;

    void RefreshFeatureAvailability(bool bForceRefresh = false);
    bool IsOutputFormatSelectable(EOmniOutputFormat Format) const;
    FText GetOutputFormatTooltip(EOmniOutputFormat Format) const;
    FText GetNVENCWarningText() const;
    EVisibility GetNVENCWarningVisibility() const;
    bool IsCodecSelectable(EOmniCaptureCodec Codec) const;
    FText GetCodecTooltip(EOmniCaptureCodec Codec) const;
    bool IsColorFormatSelectable(EOmniCaptureColorFormat Format) const;
    FText GetColorFormatTooltip(EOmniCaptureColorFormat Format) const;
    FText GetZeroCopyTooltip() const;
    FText GetZeroCopyWarningText() const;
    EVisibility GetZeroCopyWarningVisibility() const;
    FText GetFFmpegMetadataTooltip() const;
    FText GetFFmpegWarningText() const;
    EVisibility GetFFmpegWarningVisibility() const;

private:
    TWeakObjectPtr<UOmniCaptureEditorSettings> SettingsObject;
    TSharedPtr<IDetailsView> SettingsView;
    TSharedPtr<STextBlock> StatusTextBlock;
    TSharedPtr<STextBlock> ActiveConfigTextBlock;
    TSharedPtr<STextBlock> RingBufferTextBlock;
    TSharedPtr<STextBlock> AudioTextBlock;
    TSharedPtr<STextBlock> FrameRateTextBlock;
    TSharedPtr<STextBlock> LastStillTextBlock;
    TSharedPtr<STextBlock> OutputDirectoryTextBlock;
    TSharedPtr<STextBlock> DerivedPerEyeTextBlock;
    TSharedPtr<STextBlock> DerivedOutputTextBlock;
    TSharedPtr<STextBlock> DerivedFOVTextBlock;
    TSharedPtr<STextBlock> EncoderAlignmentTextBlock;
    TArray<TSharedPtr<FString>> WarningItems;
    TSharedPtr<SListView<TSharedPtr<FString>>> WarningListView;
    TArray<TSharedPtr<FDiagnosticListItem>> DiagnosticItems;
    TSharedPtr<SListView<TSharedPtr<FDiagnosticListItem>>> DiagnosticListView;
    TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;
    bool bHasDiagnostics = false;
    int32 LastDiagnosticCount = 0;

    TArray<TEnumOptionPtr<EOmniCaptureStereoLayout>> StereoLayoutOptions;
    TArray<TEnumOptionPtr<EOmniOutputFormat>> OutputFormatOptions;
    TArray<TEnumOptionPtr<EOmniCaptureCodec>> CodecOptions;
    TArray<TEnumOptionPtr<EOmniCaptureColorFormat>> ColorFormatOptions;
    TArray<TEnumOptionPtr<EOmniCaptureProjection>> ProjectionOptions;
    TArray<TEnumOptionPtr<EOmniCaptureFisheyeType>> FisheyeTypeOptions;
    TArray<TEnumOptionPtr<EOmniCaptureImageFormat>> ImageFormatOptions;
    TArray<TEnumOptionPtr<EOmniCapturePNGBitDepth>> PNGBitDepthOptions;

    TSharedPtr<SComboBox<TEnumOptionPtr<EOmniCaptureStereoLayout>>> StereoLayoutCombo;
    TSharedPtr<SComboBox<TEnumOptionPtr<EOmniOutputFormat>>> OutputFormatCombo;
    TSharedPtr<SComboBox<TEnumOptionPtr<EOmniCaptureCodec>>> CodecCombo;
    TSharedPtr<SComboBox<TEnumOptionPtr<EOmniCaptureColorFormat>>> ColorFormatCombo;
    TSharedPtr<SComboBox<TEnumOptionPtr<EOmniCaptureProjection>>> ProjectionCombo;
    TSharedPtr<SComboBox<TEnumOptionPtr<EOmniCaptureFisheyeType>>> FisheyeTypeCombo;
    TSharedPtr<SComboBox<TEnumOptionPtr<EOmniCaptureImageFormat>>> ImageFormatCombo;
    TSharedPtr<SComboBox<TEnumOptionPtr<EOmniCapturePNGBitDepth>>> PNGBitDepthCombo;

    FFeatureAvailabilityState FeatureAvailability;
    double LastFeatureAvailabilityCheckTime = 0.0;

    TSharedPtr<SImage> PreviewImageWidget;
    TSharedPtr<STextBlock> PreviewStatusText;
    TSharedPtr<STextBlock> PreviewResolutionText;
    TSharedPtr<FSlateDynamicImageBrush> PreviewBrush;
    TWeakObjectPtr<UTexture2D> CachedPreviewTexture;
    FVector2D CachedPreviewSize = FVector2D::ZeroVector;
};
