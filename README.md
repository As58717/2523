# OmniCapture Feature Overview

OmniCapture extends Unreal Engine with a turnkey panoramic capture workflow that targets 2D, VR180, and full 360° deliveries while remaining friendly to stereo use‑cases. The plugin now covers production requests such as ray/path tracing enablement, wide projection presets, and fast image exports so that teams can drop it straight into cinematic or live capture pipelines.

## Supported capture projections

| Projection | Description |
| --- | --- |
| **Planar 2D** | Traditional frame capture for flat deliverables. |
| **Equirectangular 360°** | Full-sphere capture for monoscopic or stereoscopic content. |
| **Fisheye 180° / 360°** | GPU-accelerated fisheye projection with configurable FOV, stereo layouts, and optional equirectangular conversion for VR platforms. |
| **VR180** | Half-sphere output with the correct VR metadata baked in. |
| **Cylindrical** | Wraparound projection that preserves vertical framing for dome blends. |
| **Full Dome** | 180° domed capture optimized for planetarium style playback. |
| **Spherical Mirror** | Wide FOV mapping for mirror rigs and projector domes. |

Switch projections in the OmniCapture control panel—the panel automatically exposes the new fisheye, cylindrical, full-dome, and spherical-mirror choices alongside the original planar and equirectangular presets. The fisheye panel surfaces 180°/360° presets, per-eye resolution controls, GPU shader toggles, and a one-click equirect conversion option when you need flat deliverables. 【F:Plugins/OmniCapture/Source/OmniCaptureEditor/Private/SOmniCaptureControlPanel.cpp†L377-L478】【F:Plugins/OmniCapture/Source/OmniCapture/Public/OmniCaptureTypes.h†L94-L151】【F:Plugins/OmniCapture/Source/OmniCapture/Private/OmniCaptureEquirectConverter.cpp†L1081-L1454】【F:Plugins/OmniCapture/Shaders/Private/OmniFisheyeCS.usf†L1-L112】

## Stereo ergonomics

* **Dynamic IPD curves** – assign a `CurveFloat` to drive inter-pupillary distance over time and the rig updates before every frame, which is ideal for VR storytelling beats that need dramatic parallax shifts. 【F:Plugins/OmniCapture/Source/OmniCapture/Public/OmniCaptureTypes.h†L78-L133】【F:Plugins/OmniCapture/Source/OmniCapture/Private/OmniCaptureSubsystem.cpp†L1018-L1144】
* **Eye convergence distance** – set a per-shot focal distance (or animate it via curve) so planar stereo shots converge on hero talent without requiring external rig tweaks. 【F:Plugins/OmniCapture/Source/OmniCapture/Public/OmniCaptureTypes.h†L105-L133】【F:Plugins/OmniCapture/Source/OmniCapture/Private/OmniCaptureRigActor.cpp†L17-L148】

## Rendering feature overrides

The subsystem can toggle critical rendering tech before capture starts and restores the original engine settings afterward. Enabling options such as Ray Tracing, Path Tracing, Lumen, DLSS, Bloom, and anti-aliasing is as simple as flipping booleans on the capture settings struct, which makes it painless to align with high-end Movie Render Queue passes. 【F:Plugins/OmniCapture/Source/OmniCapture/Public/OmniCaptureTypes.h†L61-L88】【F:Plugins/OmniCapture/Source/OmniCapture/Private/OmniCaptureSubsystem.cpp†L1018-L1237】

## Gamma & color consistency

Color-managed captures help the recorded footage match the in-editor viewport. Choose between **sRGB** and **linear** gamma responses on each capture asset, which controls whether the rig samples the tonemapped buffer or the untouched HDR buffer and sets render target gamma accordingly. 【F:Plugins/OmniCapture/Source/OmniCapture/Public/OmniCaptureTypes.h†L61-L107】【F:Plugins/OmniCapture/Source/OmniCapture/Private/OmniCaptureRigActor.cpp†L279-L310】

When encoding to NVENC or writing BGRA buffers, the compute shader applies the appropriate gamma curve before packing pixels so the exported frames maintain the expected color space. 【F:Plugins/OmniCapture/Shaders/Private/OmniColorConvertCS.usf†L11-L137】

## Image export formats

OmniCapture exports linear or sRGB frames to **PNG**, **JPEG**, **EXR**, and **BMP** sequences. The PNG writer exposes 8-bit, 16-bit, and 32-bit color depth controls so you can balance fidelity and disk footprint per deliverable, the EXR path preserves the floating-point payload for alpha/stencil workflows, while the BMP exporter helps teams that require legacy offline review tools. 【F:Plugins/OmniCapture/Source/OmniCapture/Private/OmniCaptureImageWriter.cpp†L398-L545】【F:Plugins/OmniCapture/Source/OmniCaptureEditor/Private/SOmniCaptureControlPanel.cpp†L120-L279】

## How to use the new options

1. Open the OmniCapture control panel in-editor and select the desired projection or image format from the updated drop-down menus.
2. Toggle rendering features (ray tracing, DLSS, bloom, etc.) on the capture settings asset you feed into the subsystem.
3. Optional: assign curves for dynamic IPD and convergence if you want the stereo rig to react over time.
4. Kick off a capture session—the subsystem applies the rendering overrides, updates stereo transforms every frame, and restores the engine state when finished. 【F:Plugins/OmniCapture/Source/OmniCapture/Private/OmniCaptureSubsystem.cpp†L80-L281】【F:Plugins/OmniCapture/Source/OmniCapture/Private/OmniCaptureSubsystem.cpp†L1018-L1237】

These upgrades keep the plugin focused on ease of operation while covering the feature set typically required for immersive, stereo, and dome deliverables.
