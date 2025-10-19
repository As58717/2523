#pragma once

// ── UTextureRenderTarget2D ───────────────────────────────────
#if __has_include("Engine/TextureRenderTarget2D.h")
    #include "Engine/TextureRenderTarget2D.h"
#elif __has_include("TextureRenderTarget2D.h")
    #include "TextureRenderTarget2D.h"
#else
    class UTextureRenderTarget2D;
#endif

// ── FTextureRenderTargetResource ───────────────────────
#if __has_include("Engine/TextureRenderTargetResource.h")
    #include "Engine/TextureRenderTargetResource.h"
#elif __has_include("TextureRenderTargetResource.h")
    #include "TextureRenderTargetResource.h"
#else
    class FTextureRenderTargetResource;
#endif
