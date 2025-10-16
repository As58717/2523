#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SListViewBase;
template<typename ItemType> class SListView;
class IDetailsView;
class UOmniCaptureEditorSettings;
class UOmniCaptureSubsystem;

class OMNICAPTUREEDITOR_API SOmniCaptureControlPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SOmniCaptureControlPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
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

    void ModifyCaptureSettings(TFunctionRef<void(FOmniCaptureSettings&)> Mutator);
    FOmniCaptureSettings GetSettingsSnapshot() const;
    void ApplyVRMode(bool bVR180);
    void ApplyStereoMode(bool bStereo);
    void ApplyStereoLayout(EOmniCaptureStereoLayout Layout);
    void ApplyPerEyeWidth(int32 NewWidth);
    void ApplyPerEyeHeight(int32 NewHeight);
    void ApplyOutputFormat(EOmniOutputFormat Format);
    void ApplyCodec(EOmniCaptureCodec Codec);
    void ApplyColorFormat(EOmniCaptureColorFormat Format);
    void ApplyMetadataToggle(EMetadataToggle Toggle, bool bEnabled);

    ECheckBoxState GetVRModeCheckState(bool bVR180) const;
    void HandleVRModeChanged(ECheckBoxState NewState, bool bVR180);
    ECheckBoxState GetStereoModeCheckState(bool bStereo) const;
    void HandleStereoModeChanged(ECheckBoxState NewState, bool bStereo);
    FText GetStereoLayoutDisplayText() const;
    void HandleStereoLayoutChanged(TSharedPtr<EOmniCaptureStereoLayout> NewValue, ESelectInfo::Type SelectInfo);
    TSharedRef<SWidget> GenerateStereoLayoutOption(TSharedPtr<EOmniCaptureStereoLayout> InValue) const;

    int32 GetPerEyeWidthValue() const;
    int32 GetPerEyeHeightValue() const;
    void HandlePerEyeWidthCommitted(int32 NewValue, ETextCommit::Type CommitType);
    void HandlePerEyeHeightCommitted(int32 NewValue, ETextCommit::Type CommitType);

    ECheckBoxState GetMetadataToggleState(EMetadataToggle Toggle) const;
    void HandleMetadataToggleChanged(ECheckBoxState NewState, EMetadataToggle Toggle);

    ECheckBoxState GetPreviewViewCheckState(EOmniCapturePreviewView View) const;
    void HandlePreviewViewChanged(ECheckBoxState NewState, EOmniCapturePreviewView View);

    TSharedPtr<EOmniCaptureStereoLayout> FindStereoLayoutOption(EOmniCaptureStereoLayout Layout) const;
    TSharedPtr<EOmniOutputFormat> FindOutputFormatOption(EOmniOutputFormat Format) const;
    TSharedPtr<EOmniCaptureCodec> FindCodecOption(EOmniCaptureCodec Codec) const;
    TSharedPtr<EOmniCaptureColorFormat> FindColorFormatOption(EOmniCaptureColorFormat Format) const;

    void HandleOutputFormatChanged(TSharedPtr<EOmniOutputFormat> NewFormat, ESelectInfo::Type SelectInfo);
    void HandleCodecChanged(TSharedPtr<EOmniCaptureCodec> NewCodec, ESelectInfo::Type SelectInfo);
    void HandleColorFormatChanged(TSharedPtr<EOmniCaptureColorFormat> NewFormat, ESelectInfo::Type SelectInfo);

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
    TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

    TArray<TSharedPtr<EOmniCaptureStereoLayout>> StereoLayoutOptions;
    TArray<TSharedPtr<EOmniOutputFormat>> OutputFormatOptions;
    TArray<TSharedPtr<EOmniCaptureCodec>> CodecOptions;
    TArray<TSharedPtr<EOmniCaptureColorFormat>> ColorFormatOptions;

    TSharedPtr<SComboBox<TSharedPtr<EOmniCaptureStereoLayout>>> StereoLayoutCombo;
    TSharedPtr<SComboBox<TSharedPtr<EOmniOutputFormat>>> OutputFormatCombo;
    TSharedPtr<SComboBox<TSharedPtr<EOmniCaptureCodec>>> CodecCombo;
    TSharedPtr<SComboBox<TSharedPtr<EOmniCaptureColorFormat>>> ColorFormatCombo;
};
