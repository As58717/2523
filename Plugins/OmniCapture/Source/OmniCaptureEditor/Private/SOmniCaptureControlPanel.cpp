#include "SOmniCaptureControlPanel.h"

#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "OmniCaptureEditorSettings.h"
#include "OmniCaptureSubsystem.h"
#include "PropertyEditorModule.h"
#include "Styling/CoreStyle.h"
#include "Internationalization/Internationalization.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "OmniCaptureTypes.h"


#define LOCTEXT_NAMESPACE "OmniCaptureControlPanel"

namespace
{
    FText CodecToText(EOmniCaptureCodec Codec)
    {
        switch (Codec)
        {
        case EOmniCaptureCodec::HEVC: return LOCTEXT("CodecHEVC", "HEVC");
        case EOmniCaptureCodec::H264:
        default: return LOCTEXT("CodecH264", "H.264");
        }
    }

    FText FormatToText(EOmniCaptureColorFormat Format)
    {
        switch (Format)
        {
        case EOmniCaptureColorFormat::NV12: return LOCTEXT("FormatNV12", "NV12");
        case EOmniCaptureColorFormat::P010: return LOCTEXT("FormatP010", "P010");
        case EOmniCaptureColorFormat::BGRA:
        default: return LOCTEXT("FormatBGRA", "BGRA");
        }
    }

    FText OutputFormatToText(EOmniOutputFormat Format)
    {
        switch (Format)
        {
        case EOmniOutputFormat::NVENCHardware:
            return LOCTEXT("OutputFormatNVENC", "NVENC (MP4)");
        case EOmniOutputFormat::ImageSequence:
        default:
            return LOCTEXT("OutputFormatImageSequence", "Image Sequence");
        }
    }

    FText ProjectionToText(EOmniCaptureProjection Projection)
    {
        switch (Projection)
        {
        case EOmniCaptureProjection::Planar2D:
            return LOCTEXT("ProjectionPlanar", "Planar 2D");
        case EOmniCaptureProjection::Equirectangular:
        default:
            return LOCTEXT("ProjectionEquirect", "Equirectangular");
        }
    }

    FText ImageFormatToText(EOmniCaptureImageFormat Format)
    {
        switch (Format)
        {
        case EOmniCaptureImageFormat::JPG:
            return LOCTEXT("ImageFormatJPG", "JPEG Sequence");
        case EOmniCaptureImageFormat::EXR:
            return LOCTEXT("ImageFormatEXR", "EXR Sequence");
        case EOmniCaptureImageFormat::PNG:
        default:
            return LOCTEXT("ImageFormatPNG", "PNG Sequence");
        }
    }

    int32 AlignDimensionUI(int32 Value, int32 Alignment)
    {
        const int32 SafeValue = FMath::Max(1, Value);
        if (Alignment <= 1)
        {
            return SafeValue;
        }

        const int32 Adjusted = ((SafeValue + Alignment - 1) / Alignment) * Alignment;
        return FMath::Max(Alignment, Adjusted);
    }

    FText CoverageToText(EOmniCaptureCoverage Coverage)
    {
        switch (Coverage)
        {
        case EOmniCaptureCoverage::HalfSphere:
            return LOCTEXT("Coverage180", "180°");
        case EOmniCaptureCoverage::FullSphere:
        default:
            return LOCTEXT("Coverage360", "360°");
        }
    }

    FText LayoutToText(const FOmniCaptureSettings& Settings)
    {
        if (Settings.Mode == EOmniCaptureMode::Stereo)
        {
            if (Settings.StereoLayout == EOmniCaptureStereoLayout::TopBottom)
            {
                return LOCTEXT("LayoutStereoTopBottom", "Stereo (Top-Bottom)");
            }
            return LOCTEXT("LayoutStereoSideBySide", "Stereo (Side-by-Side)");
        }

        return LOCTEXT("LayoutMono", "Mono");
    }
}

void SOmniCaptureControlPanel::Construct(const FArguments& InArgs)
{
    UOmniCaptureEditorSettings* Settings = GetMutableDefault<UOmniCaptureEditorSettings>();
    SettingsObject = Settings;

    FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs DetailsArgs;
    DetailsArgs.bAllowSearch = true;
    DetailsArgs.bHideSelectionTip = true;
    DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    SettingsView = PropertyEditor.CreateDetailView(DetailsArgs);
    SettingsView->SetObject(Settings);

    WarningListView = SNew(SListView<TSharedPtr<FString>>)
        .ListItemsSource(&WarningItems)
        .OnGenerateRow(this, &SOmniCaptureControlPanel::GenerateWarningRow)
        .SelectionMode(ESelectionMode::None);

    StereoLayoutOptions.Reset();
    StereoLayoutOptions.Add(MakeShared<EOmniCaptureStereoLayout>(EOmniCaptureStereoLayout::SideBySide));
    StereoLayoutOptions.Add(MakeShared<EOmniCaptureStereoLayout>(EOmniCaptureStereoLayout::TopBottom));

    OutputFormatOptions.Reset();
    OutputFormatOptions.Add(MakeShared<EOmniOutputFormat>(EOmniOutputFormat::NVENCHardware));
    OutputFormatOptions.Add(MakeShared<EOmniOutputFormat>(EOmniOutputFormat::ImageSequence));

    CodecOptions.Reset();
    CodecOptions.Add(MakeShared<EOmniCaptureCodec>(EOmniCaptureCodec::HEVC));
    CodecOptions.Add(MakeShared<EOmniCaptureCodec>(EOmniCaptureCodec::H264));

    ColorFormatOptions.Reset();
    ColorFormatOptions.Add(MakeShared<EOmniCaptureColorFormat>(EOmniCaptureColorFormat::NV12));
    ColorFormatOptions.Add(MakeShared<EOmniCaptureColorFormat>(EOmniCaptureColorFormat::P010));
    ColorFormatOptions.Add(MakeShared<EOmniCaptureColorFormat>(EOmniCaptureColorFormat::BGRA));

    ProjectionOptions.Reset();
    ProjectionOptions.Add(MakeShared<EOmniCaptureProjection>(EOmniCaptureProjection::Equirectangular));
    ProjectionOptions.Add(MakeShared<EOmniCaptureProjection>(EOmniCaptureProjection::Planar2D));

    ImageFormatOptions.Reset();
    ImageFormatOptions.Add(MakeShared<EOmniCaptureImageFormat>(EOmniCaptureImageFormat::PNG));
    ImageFormatOptions.Add(MakeShared<EOmniCaptureImageFormat>(EOmniCaptureImageFormat::JPG));
    ImageFormatOptions.Add(MakeShared<EOmniCaptureImageFormat>(EOmniCaptureImageFormat::EXR));

    auto BuildSection = [](const FText& Title, const FText& Description, const TSharedRef<SWidget>& Content) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .Padding(8.0f)
            .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(Title)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 2.f, 0.f, 6.f)
                [
                    SNew(STextBlock)
                    .Text(Description)
                    .AutoWrapText(true)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    Content
                ]
            ];
    };

    TSharedRef<SWidget> OutputModeSection = BuildSection(
        LOCTEXT("OutputModeSectionTitle", "Output Modes"),
        LOCTEXT("OutputModeSectionDesc", "Switch between 360° and VR180 capture, choose mono or stereo output, and manage layout safety constraints."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ProjectionLabel", "Projection"))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8.f, 0.f, 0.f, 0.f)
            [
                SAssignNew(ProjectionCombo, SComboBox<TSharedPtr<EOmniCaptureProjection>>)
                .OptionsSource(&ProjectionOptions)
                .OnGenerateWidget(this, &SOmniCaptureControlPanel::GenerateProjectionOption)
                .OnSelectionChanged(this, &SOmniCaptureControlPanel::HandleProjectionChanged)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        return ProjectionToText(GetSettingsSnapshot().Projection);
                    })
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.f, 0.f, 12.f, 0.f)
            [
                SNew(SCheckBox)
                .IsChecked(this, &SOmniCaptureControlPanel::GetVRModeCheckState, false)
                .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandleVRModeChanged, false)
                .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ModeFullSphere", "360° Full Sphere"))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SCheckBox)
                .IsChecked(this, &SOmniCaptureControlPanel::GetVRModeCheckState, true)
                .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandleVRModeChanged, true)
                .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ModeVR180", "VR180 Hemisphere"))
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.f, 6.f, 0.f, 0.f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.f, 0.f, 12.f, 0.f)
            [
                SNew(SCheckBox)
                .IsChecked(this, &SOmniCaptureControlPanel::GetStereoModeCheckState, false)
                .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandleStereoModeChanged, false)
                .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ModeMono", "Mono"))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SCheckBox)
                .IsChecked(this, &SOmniCaptureControlPanel::GetStereoModeCheckState, true)
                .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandleStereoModeChanged, true)
                .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ModeStereo", "Stereo"))
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.f, 6.f, 0.f, 0.f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("StereoLayoutLabel", "Stereo Layout:"))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8.f, 0.f, 0.f, 0.f)
            [
                SAssignNew(StereoLayoutCombo, SComboBox<TSharedPtr<EOmniCaptureStereoLayout>>)
                .OptionsSource(&StereoLayoutOptions)
                .OnGenerateWidget(this, &SOmniCaptureControlPanel::GenerateStereoLayoutOption)
                .OnSelectionChanged(this, &SOmniCaptureControlPanel::HandleStereoLayoutChanged)
                .IsEnabled_Lambda([this]()
                {
                    return GetSettingsSnapshot().IsStereo();
                })
                [
                    SNew(STextBlock)
                    .Text(this, &SOmniCaptureControlPanel::GetStereoLayoutDisplayText)
                ]
            ]
        ]
    );

    TSharedRef<SWidget> ResolutionSection = BuildSection(
        LOCTEXT("ResolutionSectionTitle", "Output Resolution"),
        LOCTEXT("ResolutionSectionDesc", "Configure either per-eye cube face resolution or planar output dimensions with integer scaling."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SVerticalBox)
            .Visibility_Lambda([this]()
            {
                return GetSettingsSnapshot().Projection == EOmniCaptureProjection::Equirectangular ? EVisibility::Visible : EVisibility::Collapsed;
            })
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SGridPanel)
                .FillColumn(1, 1.f)
                + SGridPanel::Slot(0, 0)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PerEyeWidthLabel", "Per-eye Width"))
                ]
                + SGridPanel::Slot(1, 0)
                [
                    SNew(SSpinBox<int32>)
                    .MinValue(512)
                    .MaxValue(16384)
                    .Delta(64)
                    .Value(this, &SOmniCaptureControlPanel::GetPerEyeWidthValue)
                    .OnValueCommitted(this, &SOmniCaptureControlPanel::HandlePerEyeWidthCommitted)
                ]
                + SGridPanel::Slot(0, 7)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PerEyeHeightLabel", "Per-eye Height"))
                ]
                + SGridPanel::Slot(1, 7)
                [
                    SNew(SSpinBox<int32>)
                    .MinValue(512)
                    .MaxValue(16384)
                    .Delta(64)
                    .Value(this, &SOmniCaptureControlPanel::GetPerEyeHeightValue)
                    .OnValueCommitted(this, &SOmniCaptureControlPanel::HandlePerEyeHeightCommitted)
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SVerticalBox)
            .Visibility_Lambda([this]()
            {
                return GetSettingsSnapshot().Projection == EOmniCaptureProjection::Planar2D ? EVisibility::Visible : EVisibility::Collapsed;
            })
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SGridPanel)
                .FillColumn(1, 1.f)
                + SGridPanel::Slot(0, 0)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PlanarWidthLabel", "Output Width"))
                ]
                + SGridPanel::Slot(1, 0)
                [
                    SNew(SSpinBox<int32>)
                    .MinValue(320)
                    .MaxValue(16384)
                    .Delta(64)
                    .Value(this, &SOmniCaptureControlPanel::GetPlanarWidthValue)
                    .OnValueCommitted(this, &SOmniCaptureControlPanel::HandlePlanarWidthCommitted)
                ]
                + SGridPanel::Slot(0, 1)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PlanarHeightLabel", "Output Height"))
                ]
                + SGridPanel::Slot(1, 1)
                [
                    SNew(SSpinBox<int32>)
                    .MinValue(240)
                    .MaxValue(16384)
                    .Delta(64)
                    .Value(this, &SOmniCaptureControlPanel::GetPlanarHeightValue)
                    .OnValueCommitted(this, &SOmniCaptureControlPanel::HandlePlanarHeightCommitted)
                ]
                + SGridPanel::Slot(0, 2)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PlanarScaleLabel", "Integer Scale"))
                ]
                + SGridPanel::Slot(1, 2)
                [
                    SNew(SSpinBox<int32>)
                    .MinValue(1)
                    .MaxValue(16)
                    .Delta(1)
                    .Value(this, &SOmniCaptureControlPanel::GetPlanarScaleValue)
                    .OnValueCommitted(this, &SOmniCaptureControlPanel::HandlePlanarScaleCommitted)
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.f, 6.f, 0.f, 0.f)
        [
            SAssignNew(DerivedPerEyeTextBlock, STextBlock)
            .Text(FText::GetEmpty())
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SAssignNew(DerivedOutputTextBlock, STextBlock)
            .Text(FText::GetEmpty())
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SAssignNew(DerivedFOVTextBlock, STextBlock)
            .Text(FText::GetEmpty())
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SAssignNew(EncoderAlignmentTextBlock, STextBlock)
            .Text(FText::GetEmpty())
        ]
    );

    TSharedRef<SWidget> MetadataSection = BuildSection(
        LOCTEXT("ProjectionMetadataTitle", "Projection & Metadata"),
        LOCTEXT("ProjectionMetadataDesc", "Control hemispherical coverage safeguards and exported VR metadata so that players like YouTube, Quest, and VLC detect VR180 correctly."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(LOCTEXT("CoverageHint", "VR180 mode automatically clips the back hemisphere and enforces 180° fields of view."))
            .AutoWrapText(true)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.f, 6.f, 0.f, 0.f)
        [
            SNew(SCheckBox)
            .IsChecked(this, &SOmniCaptureControlPanel::GetMetadataToggleState, EMetadataToggle::Manifest)
            .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandleMetadataToggleChanged, EMetadataToggle::Manifest)
            .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ManifestToggleLabel", "Generate capture manifest"))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCheckBox)
            .IsChecked(this, &SOmniCaptureControlPanel::GetMetadataToggleState, EMetadataToggle::SpatialJson)
            .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandleMetadataToggleChanged, EMetadataToggle::SpatialJson)
            .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SpatialJsonToggleLabel", "Write spatial metadata JSON"))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCheckBox)
            .IsChecked(this, &SOmniCaptureControlPanel::GetMetadataToggleState, EMetadataToggle::XMP)
            .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandleMetadataToggleChanged, EMetadataToggle::XMP)
            .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("XMPToggleLabel", "Embed Google VR XMP metadata"))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCheckBox)
            .IsChecked(this, &SOmniCaptureControlPanel::GetMetadataToggleState, EMetadataToggle::FFmpeg)
            .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandleMetadataToggleChanged, EMetadataToggle::FFmpeg)
            .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("FFmpegMetadataToggleLabel", "Inject FFmpeg spherical metadata"))
            ]
        ]
    );

    TSharedRef<SWidget> PreviewSection = BuildSection(
        LOCTEXT("PreviewSectionTitle", "Preview & Debug"),
        LOCTEXT("PreviewSectionDesc", "Inspect live previews per eye and track field-of-view, layout, and encoder alignment in real time."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SUniformGridPanel)
            .SlotPadding(FMargin(4.f))
            + SUniformGridPanel::Slot(0, 0)
            [
                SNew(SCheckBox)
                .IsChecked(this, &SOmniCaptureControlPanel::GetPreviewViewCheckState, EOmniCapturePreviewView::StereoComposite)
                .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandlePreviewViewChanged, EOmniCapturePreviewView::StereoComposite)
                .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PreviewStereoComposite", "Stereo Composite"))
                ]
            ]
            + SUniformGridPanel::Slot(1, 0)
            [
                SNew(SCheckBox)
                .IsChecked(this, &SOmniCaptureControlPanel::GetPreviewViewCheckState, EOmniCapturePreviewView::LeftEye)
                .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandlePreviewViewChanged, EOmniCapturePreviewView::LeftEye)
                .IsEnabled_Lambda([this]()
                {
                    return GetSettingsSnapshot().IsStereo();
                })
                .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PreviewLeftEye", "Left Eye"))
                ]
            ]
            + SUniformGridPanel::Slot(2, 0)
            [
                SNew(SCheckBox)
                .IsChecked(this, &SOmniCaptureControlPanel::GetPreviewViewCheckState, EOmniCapturePreviewView::RightEye)
                .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandlePreviewViewChanged, EOmniCapturePreviewView::RightEye)
                .IsEnabled_Lambda([this]()
                {
                    return GetSettingsSnapshot().IsStereo();
                })
                .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PreviewRightEye", "Right Eye"))
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.f, 6.f, 0.f, 0.f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("PreviewSafetyHint", "Preview toggles are disabled automatically while mono captures are active."))
            .AutoWrapText(true)
        ]
    );

    TSharedRef<SWidget> EncodingSection = BuildSection(
        LOCTEXT("EncodingSectionTitle", "Encoding & Output"),
        LOCTEXT("EncodingSectionDesc", "Pick encoder targets and tune codec, color format, and bitrate for production-ready deliverables."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SGridPanel)
            .FillColumn(1, 1.f)
            + SGridPanel::Slot(0, 0)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("OutputFormatLabel", "Output Format"))
            ]
            + SGridPanel::Slot(1, 0)
            [
                SAssignNew(OutputFormatCombo, SComboBox<TSharedPtr<EOmniOutputFormat>>)
                .OptionsSource(&OutputFormatOptions)
                .OnGenerateWidget_Lambda([](TSharedPtr<EOmniOutputFormat> InItem)
                {
                    return SNew(STextBlock).Text(OutputFormatToText(InItem.IsValid() ? *InItem : EOmniOutputFormat::ImageSequence));
                })
                .OnSelectionChanged(this, &SOmniCaptureControlPanel::HandleOutputFormatChanged)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        return OutputFormatToText(GetSettingsSnapshot().OutputFormat);
                    })
                ]
            ]
            + SGridPanel::Slot(0, 1)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ImageFormatLabel", "Image Format"))
                .Visibility_Lambda([this]()
                {
                    return GetSettingsSnapshot().OutputFormat == EOmniOutputFormat::ImageSequence ? EVisibility::Visible : EVisibility::Collapsed;
                })
            ]
            + SGridPanel::Slot(1, 1)
            [
                SAssignNew(ImageFormatCombo, SComboBox<TSharedPtr<EOmniCaptureImageFormat>>)
                .OptionsSource(&ImageFormatOptions)
                .OnGenerateWidget(this, &SOmniCaptureControlPanel::GenerateImageFormatOption)
                .OnSelectionChanged(this, &SOmniCaptureControlPanel::HandleImageFormatChanged)
                .Visibility_Lambda([this]()
                {
                    return GetSettingsSnapshot().OutputFormat == EOmniOutputFormat::ImageSequence ? EVisibility::Visible : EVisibility::Collapsed;
                })
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        return ImageFormatToText(GetSettingsSnapshot().ImageFormat);
                    })
                ]
            ]
            + SGridPanel::Slot(0, 2)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("CodecLabel", "Codec"))
            ]
            + SGridPanel::Slot(1, 2)
            [
                SAssignNew(CodecCombo, SComboBox<TSharedPtr<EOmniCaptureCodec>>)
                .OptionsSource(&CodecOptions)
                .OnGenerateWidget_Lambda([](TSharedPtr<EOmniCaptureCodec> InItem)
                {
                    return SNew(STextBlock).Text(CodecToText(InItem.IsValid() ? *InItem : EOmniCaptureCodec::H264));
                })
                .OnSelectionChanged(this, &SOmniCaptureControlPanel::HandleCodecChanged)
                .IsEnabled_Lambda([this]()
                {
                    return GetSettingsSnapshot().OutputFormat == EOmniOutputFormat::NVENCHardware;
                })
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        return CodecToText(GetSettingsSnapshot().Codec);
                    })
                ]
            ]
            + SGridPanel::Slot(0, 3)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ColorFormatLabel", "Color Format"))
            ]
            + SGridPanel::Slot(1, 3)
            [
                SAssignNew(ColorFormatCombo, SComboBox<TSharedPtr<EOmniCaptureColorFormat>>)
                .OptionsSource(&ColorFormatOptions)
                .OnGenerateWidget_Lambda([](TSharedPtr<EOmniCaptureColorFormat> InItem)
                {
                    return SNew(STextBlock).Text(FormatToText(InItem.IsValid() ? *InItem : EOmniCaptureColorFormat::NV12));
                })
                .OnSelectionChanged(this, &SOmniCaptureControlPanel::HandleColorFormatChanged)
                .IsEnabled_Lambda([this]()
                {
                    return GetSettingsSnapshot().OutputFormat == EOmniOutputFormat::NVENCHardware;
                })
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        return FormatToText(GetSettingsSnapshot().NVENCColorFormat);
                    })
                ]
            ]
            + SGridPanel::Slot(0, 4)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("TargetBitrateLabel", "Target Bitrate (kbps)"))
            ]
            + SGridPanel::Slot(1, 4)
            [
                SNew(SSpinBox<int32>)
                .MinValue(1000)
                .MaxValue(1000000)
                .Delta(1000)
                .Value(this, &SOmniCaptureControlPanel::GetTargetBitrate)
                .OnValueCommitted(this, &SOmniCaptureControlPanel::HandleTargetBitrateCommitted)
            ]
            + SGridPanel::Slot(0, 5)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("MaxBitrateLabel", "Max Bitrate (kbps)"))
            ]
            + SGridPanel::Slot(1, 5)
            [
                SNew(SSpinBox<int32>)
                .MinValue(1000)
                .MaxValue(1500000)
                .Delta(1000)
                .Value(this, &SOmniCaptureControlPanel::GetMaxBitrate)
                .OnValueCommitted(this, &SOmniCaptureControlPanel::HandleMaxBitrateCommitted)
            ]
            + SGridPanel::Slot(0, 6)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("GOPLengthLabel", "GOP Length"))
            ]
            + SGridPanel::Slot(1, 6)
            [
                SNew(SSpinBox<int32>)
                .MinValue(1)
                .MaxValue(600)
                .Delta(1)
                .Value(this, &SOmniCaptureControlPanel::GetGOPLength)
                .OnValueCommitted(this, &SOmniCaptureControlPanel::HandleGOPCommitted)
            ]
            + SGridPanel::Slot(0, 7)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("BFramesLabel", "B-Frames"))
            ]
            + SGridPanel::Slot(1, 7)
            [
                SNew(SSpinBox<int32>)
                .MinValue(0)
                .MaxValue(8)
                .Delta(1)
                .Value(this, &SOmniCaptureControlPanel::GetBFrameCount)
                .OnValueCommitted(this, &SOmniCaptureControlPanel::HandleBFramesCommitted)
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.f, 6.f, 0.f, 0.f)
        [
            SNew(SCheckBox)
            .IsChecked(this, &SOmniCaptureControlPanel::GetZeroCopyState)
            .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandleZeroCopyChanged)
            .IsEnabled_Lambda([this]()
            {
                return GetSettingsSnapshot().OutputFormat == EOmniOutputFormat::NVENCHardware;
            })
            .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ZeroCopyToggle", "Enable zero-copy NVENC"))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCheckBox)
            .IsChecked(this, &SOmniCaptureControlPanel::GetConstantFrameRateState)
            .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandleConstantFrameRateChanged)
            .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("CfrToggle", "Force constant frame rate"))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCheckBox)
            .IsChecked(this, &SOmniCaptureControlPanel::GetFastStartState)
            .OnCheckStateChanged(this, &SOmniCaptureControlPanel::HandleFastStartChanged)
            .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("FastStartToggle", "Enable fast-start MP4"))
            ]
        ]
    );

    TSharedRef<SWidget> AdvancedSection = BuildSection(
        LOCTEXT("AdvancedSectionTitle", "Advanced Settings"),
        LOCTEXT("AdvancedSectionDesc", "Full OmniCapture settings remain accessible for expert tuning."),
        SettingsView.ToSharedRef()
    );

    ChildSlot
    [
        SNew(SBorder)
        .Padding(8.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.f, 0.f, 0.f, 8.f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.f, 0.f, 8.f, 0.f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("StartCapture", "Start Capture"))
                    .OnClicked(this, &SOmniCaptureControlPanel::OnStartCapture)
                    .IsEnabled(this, &SOmniCaptureControlPanel::CanStartCapture)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.f, 0.f, 8.f, 0.f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CaptureStill", "Capture Still"))
                    .OnClicked(this, &SOmniCaptureControlPanel::OnCaptureStill)
                    .IsEnabled(this, &SOmniCaptureControlPanel::CanCaptureStill)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.f, 0.f, 8.f, 0.f)
                [
                    SNew(SButton)
                    .Text(this, &SOmniCaptureControlPanel::GetPauseButtonText)
                    .OnClicked(this, &SOmniCaptureControlPanel::OnTogglePause)
                    .IsEnabled(this, &SOmniCaptureControlPanel::IsPauseButtonEnabled)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("StopCapture", "Stop"))
                    .OnClicked(this, &SOmniCaptureControlPanel::OnStopCapture)
                    .IsEnabled(this, &SOmniCaptureControlPanel::CanStopCapture)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(8.f, 0.f, 0.f, 0.f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("OpenLastOutput", "Open Output"))
                    .OnClicked(this, &SOmniCaptureControlPanel::OnOpenLastOutput)
                    .IsEnabled(this, &SOmniCaptureControlPanel::CanOpenLastOutput)
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(StatusTextBlock, STextBlock)
                .Text(LOCTEXT("StatusIdle", "Status: Idle"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(ActiveConfigTextBlock, STextBlock)
                .Text(LOCTEXT("ConfigInactive", "Codec: - | Format: - | Zero Copy: -"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(LastStillTextBlock, STextBlock)
                .Text(LOCTEXT("LastStillInactive", "Last Still: -"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.f, 4.f, 8.f, 0.f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("BrowseOutputDirectory", "Set Output Folder"))
                    .OnClicked(this, &SOmniCaptureControlPanel::OnBrowseOutputDirectory)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.f)
                .VAlign(VAlign_Center)
                [
                    SAssignNew(OutputDirectoryTextBlock, STextBlock)
                    .Text(LOCTEXT("OutputDirectoryInactive", "Output Folder: -"))
                    .AutoWrapText(true)
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(FrameRateTextBlock, STextBlock)
                .Text(LOCTEXT("FrameRateInactive", "Frame Rate: 0.00 FPS"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(RingBufferTextBlock, STextBlock)
                .Text(LOCTEXT("RingBufferStats", "Ring Buffer: Pending 0 | Dropped 0 | Blocked 0"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(AudioTextBlock, STextBlock)
                .Text(LOCTEXT("AudioStats", "Audio Drift: 0 ms"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.f, 8.f)
            [
                SNew(SSeparator)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("WarningsHeader", "Environment & Warnings"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.f, 4.f)
            [
                SNew(SBox)
                .HeightOverride(96.f)
                [
                    WarningListView.ToSharedRef()
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.f, 8.f)
            [
                SNew(SSeparator)
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                .Padding(0.f, 0.f, 0.f, 8.f)
                [
                    OutputModeSection
                ]
                + SScrollBox::Slot()
                .Padding(0.f, 0.f, 0.f, 8.f)
                [
                    ResolutionSection
                ]
                + SScrollBox::Slot()
                .Padding(0.f, 0.f, 0.f, 8.f)
                [
                    MetadataSection
                ]
                + SScrollBox::Slot()
                .Padding(0.f, 0.f, 0.f, 8.f)
                [
                    PreviewSection
                ]
                + SScrollBox::Slot()
                .Padding(0.f, 0.f, 0.f, 8.f)
                [
                    EncodingSection
                ]
                + SScrollBox::Slot()
                [
                    AdvancedSection
                ]
            ]
        ]
    ];

    RefreshStatus();
    UpdateOutputDirectoryDisplay();
    RefreshConfigurationSummary();
    ActiveTimerHandle = RegisterActiveTimer(0.25f, FWidgetActiveTimerDelegate::CreateSP(this, &SOmniCaptureControlPanel::HandleActiveTimer));
}

FReply SOmniCaptureControlPanel::OnStartCapture()
{
    if (!SettingsObject.IsValid())
    {
        return FReply::Handled();
    }

    if (UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        Subsystem->BeginCapture(SettingsObject->CaptureSettings);
    }

    return FReply::Handled();
}

FReply SOmniCaptureControlPanel::OnCaptureStill()
{
    if (!SettingsObject.IsValid())
    {
        return FReply::Handled();
    }

    if (UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        FString OutputPath;
        Subsystem->CapturePanoramaStill(SettingsObject->CaptureSettings, OutputPath);
    }

    RefreshStatus();
    return FReply::Handled();
}

FReply SOmniCaptureControlPanel::OnStopCapture()
{
    if (UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        Subsystem->EndCapture(true);
    }

    return FReply::Handled();
}

FReply SOmniCaptureControlPanel::OnTogglePause()
{
    if (UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        if (Subsystem->IsPaused())
        {
            if (Subsystem->CanResume())
            {
                Subsystem->ResumeCapture();
            }
        }
        else if (Subsystem->CanPause())
        {
            Subsystem->PauseCapture();
        }
    }

    return FReply::Handled();
}

FReply SOmniCaptureControlPanel::OnOpenLastOutput()
{
    if (UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        const FString OutputPath = Subsystem->GetLastFinalizedOutputPath();
        if (!OutputPath.IsEmpty() && FPaths::FileExists(OutputPath))
        {
            FPlatformProcess::LaunchFileInDefaultExternalApplication(*OutputPath);
        }
    }

    return FReply::Handled();
}

FReply SOmniCaptureControlPanel::OnBrowseOutputDirectory()
{
    if (!SettingsObject.IsValid())
    {
        return FReply::Handled();
    }

    UOmniCaptureEditorSettings* Settings = SettingsObject.Get();
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        return FReply::Handled();
    }

    FString DefaultPath = Settings->CaptureSettings.OutputDirectory;
    if (DefaultPath.IsEmpty())
    {
        DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("OmniCaptures"));
    }

    const void* ParentWindowHandle = nullptr;
    if (FSlateApplication::IsInitialized())
    {
        ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
    }

    FString ChosenDirectory;
    const bool bOpened = DesktopPlatform->OpenDirectoryDialog(
        const_cast<void*>(ParentWindowHandle),
        TEXT("Choose Capture Output Folder"),
        DefaultPath,
        ChosenDirectory
    );

    if (bOpened)
    {
        const FString AbsoluteDirectory = FPaths::ConvertRelativePathToFull(ChosenDirectory);
        Settings->Modify();
        Settings->CaptureSettings.OutputDirectory = AbsoluteDirectory;
        Settings->SaveConfig();

        if (SettingsView.IsValid())
        {
            SettingsView->ForceRefresh();
        }

        UpdateOutputDirectoryDisplay();
    }

    return FReply::Handled();
}

bool SOmniCaptureControlPanel::CanStartCapture() const
{
    if (const UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        return !Subsystem->IsCapturing();
    }
    return false;
}

bool SOmniCaptureControlPanel::CanStopCapture() const
{
    if (const UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        return Subsystem->IsCapturing();
    }
    return false;
}

bool SOmniCaptureControlPanel::CanCaptureStill() const
{
    if (const UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        return !Subsystem->IsCapturing();
    }
    return false;
}

bool SOmniCaptureControlPanel::CanPauseCapture() const
{
    if (const UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        return Subsystem->CanPause();
    }
    return false;
}

bool SOmniCaptureControlPanel::CanResumeCapture() const
{
    if (const UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        return Subsystem->CanResume();
    }
    return false;
}

bool SOmniCaptureControlPanel::CanOpenLastOutput() const
{
    if (const UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        const FString OutputPath = Subsystem->GetLastFinalizedOutputPath();
        return Subsystem->HasFinalizedOutput() && !OutputPath.IsEmpty() && FPaths::FileExists(OutputPath);
    }
    return false;
}

FText SOmniCaptureControlPanel::GetPauseButtonText() const
{
    if (const UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        return Subsystem->IsPaused() ? LOCTEXT("ResumeCapture", "Resume") : LOCTEXT("PauseCapture", "Pause");
    }
    return LOCTEXT("PauseCapture", "Pause");
}

bool SOmniCaptureControlPanel::IsPauseButtonEnabled() const
{
    return CanPauseCapture() || CanResumeCapture();
}

UOmniCaptureSubsystem* SOmniCaptureControlPanel::GetSubsystem() const
{
    if (!GEditor)
    {
        return nullptr;
    }

    const FWorldContext& WorldContext = GEditor->GetEditorWorldContext();
    UWorld* World = WorldContext.World();
    return World ? World->GetSubsystem<UOmniCaptureSubsystem>() : nullptr;
}

EActiveTimerReturnType SOmniCaptureControlPanel::HandleActiveTimer(double InCurrentTime, float InDeltaTime)
{
    RefreshStatus();
    return EActiveTimerReturnType::Continue;
}

void SOmniCaptureControlPanel::RefreshStatus()
{
    if (!StatusTextBlock.IsValid())
    {
        return;
    }

    UOmniCaptureSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem)
    {
        StatusTextBlock->SetText(LOCTEXT("StatusNoWorld", "Status: No active editor world"));
        ActiveConfigTextBlock->SetText(LOCTEXT("ConfigUnavailable", "Codec: - | Format: - | Zero Copy: -"));
        if (LastStillTextBlock.IsValid())
        {
            LastStillTextBlock->SetText(LOCTEXT("LastStillInactive", "Last Still: -"));
        }
        if (FrameRateTextBlock.IsValid())
        {
            FrameRateTextBlock->SetText(LOCTEXT("FrameRateInactive", "Frame Rate: 0.00 FPS"));
        }
        RingBufferTextBlock->SetText(FText::GetEmpty());
        AudioTextBlock->SetText(FText::GetEmpty());
        UpdateOutputDirectoryDisplay();
        RebuildWarningList(TArray<FString>());
        return;
    }

    StatusTextBlock->SetText(FText::FromString(Subsystem->GetStatusString()));

    const bool bCapturing = Subsystem->IsCapturing();
    const FOmniCaptureSettings& Settings = bCapturing ? Subsystem->GetActiveSettings() : (SettingsObject.IsValid() ? SettingsObject->CaptureSettings : FOmniCaptureSettings());

    const FIntPoint OutputSize = Settings.GetOutputResolution();
    const FText ProjectionText = ProjectionToText(Settings.Projection);
    const FText CoverageText = Settings.IsPlanar() ? LOCTEXT("CoverageNA", "N/A") : CoverageToText(Settings.Coverage);
    const FText LayoutText = LayoutToText(Settings);
    const FText OutputFormatText = OutputFormatToText(Settings.OutputFormat);
    const FText CodecText = Settings.OutputFormat == EOmniOutputFormat::NVENCHardware ? CodecToText(Settings.Codec) : LOCTEXT("CodecNotApplicable", "N/A");
    const FText ColorFormatText = Settings.OutputFormat == EOmniOutputFormat::NVENCHardware ? FormatToText(Settings.NVENCColorFormat) : LOCTEXT("ColorFormatNotApplicable", "N/A");
    const FText ZeroCopyText = Settings.OutputFormat == EOmniOutputFormat::NVENCHardware
        ? (Settings.bZeroCopy ? LOCTEXT("ZeroCopyYes", "Yes") : LOCTEXT("ZeroCopyNo", "No"))
        : LOCTEXT("ZeroCopyNotApplicable", "N/A");
    const FText ImageFormatText = Settings.OutputFormat == EOmniOutputFormat::ImageSequence ? ImageFormatToText(Settings.ImageFormat) : LOCTEXT("ImageFormatNotApplicable", "N/A");

    const FText ConfigText = FText::Format(LOCTEXT("ConfigFormat", "Output: {0} | Projection: {1} | Coverage: {2} | Layout: {3} | Resolution: {4}×{5} | Codec: {6} | Color: {7} | Zero Copy: {8} | Images: {9}"),
        OutputFormatText,
        ProjectionText,
        CoverageText,
        LayoutText,
        FText::AsNumber(OutputSize.X),
        FText::AsNumber(OutputSize.Y),
        CodecText,
        ColorFormatText,
        ZeroCopyText,
        ImageFormatText);
    ActiveConfigTextBlock->SetText(ConfigText);

    if (LastStillTextBlock.IsValid())
    {
        const FString LastStillPath = Subsystem->GetLastStillImagePath();
        LastStillTextBlock->SetText(LastStillPath.IsEmpty()
            ? LOCTEXT("LastStillInactive", "Last Still: -")
            : FText::Format(LOCTEXT("LastStillFormat", "Last Still: {0}"), FText::FromString(LastStillPath)));
    }

    if (FrameRateTextBlock.IsValid())
    {
        const double CurrentFps = Subsystem->GetCurrentFrameRate();
        FNumberFormattingOptions FpsFormat;
        FpsFormat.SetMinimumFractionalDigits(2);
        FpsFormat.SetMaximumFractionalDigits(2);
        const FText FrameText = FText::Format(LOCTEXT("FrameRateFormat", "Frame Rate: {0} FPS"), FText::AsNumber(CurrentFps, &FpsFormat));
        FrameRateTextBlock->SetText(FrameText);
        FrameRateTextBlock->SetColorAndOpacity(Subsystem->IsPaused() ? FSlateColor(FLinearColor::Gray) : FSlateColor::UseForeground());
    }

    const FOmniCaptureRingBufferStats RingStats = Subsystem->GetRingBufferStats();
    const FText RingText = FText::Format(LOCTEXT("RingStatsFormat", "Ring Buffer: Pending {0} | Dropped {1} | Blocked {2}"),
        FText::AsNumber(RingStats.PendingFrames),
        FText::AsNumber(RingStats.DroppedFrames),
        FText::AsNumber(RingStats.BlockedPushes));
    RingBufferTextBlock->SetText(RingText);

    const FOmniAudioSyncStats AudioStats = Subsystem->GetAudioSyncStats();
    const FString DriftString = FString::Printf(TEXT("%.2f"), AudioStats.DriftMilliseconds);
    const FString MaxString = FString::Printf(TEXT("%.2f"), AudioStats.MaxObservedDriftMilliseconds);
    const FText AudioText = FText::Format(LOCTEXT("AudioStatsFormat", "Audio Drift: {0} ms (Max {1} ms) Pending {2}"),
        FText::FromString(DriftString),
        FText::FromString(MaxString),
        FText::AsNumber(AudioStats.PendingPackets));
    AudioTextBlock->SetText(AudioText);
    AudioTextBlock->SetColorAndOpacity(AudioStats.bInError ? FSlateColor(FLinearColor::Red) : FSlateColor::UseForeground());

    UpdateOutputDirectoryDisplay();
    RebuildWarningList(Subsystem->GetActiveWarnings());
    RefreshConfigurationSummary();
}

void SOmniCaptureControlPanel::RebuildWarningList(const TArray<FString>& Warnings)
{
    WarningItems.Reset();
    for (const FString& Warning : Warnings)
    {
        WarningItems.Add(MakeShared<FString>(Warning));
    }

    if (WarningItems.Num() == 0)
    {
        WarningItems.Add(MakeShared<FString>(LOCTEXT("NoWarnings", "No warnings detected").ToString()));
    }

    if (WarningListView.IsValid())
    {
        WarningListView->RequestListRefresh();
    }
}

TSharedRef<ITableRow> SOmniCaptureControlPanel::GenerateWarningRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
        [
            SNew(STextBlock)
            .Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty())
        ];
}

void SOmniCaptureControlPanel::UpdateOutputDirectoryDisplay()
{
    if (!OutputDirectoryTextBlock.IsValid())
    {
        return;
    }

    FString DisplayPath = TEXT("-");
    if (SettingsObject.IsValid())
    {
        const FString& ConfiguredPath = SettingsObject->CaptureSettings.OutputDirectory;
        if (ConfiguredPath.IsEmpty())
        {
            DisplayPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("OmniCaptures"));
        }
        else
        {
            DisplayPath = FPaths::ConvertRelativePathToFull(ConfiguredPath);
        }
    }

    OutputDirectoryTextBlock->SetText(FText::Format(LOCTEXT("OutputDirectoryFormat", "Output Folder: {0}"), FText::FromString(DisplayPath)));
}

void SOmniCaptureControlPanel::RefreshConfigurationSummary()
{
    const FOmniCaptureSettings Snapshot = GetSettingsSnapshot();

    const FIntPoint PerEyeSize = Snapshot.GetPerEyeOutputResolution();
    const FIntPoint OutputSize = Snapshot.GetOutputResolution();
    const int32 Alignment = Snapshot.GetEncoderAlignmentRequirement();

    if (DerivedPerEyeTextBlock.IsValid())
    {
        if (Snapshot.IsPlanar())
        {
            const FIntPoint BasePlanar(FMath::Max(1, Snapshot.PlanarResolution.X), FMath::Max(1, Snapshot.PlanarResolution.Y));
            DerivedPerEyeTextBlock->SetText(FText::Format(LOCTEXT("PlanarBaseSummary", "Planar base: {0}×{1}"), FText::AsNumber(BasePlanar.X), FText::AsNumber(BasePlanar.Y)));
        }
        else
        {
            DerivedPerEyeTextBlock->SetText(FText::Format(LOCTEXT("PerEyeSummary", "Per-eye output: {0}×{1}"), FText::AsNumber(PerEyeSize.X), FText::AsNumber(PerEyeSize.Y)));
        }
    }
    if (DerivedOutputTextBlock.IsValid())
    {
        if (Snapshot.IsPlanar())
        {
            DerivedOutputTextBlock->SetText(FText::Format(LOCTEXT("PlanarOutputSummary", "Final frame: {0}×{1} (Scale ×{2})"),
                FText::AsNumber(OutputSize.X),
                FText::AsNumber(OutputSize.Y),
                FText::AsNumber(FMath::Max(1, Snapshot.PlanarIntegerScale))));
        }
        else
        {
            DerivedOutputTextBlock->SetText(FText::Format(LOCTEXT("OutputSummary", "Final frame: {0}×{1} ({2})"),
                FText::AsNumber(OutputSize.X),
                FText::AsNumber(OutputSize.Y),
                LayoutToText(Snapshot)));
        }
    }
    if (DerivedFOVTextBlock.IsValid())
    {
        if (Snapshot.IsPlanar())
        {
            DerivedFOVTextBlock->SetText(LOCTEXT("FOVSummaryPlanar", "FOV: N/A for planar projection"));
        }
        else
        {
            const FText FovText = FText::Format(LOCTEXT("FOVSummary", "FOV: {0}° horizontal × {1}° vertical"),
                FText::AsNumber(Snapshot.GetHorizontalFOVDegrees()),
                FText::AsNumber(Snapshot.GetVerticalFOVDegrees()));
            DerivedFOVTextBlock->SetText(FovText);
        }
    }
    if (EncoderAlignmentTextBlock.IsValid())
    {
        EncoderAlignmentTextBlock->SetText(FText::Format(LOCTEXT("AlignmentSummary", "Encoder alignment: {0}-pixel"), FText::AsNumber(Alignment)));
    }

    if (StereoLayoutCombo.IsValid())
    {
        StereoLayoutCombo->SetSelectedItem(FindStereoLayoutOption(Snapshot.StereoLayout));
    }
    if (OutputFormatCombo.IsValid())
    {
        OutputFormatCombo->SetSelectedItem(FindOutputFormatOption(Snapshot.OutputFormat));
    }
    if (CodecCombo.IsValid())
    {
        CodecCombo->SetSelectedItem(FindCodecOption(Snapshot.Codec));
    }
    if (ColorFormatCombo.IsValid())
    {
        ColorFormatCombo->SetSelectedItem(FindColorFormatOption(Snapshot.NVENCColorFormat));
    }
    if (ProjectionCombo.IsValid())
    {
        ProjectionCombo->SetSelectedItem(FindProjectionOption(Snapshot.Projection));
    }
    if (ImageFormatCombo.IsValid())
    {
        ImageFormatCombo->SetSelectedItem(FindImageFormatOption(Snapshot.ImageFormat));
    }
}

void SOmniCaptureControlPanel::ModifyCaptureSettings(TFunctionRef<void(FOmniCaptureSettings&)> Mutator)
{
    if (!SettingsObject.IsValid())
    {
        return;
    }

    UOmniCaptureEditorSettings* Settings = SettingsObject.Get();
    Settings->Modify();
    Mutator(Settings->CaptureSettings);
    Settings->SaveConfig();

    if (SettingsView.IsValid())
    {
        SettingsView->ForceRefresh();
    }

    RefreshConfigurationSummary();
    RefreshStatus();
}

FOmniCaptureSettings SOmniCaptureControlPanel::GetSettingsSnapshot() const
{
    if (const UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        if (Subsystem->IsCapturing())
        {
            return Subsystem->GetActiveSettings();
        }
    }

    return SettingsObject.IsValid() ? SettingsObject->CaptureSettings : FOmniCaptureSettings();
}

void SOmniCaptureControlPanel::ApplyVRMode(bool bVR180)
{
    ModifyCaptureSettings([bVR180](FOmniCaptureSettings& Settings)
    {
        Settings.Coverage = bVR180 ? EOmniCaptureCoverage::HalfSphere : EOmniCaptureCoverage::FullSphere;
    });
}

void SOmniCaptureControlPanel::ApplyStereoMode(bool bStereo)
{
    ModifyCaptureSettings([bStereo](FOmniCaptureSettings& Settings)
    {
        Settings.Mode = bStereo ? EOmniCaptureMode::Stereo : EOmniCaptureMode::Mono;
        if (!bStereo)
        {
            Settings.PreviewVisualization = EOmniCapturePreviewView::StereoComposite;
        }
    });

    if (!bStereo)
    {
        if (UOmniCaptureSubsystem* Subsystem = GetSubsystem())
        {
            Subsystem->SetPreviewVisualizationMode(EOmniCapturePreviewView::StereoComposite);
        }
    }
}

void SOmniCaptureControlPanel::ApplyStereoLayout(EOmniCaptureStereoLayout Layout)
{
    ModifyCaptureSettings([Layout](FOmniCaptureSettings& Settings)
    {
        Settings.StereoLayout = Layout;
    });
}

void SOmniCaptureControlPanel::ApplyPerEyeWidth(int32 NewWidth)
{
    ModifyCaptureSettings([NewWidth](FOmniCaptureSettings& Settings)
    {
        const int32 Alignment = Settings.GetEncoderAlignmentRequirement();
        const int32 Base = Settings.IsVR180() ? NewWidth : FMath::Max(1, NewWidth / 2);
        Settings.Resolution = AlignDimensionUI(Base, Alignment);
    });
}

void SOmniCaptureControlPanel::ApplyPerEyeHeight(int32 NewHeight)
{
    ModifyCaptureSettings([NewHeight](FOmniCaptureSettings& Settings)
    {
        const int32 Alignment = Settings.GetEncoderAlignmentRequirement();
        Settings.Resolution = AlignDimensionUI(NewHeight, Alignment);
    });
}

void SOmniCaptureControlPanel::ApplyPlanarWidth(int32 NewWidth)
{
    ModifyCaptureSettings([NewWidth](FOmniCaptureSettings& Settings)
    {
        Settings.PlanarResolution.X = FMath::Max(16, NewWidth);
    });
}

void SOmniCaptureControlPanel::ApplyPlanarHeight(int32 NewHeight)
{
    ModifyCaptureSettings([NewHeight](FOmniCaptureSettings& Settings)
    {
        Settings.PlanarResolution.Y = FMath::Max(16, NewHeight);
    });
}

void SOmniCaptureControlPanel::ApplyPlanarScale(int32 NewScale)
{
    ModifyCaptureSettings([NewScale](FOmniCaptureSettings& Settings)
    {
        Settings.PlanarIntegerScale = FMath::Clamp(NewScale, 1, 16);
    });
}

void SOmniCaptureControlPanel::ApplyProjection(EOmniCaptureProjection Projection)
{
    ModifyCaptureSettings([Projection](FOmniCaptureSettings& Settings)
    {
        Settings.Projection = Projection;
    });
}

void SOmniCaptureControlPanel::ApplyOutputFormat(EOmniOutputFormat Format)
{
    ModifyCaptureSettings([Format](FOmniCaptureSettings& Settings)
    {
        Settings.OutputFormat = Format;
    });
}

void SOmniCaptureControlPanel::ApplyCodec(EOmniCaptureCodec Codec)
{
    ModifyCaptureSettings([Codec](FOmniCaptureSettings& Settings)
    {
        Settings.Codec = Codec;
    });
}

void SOmniCaptureControlPanel::ApplyColorFormat(EOmniCaptureColorFormat Format)
{
    ModifyCaptureSettings([Format](FOmniCaptureSettings& Settings)
    {
        Settings.NVENCColorFormat = Format;
    });
}

void SOmniCaptureControlPanel::ApplyImageFormat(EOmniCaptureImageFormat Format)
{
    ModifyCaptureSettings([Format](FOmniCaptureSettings& Settings)
    {
        Settings.ImageFormat = Format;
    });
}

void SOmniCaptureControlPanel::ApplyMetadataToggle(EMetadataToggle Toggle, bool bEnabled)
{
    ModifyCaptureSettings([Toggle, bEnabled](FOmniCaptureSettings& Settings)
    {
        switch (Toggle)
        {
        case EMetadataToggle::Manifest:
            Settings.bGenerateManifest = bEnabled;
            break;
        case EMetadataToggle::SpatialJson:
            Settings.bWriteSpatialMetadata = bEnabled;
            break;
        case EMetadataToggle::XMP:
            Settings.bWriteXMPMetadata = bEnabled;
            break;
        case EMetadataToggle::FFmpeg:
            Settings.bInjectFFmpegMetadata = bEnabled;
            break;
        default:
            break;
        }
    });
}

ECheckBoxState SOmniCaptureControlPanel::GetVRModeCheckState(bool bVR180) const
{
    const bool bIsVR180 = GetSettingsSnapshot().IsVR180();
    return (bIsVR180 == bVR180) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SOmniCaptureControlPanel::HandleVRModeChanged(ECheckBoxState NewState, bool bVR180)
{
    if (NewState == ECheckBoxState::Checked)
    {
        ApplyVRMode(bVR180);
    }
}

ECheckBoxState SOmniCaptureControlPanel::GetStereoModeCheckState(bool bStereo) const
{
    const bool bIsStereo = GetSettingsSnapshot().IsStereo();
    return (bIsStereo == bStereo) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SOmniCaptureControlPanel::HandleStereoModeChanged(ECheckBoxState NewState, bool bStereo)
{
    if (NewState == ECheckBoxState::Checked)
    {
        ApplyStereoMode(bStereo);
    }
}

FText SOmniCaptureControlPanel::GetStereoLayoutDisplayText() const
{
    return LayoutToText(GetSettingsSnapshot());
}

void SOmniCaptureControlPanel::HandleStereoLayoutChanged(TSharedPtr<EOmniCaptureStereoLayout> NewValue, ESelectInfo::Type SelectInfo)
{
    if (NewValue.IsValid())
    {
        ApplyStereoLayout(*NewValue);
    }
}

TSharedRef<SWidget> SOmniCaptureControlPanel::GenerateStereoLayoutOption(TSharedPtr<EOmniCaptureStereoLayout> InValue) const
{
    const EOmniCaptureStereoLayout Layout = InValue.IsValid() ? *InValue : EOmniCaptureStereoLayout::SideBySide;
    const FText Label = (Layout == EOmniCaptureStereoLayout::TopBottom)
        ? LOCTEXT("StereoLayoutTopBottom", "Top / Bottom")
        : LOCTEXT("StereoLayoutSideBySide", "Side-by-Side");
    return SNew(STextBlock).Text(Label);
}

void SOmniCaptureControlPanel::HandleProjectionChanged(TSharedPtr<EOmniCaptureProjection> NewProjection, ESelectInfo::Type SelectInfo)
{
    if (NewProjection.IsValid())
    {
        ApplyProjection(*NewProjection);
    }
}

TSharedRef<SWidget> SOmniCaptureControlPanel::GenerateProjectionOption(TSharedPtr<EOmniCaptureProjection> InValue) const
{
    const EOmniCaptureProjection Projection = InValue.IsValid() ? *InValue : EOmniCaptureProjection::Equirectangular;
    return SNew(STextBlock).Text(ProjectionToText(Projection));
}

int32 SOmniCaptureControlPanel::GetPerEyeWidthValue() const
{
    return GetSettingsSnapshot().GetPerEyeOutputResolution().X;
}

int32 SOmniCaptureControlPanel::GetPerEyeHeightValue() const
{
    return GetSettingsSnapshot().GetPerEyeOutputResolution().Y;
}

int32 SOmniCaptureControlPanel::GetPlanarWidthValue() const
{
    return GetSettingsSnapshot().PlanarResolution.X;
}

int32 SOmniCaptureControlPanel::GetPlanarHeightValue() const
{
    return GetSettingsSnapshot().PlanarResolution.Y;
}

int32 SOmniCaptureControlPanel::GetPlanarScaleValue() const
{
    return GetSettingsSnapshot().PlanarIntegerScale;
}

void SOmniCaptureControlPanel::HandlePerEyeWidthCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
    ApplyPerEyeWidth(FMath::Max(1, NewValue));
}

void SOmniCaptureControlPanel::HandlePerEyeHeightCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
    ApplyPerEyeHeight(FMath::Max(1, NewValue));
}

void SOmniCaptureControlPanel::HandlePlanarWidthCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
    ApplyPlanarWidth(FMath::Max(16, NewValue));
}

void SOmniCaptureControlPanel::HandlePlanarHeightCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
    ApplyPlanarHeight(FMath::Max(16, NewValue));
}

void SOmniCaptureControlPanel::HandlePlanarScaleCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
    ApplyPlanarScale(FMath::Max(1, NewValue));
}

ECheckBoxState SOmniCaptureControlPanel::GetMetadataToggleState(EMetadataToggle Toggle) const
{
    const FOmniCaptureSettings Snapshot = GetSettingsSnapshot();
    bool bEnabled = false;

    switch (Toggle)
    {
    case EMetadataToggle::Manifest:
        bEnabled = Snapshot.bGenerateManifest;
        break;
    case EMetadataToggle::SpatialJson:
        bEnabled = Snapshot.bWriteSpatialMetadata;
        break;
    case EMetadataToggle::XMP:
        bEnabled = Snapshot.bWriteXMPMetadata;
        break;
    case EMetadataToggle::FFmpeg:
        bEnabled = Snapshot.bInjectFFmpegMetadata;
        break;
    default:
        break;
    }

    return bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SOmniCaptureControlPanel::HandleMetadataToggleChanged(ECheckBoxState NewState, EMetadataToggle Toggle)
{
    ApplyMetadataToggle(Toggle, NewState == ECheckBoxState::Checked);
}

ECheckBoxState SOmniCaptureControlPanel::GetPreviewViewCheckState(EOmniCapturePreviewView View) const
{
    return GetSettingsSnapshot().PreviewVisualization == View ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SOmniCaptureControlPanel::HandlePreviewViewChanged(ECheckBoxState NewState, EOmniCapturePreviewView View)
{
    if (NewState != ECheckBoxState::Checked)
    {
        return;
    }

    ModifyCaptureSettings([View](FOmniCaptureSettings& Settings)
    {
        Settings.PreviewVisualization = View;
    });

    if (UOmniCaptureSubsystem* Subsystem = GetSubsystem())
    {
        Subsystem->SetPreviewVisualizationMode(View);
    }
}

TSharedPtr<EOmniCaptureStereoLayout> SOmniCaptureControlPanel::FindStereoLayoutOption(EOmniCaptureStereoLayout Layout) const
{
    for (const TSharedPtr<EOmniCaptureStereoLayout>& Option : StereoLayoutOptions)
    {
        if (Option.IsValid() && *Option == Layout)
        {
            return Option;
        }
    }
    return StereoLayoutOptions.Num() > 0 ? StereoLayoutOptions[0] : nullptr;
}

TSharedPtr<EOmniOutputFormat> SOmniCaptureControlPanel::FindOutputFormatOption(EOmniOutputFormat Format) const
{
    for (const TSharedPtr<EOmniOutputFormat>& Option : OutputFormatOptions)
    {
        if (Option.IsValid() && *Option == Format)
        {
            return Option;
        }
    }
    return OutputFormatOptions.Num() > 0 ? OutputFormatOptions[0] : nullptr;
}

TSharedPtr<EOmniCaptureCodec> SOmniCaptureControlPanel::FindCodecOption(EOmniCaptureCodec Codec) const
{
    for (const TSharedPtr<EOmniCaptureCodec>& Option : CodecOptions)
    {
        if (Option.IsValid() && *Option == Codec)
        {
            return Option;
        }
    }
    return CodecOptions.Num() > 0 ? CodecOptions[0] : nullptr;
}

TSharedPtr<EOmniCaptureColorFormat> SOmniCaptureControlPanel::FindColorFormatOption(EOmniCaptureColorFormat Format) const
{
    for (const TSharedPtr<EOmniCaptureColorFormat>& Option : ColorFormatOptions)
    {
        if (Option.IsValid() && *Option == Format)
        {
            return Option;
        }
    }
    return ColorFormatOptions.Num() > 0 ? ColorFormatOptions[0] : nullptr;
}

TSharedPtr<EOmniCaptureProjection> SOmniCaptureControlPanel::FindProjectionOption(EOmniCaptureProjection Projection) const
{
    for (const TSharedPtr<EOmniCaptureProjection>& Option : ProjectionOptions)
    {
        if (Option.IsValid() && *Option == Projection)
        {
            return Option;
        }
    }
    return ProjectionOptions.Num() > 0 ? ProjectionOptions[0] : nullptr;
}

TSharedPtr<EOmniCaptureImageFormat> SOmniCaptureControlPanel::FindImageFormatOption(EOmniCaptureImageFormat Format) const
{
    for (const TSharedPtr<EOmniCaptureImageFormat>& Option : ImageFormatOptions)
    {
        if (Option.IsValid() && *Option == Format)
        {
            return Option;
        }
    }
    return ImageFormatOptions.Num() > 0 ? ImageFormatOptions[0] : nullptr;
}

void SOmniCaptureControlPanel::HandleOutputFormatChanged(TSharedPtr<EOmniOutputFormat> NewFormat, ESelectInfo::Type SelectInfo)
{
    if (NewFormat.IsValid())
    {
        ApplyOutputFormat(*NewFormat);
    }
}

void SOmniCaptureControlPanel::HandleCodecChanged(TSharedPtr<EOmniCaptureCodec> NewCodec, ESelectInfo::Type SelectInfo)
{
    if (NewCodec.IsValid())
    {
        ApplyCodec(*NewCodec);
    }
}

void SOmniCaptureControlPanel::HandleColorFormatChanged(TSharedPtr<EOmniCaptureColorFormat> NewFormat, ESelectInfo::Type SelectInfo)
{
    if (NewFormat.IsValid())
    {
        ApplyColorFormat(*NewFormat);
    }
}

void SOmniCaptureControlPanel::HandleImageFormatChanged(TSharedPtr<EOmniCaptureImageFormat> NewFormat, ESelectInfo::Type SelectInfo)
{
    if (NewFormat.IsValid())
    {
        ApplyImageFormat(*NewFormat);
    }
}

TSharedRef<SWidget> SOmniCaptureControlPanel::GenerateImageFormatOption(TSharedPtr<EOmniCaptureImageFormat> InValue) const
{
    const EOmniCaptureImageFormat Format = InValue.IsValid() ? *InValue : EOmniCaptureImageFormat::PNG;
    return SNew(STextBlock).Text(ImageFormatToText(Format));
}

int32 SOmniCaptureControlPanel::GetTargetBitrate() const
{
    return GetSettingsSnapshot().Quality.TargetBitrateKbps;
}

int32 SOmniCaptureControlPanel::GetMaxBitrate() const
{
    return GetSettingsSnapshot().Quality.MaxBitrateKbps;
}

int32 SOmniCaptureControlPanel::GetGOPLength() const
{
    return GetSettingsSnapshot().Quality.GOPLength;
}

int32 SOmniCaptureControlPanel::GetBFrameCount() const
{
    return GetSettingsSnapshot().Quality.BFrames;
}

void SOmniCaptureControlPanel::HandleTargetBitrateCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
    ModifyCaptureSettings([NewValue](FOmniCaptureSettings& Settings)
    {
        Settings.Quality.TargetBitrateKbps = FMath::Clamp(NewValue, 1000, 1'500'000);
    });
}

void SOmniCaptureControlPanel::HandleMaxBitrateCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
    ModifyCaptureSettings([NewValue](FOmniCaptureSettings& Settings)
    {
        Settings.Quality.MaxBitrateKbps = FMath::Clamp(NewValue, 1000, 1'500'000);
    });
}

void SOmniCaptureControlPanel::HandleGOPCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
    ModifyCaptureSettings([NewValue](FOmniCaptureSettings& Settings)
    {
        Settings.Quality.GOPLength = FMath::Clamp(NewValue, 1, 600);
    });
}

void SOmniCaptureControlPanel::HandleBFramesCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
    ModifyCaptureSettings([NewValue](FOmniCaptureSettings& Settings)
    {
        Settings.Quality.BFrames = FMath::Clamp(NewValue, 0, 8);
    });
}

ECheckBoxState SOmniCaptureControlPanel::GetZeroCopyState() const
{
    return GetSettingsSnapshot().bZeroCopy ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SOmniCaptureControlPanel::HandleZeroCopyChanged(ECheckBoxState NewState)
{
    ModifyCaptureSettings([NewState](FOmniCaptureSettings& Settings)
    {
        Settings.bZeroCopy = (NewState == ECheckBoxState::Checked);
    });
}

ECheckBoxState SOmniCaptureControlPanel::GetFastStartState() const
{
    return GetSettingsSnapshot().bEnableFastStart ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SOmniCaptureControlPanel::HandleFastStartChanged(ECheckBoxState NewState)
{
    ModifyCaptureSettings([NewState](FOmniCaptureSettings& Settings)
    {
        Settings.bEnableFastStart = (NewState == ECheckBoxState::Checked);
    });
}

ECheckBoxState SOmniCaptureControlPanel::GetConstantFrameRateState() const
{
    return GetSettingsSnapshot().bForceConstantFrameRate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SOmniCaptureControlPanel::HandleConstantFrameRateChanged(ECheckBoxState NewState)
{
    ModifyCaptureSettings([NewState](FOmniCaptureSettings& Settings)
    {
        Settings.bForceConstantFrameRate = (NewState == ECheckBoxState::Checked);
    });
}

#undef LOCTEXT_NAMESPACE
