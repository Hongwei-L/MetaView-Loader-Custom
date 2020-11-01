// Copyright (c) 2020, Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/Providers/Display/Display.cpp
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#include "Display.h"

#include "Metadata.h"
#include "Model/Log.h"
#include "Util.h"

#include "Model/DirectDisplayManager.h"
#include "Model/DisplayDetection.h"

#include "CommonTypes.h"
#include "Input/Input.h"
#include "Model/InitErrors.h"
#include "OpenVRProviderContext.h"
#include "ProviderInterface/IUnityXRStats.h"
#include "ProviderInterface/UnityXRDisplayStats.h"
#include "ProviderInterface/XRMath.h"
#include "UnityInterfaces.h"
#include "UserProjectSettings.h"

#ifdef __linux__
#include <cstring>
#endif

// #define REALORTHO
using namespace metaview;

//! @todo this was taken from the rig, but the aspect ratio doesn't seem to
//! match the pixel size
constexpr float AspectRatio = 2.00683f;
// Distance from center to top, as well as distance from center to viewer.
constexpr float OrthoHalfSize = 2.5f;  // 0.1f;

constexpr float PerspectiveHalfAngleHeightTangent = 0.845065f;
constexpr float PerspectiveHalfAngleWidthTangent = 0.939062f;

//! @todo these are placeholder values
constexpr UnityXRProjectionHalfAngles LeftFov = {
    -PerspectiveHalfAngleWidthTangent, PerspectiveHalfAngleWidthTangent,
    PerspectiveHalfAngleHeightTangent, -PerspectiveHalfAngleHeightTangent};
constexpr UnityXRProjectionHalfAngles RightFov = {
    -PerspectiveHalfAngleWidthTangent, PerspectiveHalfAngleWidthTangent,
    PerspectiveHalfAngleHeightTangent, -PerspectiveHalfAngleHeightTangent};

// This can be updated during runtime.
static UnityXRProjectionHalfAngles SingleCamFov = LeftFov;

// Interfaces
static IUnityXRDisplayInterface *s_pXRDisplay = nullptr;
static IUnityXRStats *s_pXRStats;
static UnitySubsystemHandle s_DisplayHandle;
static OpenVRProviderContext *s_pProviderContext;

// Han Custom Code
// this IPD can be changed in runtime.
static float SinglePassInstancedCamIPD = NominalIpd;

static UnityXRProjectionHalfAngles LeftFovRuntime = LeftFov;

static UnityXRProjectionHalfAngles RightFovRuntime = RightFov;

// XR Stats
static UnityXRStatId m_nNumDroppedFrames;  // number of additional times
                                           // previous frame was scanned out
static UnityXRStatId
    m_nNumFramePresents;  // number of times this frame was presented
static UnityXRStatId
    m_flFrameTimeReference;  // absolute time reference for comparing
                             // frames.This aligns with the vsync that running
                             // start is relative to.
static UnityXRStatId
    m_flGPURenderTimeInMs;  // time between work submitted immediately after
                            // present (ideally vsync) until the end of
                            // compositor submitted work
static UnityXRStatId m_flCPURenderTimeInMs;  // time spent on cpu submitting the
                                             // above work for this frame
static UnityXRStatId
    m_flCPUIdelTimeInMs;  // time spent waiting for running start (application
                          // could have used this much more time)
static UnityXRStatId
    m_flCompositorRenderTimeInMs;  // time spend performing distortion
                                   // correction, rendering chaperone, overlays,
                                   // etc.

static UnitySubsystemErrorCode UNITY_INTERFACE_API
GfxThread_Start(UnitySubsystemHandle handle, void *userData,
                UnityXRRenderingCapabilities *renderingCaps) {
    OpenVRDisplayProvider *pDisplay = (OpenVRDisplayProvider *)userData;

    return pDisplay->GfxThread_Start(renderingCaps);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
GfxThread_PopulateNextFrameDesc(UnitySubsystemHandle handle, void *userData,
                                const UnityXRFrameSetupHints *frameHints,
                                UnityXRNextFrameDesc *nextFrame) {
    OpenVRDisplayProvider *pDisplay = (OpenVRDisplayProvider *)userData;

    return pDisplay->GfxThread_PopulateNextFrameDesc(frameHints, nextFrame);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
GfxThread_SubmitCurrentFrame(UnitySubsystemHandle handle, void *userData) {
    OpenVRDisplayProvider *pDisplay = (OpenVRDisplayProvider *)userData;

    return pDisplay->GfxThread_SubmitCurrentFrame();
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
GfxThread_Stop(UnitySubsystemHandle handle, void *userData) {
    OpenVRDisplayProvider *pDisplay = (OpenVRDisplayProvider *)userData;

    return pDisplay->GfxThread_Stop();
}

// Callback executed for rendering to editor preview
static UnitySubsystemErrorCode UNITY_INTERFACE_API
GfxThread_BlitToMirrorViewRenderTarget(
    UnitySubsystemHandle handle, void *userData,
    const UnityXRMirrorViewBlitInfo mirrorBlitInfo) {
    OpenVRDisplayProvider *pDisplay = (OpenVRDisplayProvider *)userData;

    return pDisplay->GfxThread_BlitToMirrorViewRenderTarget(&mirrorBlitInfo);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
MainThread_UpdateDisplayState(UnitySubsystemHandle handle, void *userData,
                              UnityXRDisplayState *state) {
    OpenVRDisplayProvider *pDisplay = (OpenVRDisplayProvider *)userData;

    return pDisplay->MainThread_UpdateDisplayState(state);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
MainThread_QueryMirrorViewBlitDesc(
    UnitySubsystemHandle pHandle, void *pUserData,
    const UnityXRMirrorViewBlitInfo pMirrorBlitInfo,
    UnityXRMirrorViewBlitDesc *pBlitDescriptor) {
    OpenVRDisplayProvider *pDisplay = (OpenVRDisplayProvider *)pUserData;

    return pDisplay->MainThread_QueryMirrorViewBlitDesc(
        &pMirrorBlitInfo, pBlitDescriptor, pDisplay);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
SetProjectionParamsForSingleCamera(float left, float right, float top,
                                   float bottom) {
    XR_TRACE(PLUGIN_LOG_PREFIX
             "Given project parameters from managed code: L: %f  R: %f  T: %f  "
             "B: %f\n",
             left, right, top, bottom);
    SingleCamFov.left = left;
    SingleCamFov.right = right;
    SingleCamFov.top = top;
    SingleCamFov.bottom = bottom;
}

// Callback executed when a subsystem should initialize in preparation for
// becoming active.
static UnitySubsystemErrorCode UNITY_INTERFACE_API
Lifecycle_Initialize(UnitySubsystemHandle handle, void *userData) {
    OpenVRSystem::Get().Initialize();

    OpenVRDisplayProvider *pDisplay = (OpenVRDisplayProvider *)userData;

    return pDisplay->Lifecycle_Initialize(handle, userData);
}

// Callback executed when a subsystem should become active.
static UnitySubsystemErrorCode UNITY_INTERFACE_API
Lifecycle_Start(UnitySubsystemHandle handle, void *userData) {
    OpenVRDisplayProvider *pDisplay = (OpenVRDisplayProvider *)userData;

    return pDisplay->Lifecycle_Start(handle);
}

// Callback executed when a subsystem should become inactive.
static void UNITY_INTERFACE_API Lifecycle_Stop(UnitySubsystemHandle handle,
                                               void *userData) {
    OpenVRDisplayProvider *pDisplay = (OpenVRDisplayProvider *)userData;

    pDisplay->Lifecycle_Stop(handle);
}

// Callback executed when a subsystem should release all resources and is about
// to be unloaded.
static void UNITY_INTERFACE_API Lifecycle_Shutdown(UnitySubsystemHandle handle,
                                                   void *userData) {
    OpenVRDisplayProvider *pDisplay = (OpenVRDisplayProvider *)userData;

    pDisplay->Lifecycle_Shutdown(handle);

    OpenVRSystem::Get().Shutdown();
}

OpenVRDisplayProvider::OpenVRDisplayProvider()
    : m_nCurFrame(0), m_bFrameInFlight(false), m_bTexturesCreated(false) {
    XR_TRACE(PLUGIN_LOG_PREFIX "Display Provider created\n");
    for (int i = 0; i < k_nMaxNumStages; ++i) {
        for (int j = 0; j < 2; ++j) {
            m_pNativeColorTextures[i][j] = nullptr;
            m_pNativeDepthTextures[i][j] = nullptr;
            m_UnityTextures[i][j] = 0;
        }
    }
}

OpenVRDisplayProvider::~OpenVRDisplayProvider() {}

UnitySubsystemErrorCode OpenVRDisplayProvider::Lifecycle_Initialize(
    UnitySubsystemHandle handle, void *userData) {
    XR_TRACE(PLUGIN_LOG_PREFIX "Display Lifecycle Initialize\n");
    // Register handles
    s_DisplayHandle = handle;
    if (s_pProviderContext) {
        s_pProviderContext->displayProvider = this;
    }

    // Register for callbacks on the graphics thread.
    UnityXRDisplayGraphicsThreadProvider gfxThreadProvider = {
        this,
        &::GfxThread_Start,
        &::GfxThread_SubmitCurrentFrame,
        &::GfxThread_PopulateNextFrameDesc,
        &::GfxThread_Stop,
        &::GfxThread_BlitToMirrorViewRenderTarget,
    };
    if (s_pXRDisplay->RegisterProviderForGraphicsThread(
            handle, &gfxThreadProvider) != kUnitySubsystemErrorCodeSuccess)
        return kUnitySubsystemErrorCodeFailure;

    // Register for callbacks on the main thread.
    UnityXRDisplayProvider mainThreadProvider = {
        this, &::MainThread_UpdateDisplayState,
        &::MainThread_QueryMirrorViewBlitDesc};

    if (s_pXRDisplay->RegisterProvider(handle, &mainThreadProvider))
        return kUnitySubsystemErrorCodeFailure;

    return kUnitySubsystemErrorCodeSuccess;
}

UnitySubsystemErrorCode OpenVRDisplayProvider::Lifecycle_Start(
    UnitySubsystemHandle handle) {
    XR_TRACE_LOG(XR_TRACE_PTR, PLUGIN_LOG_PREFIX "XR Display Start\n");
    OpenVRSystem::Get();

    m_nOpenVRMirrorAttempts = 0;
    m_bOverlayFallback = false;

    SetMirrorMode(UserProjectSettings::GetUnityMirrorViewMode());

    // Reset previous mirror mode
    m_nPrevMirrorMode = m_nMirrorMode;

    // Setup mirror subrect defaults
    SetupMirror();

    return kUnitySubsystemErrorCodeSuccess;
}

void OpenVRDisplayProvider::Lifecycle_Stop(UnitySubsystemHandle handle) {
    XR_TRACE_LOG(XR_TRACE_PTR, PLUGIN_LOG_PREFIX "XR Display Stop\n");

#if 0
    // Unregister XR Stats
    if (s_pXRStats) {
        s_pXRStats->UnregisterStatSource(handle);
    }
#endif

    m_bFrameInFlight = false;
}

void OpenVRDisplayProvider::Lifecycle_Shutdown(UnitySubsystemHandle handle) {
    XR_TRACE_LOG(XR_TRACE_PTR, PLUGIN_LOG_PREFIX "XR Display Shutdown\n");

    // Destroy all eye textures
    DestroyEyeTextures(handle);

    swapchainImages_.clear();
    rtvs_.clear();
    renderer_.reset();

    // Explicitly reset member vars as Unity holds on to them in-between editor
    // runs
    m_nCurFrame = 0;
    m_bTexturesCreated = false;
    m_bIsUsingCustomMirrorMode = false;
    m_bIsSteamVRViewAvailable = false;
    m_bIsIncorrectTexture = false;
    m_bIsHeadsetResolutionSet = false;
    OpenVRSystem::Get().SetTickCallback(nullptr);
}

UnitySubsystemErrorCode OpenVRDisplayProvider::GfxThread_Start(
    UnityXRRenderingCapabilities *renderingCaps) {
    // Set active graphics renderer
    IUnityGraphics *pUnityGraphics =
        UnityInterfaces::Get().GetInterface<IUnityGraphics>();

    switch (pUnityGraphics->GetRenderer()) {
        case kUnityGfxRendererD3D11:
            m_eActiveTextureType = vr::TextureType_DirectX;
            break;

        default:
            XR_TRACE_ERROR(XR_TRACE_PTR, PLUGIN_LOG_PREFIX
                           "[Error] Unsupported graphics api! Only "
                           "DirectX11 is supported at this time.");
            return kUnitySubsystemErrorCodeFailure;
            break;
    }

    // Check if we are using single pass
    m_bUseSinglePass = UserProjectSettings::GetStereoRenderingMode() ==
                           EVRStereoRenderingModes::SinglePassInstanced ||
                       UserProjectSettings::GetStereoRenderingMode() ==
                           EVRStereoRenderingModes::SingleCamera;
    m_renderingMode = UserProjectSettings::GetStereoRenderingMode();

    // Setup rendering capabilities
    renderingCaps->noSinglePassRenderingSupport = false;
    renderingCaps->invalidateRenderStateAfterEachCallback = true;

    // Get occlusion mesh/hidden area mesh for both eyes (if available)
    // m_pOcclusionMeshLeftEye = SetupOcclusionMesh(Eye::Left);
    // m_pOcclusionMeshRightEye = SetupOcclusionMesh(Eye::Right);

    if (renderer_) {
        // We already brought up the renderer
        return kUnitySubsystemErrorCodeSuccess;
    }
    try {
        auto &displayDetection = OpenVRSystem::Get().GetDisplayDetection();
        auto &directDisplayManager =
            OpenVRSystem::Get().GetDirectDisplayManager();
        OpenVRSystem::Get().ReEnumerateDisplaysIfNeeded();
        if (displayDetection.getStatus() != InitErrors::None) {
            XR_TRACE_ERROR(XR_TRACE_PTR,
                           PLUGIN_LOG_PREFIX "We had an error!\n");
            return kUnitySubsystemErrorCodeFailure;
        }
        auto renderParam = directDisplayManager.setUpDirectDisplay(
            *displayDetection.getDisplay());
        auto unityD3D11 =
            UnityInterfaces::Get().GetInterface<IUnityGraphicsD3D11>();

        renderer_ = std::make_unique<metaview::Renderer>(
            std::move(renderParam), 2, unityD3D11->GetDevice());

        swapchainImages_ = renderer_->getSwapchainImages();
        rtvs_ = renderer_->getSwapchainRTVs();
    } catch (std::exception const &e) {
        XR_TRACE_ERROR(XR_TRACE_PTR, PLUGIN_LOG_PREFIX "Exception: %s\n",
                       e.what());
        return kUnitySubsystemErrorCodeFailure;
    }

    return kUnitySubsystemErrorCodeSuccess;
}

void OpenVRDisplayProvider::TryUpdateMirrorMode(bool skipResolutionCheck) {
    // Disregard mirror mode changes in the first frame
    if (m_bIsHeadsetResolutionSet || skipResolutionCheck) {
        int requestedMirrorMode = UserProjectSettings::GetUnityMirrorViewMode();

        if (requestedMirrorMode == kUnityXRMirrorBlitDistort &&
            m_bOverlayFallback) {
            // don't try to set it again if we've already failed
        } else if (requestedMirrorMode != GetCurrentMirrorMode()) {
            SetMirrorMode(requestedMirrorMode);
            SetupMirror();
        } else if (m_nMirrorMode == kUnityXRMirrorBlitDistort) {
            if ((!m_bIsUsingCustomMirrorMode &&
                 m_bIsHeadsetResolutionSet)  // We need to pick up the shared
                                             // texture if we're on SteamVR View
                                             // mode after the first frame
                || !HasOverlayPointer())     // If getting the shared texture
                                             // failed once, retry
            {
                SetupMirror();
            }
        }
    }

    m_nPrevMirrorMode = m_nMirrorMode;
}

UnitySubsystemErrorCode OpenVRDisplayProvider::GfxThread_PopulateNextFrameDesc(
    const UnityXRFrameSetupHints *frameHints, UnityXRNextFrameDesc *nextFrame) {
    UnitySubsystemErrorCode ret = kUnitySubsystemErrorCodeSuccess;
    s_pProviderContext->inputProvider->GfxThread_UpdateDevices();
    m_bIsUsingSRGB = frameHints->appSetup.sRGB;
    m_bRotateEyes = UserProjectSettings::RotateEyes();

    TryUpdateMirrorMode();

    // Check if engine requested a change of the viewport
    if ((kUnityXRFrameSetupHintsChangedRenderViewport &
         frameHints->changedFlags) != 0) {
        // Set new texture bounds and mirror subrect
        m_textureBounds.uMin = frameHints->appSetup.renderViewport.x;
        m_textureBounds.vMin = frameHints->appSetup.renderViewport.y;
        m_textureBounds.uMax = frameHints->appSetup.renderViewport.width;
        m_textureBounds.vMax = frameHints->appSetup.renderViewport.height;

        // Re-init mirror
        m_bIsRenderViewportScaling = false;
        SetupMirror();

        // Set render viewport scaling status
        if (frameHints->appSetup.renderViewport.x > 0.0f ||
            frameHints->appSetup.renderViewport.y > 0.0f ||
            frameHints->appSetup.renderViewport.width < 1.0f ||
            frameHints->appSetup.renderViewport.height < 1.0f) {
            // Set mirror subrect from render target location
            m_mirrorRenderSubRect.x *=
                frameHints->appSetup.renderViewport.width;
            m_mirrorRenderSubRect.y *=
                frameHints->appSetup.renderViewport.height;
            m_mirrorRenderSubRect.width *=
                frameHints->appSetup.renderViewport.width;
            m_mirrorRenderSubRect.height *=
                frameHints->appSetup.renderViewport.height;

            m_bIsRenderViewportScaling = true;
        }
    }

    // Check if there was a request to change the texture resolution scale
    if ((kUnityXRFrameSetupHintsChangedTextureResolutionScale &
         frameHints->changedFlags) != 0) {
        if (m_bTexturesCreated && s_DisplayHandle)
            DestroyEyeTextures(s_DisplayHandle);

        m_bTexturesCreated = false;
    }

    if (!m_bTexturesCreated) {
        ret = CreateEyeTextures(frameHints);
    }

    if (renderer_) {
        swapchainImageIndex_ = renderer_->waitFrame();

        auto rtv = rtvs_[swapchainImageIndex_];
        float clearColor[4] = {0.7f, 0.2f, 0.2f, 1.f};
        renderer_->getImmediateContext()->ClearRenderTargetView(rtv.get(),
                                                                clearColor);
    }
    if (m_renderingMode == EVRStereoRenderingModes::SingleCamera &&
        !frameHints->appSetup.singlePassRendering) {
        XR_TRACE_WARNING(
            XR_TRACE_PTR, PLUGIN_LOG_PREFIX
            "Single Camera mode requires single-pass-capable shaders");
    }

    // Calculate culling frustum
    if (frameHints->appSetup.singlePassRendering ||
        m_renderingMode == EVRStereoRenderingModes::SingleCamera) {
        // Single pass
        SetupCullingPass(EEye::CenterOrBoth, frameHints,
                         nextFrame->cullingPasses[0]);
    } else {
        // Multi-pass
        SetupCullingPass(EEye::Left, frameHints, nextFrame->cullingPasses[0]);
        SetupCullingPass(EEye::Right, frameHints, nextFrame->cullingPasses[1]);
    }

    if (m_renderingMode == EVRStereoRenderingModes::SingleCamera) {
        SetupRenderPass(EEye::CenterOrBoth, frameHints, nextFrame);
    } else {
        SetupRenderPass(EEye::Left, frameHints, nextFrame);
        SetupRenderPass(EEye::Right, frameHints, nextFrame);
    }

    m_bFrameInFlight = true;

#if 0
    // Set frame stats
    if (s_pXRStats) {
        vr::Compositor_FrameTiming pTiming = {};
        pTiming.m_nSize = sizeof(vr::Compositor_FrameTiming);

        if (vr::VRCompositor() &&
            vr::VRCompositor()->GetFrameTiming(&pTiming, 0)) {
            // Unity XRStats in this version only exposes floats
            // Since we are in the gfx thread, we do NOT need to call increment
            // frame here for XRStats
            s_pXRStats->SetStatFloat(m_nNumDroppedFrames,
                                     (float)pTiming.m_nNumDroppedFrames);
            s_pXRStats->SetStatFloat(m_nNumFramePresents,
                                     (float)pTiming.m_nNumFramePresents);
            s_pXRStats->SetStatFloat(m_flFrameTimeReference,
                                     (float)pTiming.m_flSystemTimeInSeconds);
            s_pXRStats->SetStatFloat(m_flGPURenderTimeInMs,
                                     pTiming.m_flTotalRenderGpuMs);
            s_pXRStats->SetStatFloat(m_flCPURenderTimeInMs,
                                     pTiming.m_flCompositorRenderCpuMs);
            s_pXRStats->SetStatFloat(m_flCPUIdelTimeInMs,
                                     pTiming.m_flCompositorIdleCpuMs);
            s_pXRStats->SetStatFloat(m_flCompositorRenderTimeInMs,
                                     pTiming.m_flCompositorRenderGpuMs);
        }
    }
#endif

    return ret;
}

void OpenVRDisplayProvider::SubmitToRenderer(int stage) {
    if (!renderer_) {
        return;
    }
    auto rtv = rtvs_[swapchainImageIndex_];
    auto texture = swapchainImages_[swapchainImageIndex_];
    auto blitIt = [&](int nTexIndex, int subresource, UINT dstx, UINT dsty) {
        ID3D11Texture2D *src = static_cast<ID3D11Texture2D *>(
            GetNativeEyeTexture(stage, nTexIndex));
        // copy the entire eye texture to our output texture.
        this->renderer_->getImmediateContext()->CopySubresourceRegion(
            texture.get(), 0, dstx, dsty, 0, src, subresource, nullptr);
    };
    uint32_t height = 0, width = 0;
    GetEyeTextureDimensions(height, width);
    switch (m_renderingMode) {
        case EVRStereoRenderingModes::MultiPass:
            // left eye
            blitIt(0, 0, 0, 0);
            // right eye
            blitIt(1, 0, width, 0);
            break;
        case EVRStereoRenderingModes::SinglePassInstanced:
            // Use the left eye texture if we're doing a single pass
            // left eye
            blitIt(0, 0, 0, 0);
            // right eye
            blitIt(0, 1, width, 0);
            break;
        case EVRStereoRenderingModes::SingleCamera:
            // everything in the first layer of the left eye
            blitIt(0, 0, 0, 0);
            break;
    }
    renderer_->endFrame();
}

UnitySubsystemErrorCode OpenVRDisplayProvider::GfxThread_SubmitCurrentFrame() {
    if (!m_bFrameInFlight) return kUnitySubsystemErrorCodeSuccess;

    // Get the stage for the current frame
    int stage = m_nCurFrame % m_nNumStages;

    // Advance frame number
    m_nCurFrame = (m_nCurFrame < UINT32_MAX) ? m_nCurFrame + 1 : 0;

    // Tell the compositor it can start rendering immediately
    SubmitToRenderer(stage);

#if 0
    // Set the mirror resolution - should be done only ONCE and after at least
    // ONE FRAME has been submitted
    if (!m_bIsHeadsetResolutionSet && !m_bOverlayFallback &&
        GetCurrentMirrorMode() == kUnityXRMirrorBlitDistort &&
        vr::VRHeadsetView()) {
        // Get recommended mirror resolution
        UnityXRVector2 mirrorResolution = GetRecommendedMirrorResolution();
        uint32_t nRecommendedMirrorWidth = (uint32_t)mirrorResolution.x;
        uint32_t nRecommendedMirrorHeight = (uint32_t)mirrorResolution.y;

        // Get current mirror resolution
        uint32_t nCurrentMirrorWidth, nCurrentMirrorHeight;
        vr::VRHeadsetView()->GetHeadsetViewSize(&nCurrentMirrorWidth,
                                                &nCurrentMirrorHeight);

        // Set headset resolution if needed
        if (nCurrentMirrorWidth != nRecommendedMirrorWidth ||
            nCurrentMirrorHeight != nRecommendedMirrorHeight) {
            vr::VRHeadsetView()->SetHeadsetViewSize(nRecommendedMirrorWidth,
                                                    nRecommendedMirrorHeight);
            vr::VRHeadsetView()->SetHeadsetViewCropped(true);
            XR_TRACE(PLUGIN_LOG_PREFIX
                     "[Mirror] Setting mirror view to %ix%i\n",
                     nRecommendedMirrorWidth, nRecommendedMirrorHeight);
        }

        m_bIsHeadsetResolutionSet = true;
        vr::VRHeadsetView()->GetHeadsetViewSize(&nCurrentMirrorWidth,
                                                &nCurrentMirrorHeight);
        XR_TRACE(PLUGIN_LOG_PREFIX "[Mirror] Mirror view set to %ix%i\n",
                 nCurrentMirrorWidth, nCurrentMirrorHeight);
    }
#endif

    // Returning anything other than success here will shutdown the display
    // provider (Unity SDK behavior)
    return kUnitySubsystemErrorCodeSuccess;
}

UnitySubsystemErrorCode
OpenVRDisplayProvider::GfxThread_BlitToMirrorViewRenderTarget(
    const UnityXRMirrorViewBlitInfo *mirrorBlitInfo) {
#ifndef __linux__
    // Set RTV
    if (XR_WIN && m_eActiveTextureType == vr::TextureType_DirectX) {
        ID3D11RenderTargetView *pRTV =
            UnityInterfaces::Get()
                .GetInterface<IUnityGraphicsD3D11>()
                ->RTVFromRenderBuffer(mirrorBlitInfo->mirrorRtDesc->rtNative);
        ID3D11DeviceContext *pImmediateContext;
        ((ID3D11Device *)m_pRenderDevice)
            ->GetImmediateContext(&pImmediateContext);
        pImmediateContext->OMSetRenderTargets(1, &pRTV, NULL);

        // Clear out RTV
        const FLOAT clrColor[4] = {0, 0, 0, 0};
        pImmediateContext->ClearRenderTargetView(pRTV, clrColor);
    }
#endif

    return kUnitySubsystemErrorCodeSuccess;
}

UnitySubsystemErrorCode OpenVRDisplayProvider::GfxThread_Stop() {
    m_nCurFrame = 0;

    // Clean-up occlusion meshes
    if (m_pOcclusionMeshLeftEye != 0)
        s_pXRDisplay->DestroyOcclusionMesh(s_DisplayHandle,
                                           m_pOcclusionMeshLeftEye);

    if (m_pOcclusionMeshRightEye != 0)
        s_pXRDisplay->DestroyOcclusionMesh(s_DisplayHandle,
                                           m_pOcclusionMeshRightEye);

    renderer_->blankScreen();
    return kUnitySubsystemErrorCodeSuccess;
}

UnitySubsystemErrorCode OpenVRDisplayProvider::MainThread_UpdateDisplayState(
    UnityXRDisplayState *state) {
    state->displayIsTransparent = true;

    state->reprojectionMode = kUnityXRReprojectionModeNone;
    state->focusLost = false;

    return kUnitySubsystemErrorCodeSuccess;
}

UnitySubsystemErrorCode
OpenVRDisplayProvider::MainThread_QueryMirrorViewBlitDesc(
    const UnityXRMirrorViewBlitInfo *pMirrorBlitInfo,
    UnityXRMirrorViewBlitDesc *pBlitDescriptor,
    OpenVRDisplayProvider *pDisplay) {
    return kUnitySubsystemErrorCodeFailure;
#if 0
    // Ensure that we can access the Overlay interface
    if (!vr::VROverlay()) return kUnitySubsystemErrorCodeFailure;

    TryUpdateMirrorMode();

    // Set mirror subrect and aspect ratio for the steamvr mirror view
    float flSourceAspect =
        (m_nEyeMirrorWidth * m_mirrorRenderSubRect.width) /
        (m_nRenderMirrorHeight * m_mirrorRenderSubRect.height);
    if (m_nMirrorMode == kUnityXRMirrorBlitDistort &&
        m_bIsSteamVRViewAvailable && m_bIsUsingCustomMirrorMode &&
        m_bIsHeadsetResolutionSet) {
        // Full subrect if there's a valid mirror view
        m_mirrorRenderSubRect = {0.0f, 0.0f, 1.0f, 1.0f};
        flSourceAspect = static_cast<float>(m_nRenderMirrorWidth) /
                         static_cast<float>(m_nRenderMirrorHeight);
    }

    // Set default mirror blit
    int stage = m_nCurFrame % m_nNumStages;
    int32_t nTextureArraySlice = 0;
    m_pMirrorTexture = m_UnityTextures[stage][0];

    // Check the current mirror mode
    if (m_bIsHeadsetResolutionSet &&
        m_nMirrorMode == kUnityXRMirrorBlitDistort &&
        m_bIsUsingCustomMirrorMode && HasOverlayPointer()) {
        // SteamVR View - Grab the Overlay view for the SteamVR VR view
        vr::EVROverlayError eOverlayError =
            vr::VROverlayView()->AcquireOverlayView(m_hOverlay, &m_nativeDevice,
                                                    &m_overlayView,
                                                    sizeof(m_overlayView));
        if (eOverlayError != vr::VROverlayError_None) {
            XR_TRACE(PLUGIN_LOG_PREFIX
                     "[Mirror] Fatal. Unable to acquire the SteamVR "
                     "Display VR View overlay this frame [%i]\n",
                     eOverlayError);
            return kUnitySubsystemErrorCodeFailure;
        }

        // Set the mirror texture to the SteamVR mirror texture
        m_pMirrorTexture = m_pSteamVRTextureId;
    } else {
        // Right eye texture (left is default)
        if (m_nMirrorMode == kUnityXRMirrorBlitRightEye) {
            m_pMirrorTexture = m_UnityTextures[stage][m_bUseSinglePass ? 0 : 1];
            nTextureArraySlice = m_bUseSinglePass ? 1 : 0;
        }
    }

    // Get destination texture size and UV from current Editor/Player Window
    // scale
    UnityXRVector2 vSourceUV0, vSourceUV1, vDestUV0, vDestUV1;
    const UnityXRVector2 vDestTextureSize = {
        static_cast<float>(pMirrorBlitInfo->mirrorRtDesc->rtScaledWidth),
        static_cast<float>(pMirrorBlitInfo->mirrorRtDesc->rtScaledHeight)};
    const UnityXRRectf vDestUVRect = (m_nMirrorMode == kUnityXRMirrorBlitNone)
                                         ? UnityXRRectf{0.0f}
                                         : UnityXRRectf{0.0f, 0.0f, 1.0f, 1.0f};

    // Calculate aspect ratios
    float flDestAspect = (vDestTextureSize.x * vDestUVRect.width) /
                         (vDestTextureSize.y * vDestUVRect.height);
    float flRatio = flSourceAspect / flDestAspect;

    // Adjust sub rects to fit texture into editor preview/player window
    UnityXRVector2 vSourceUVCenter = {
        m_mirrorRenderSubRect.x + m_mirrorRenderSubRect.width * 0.5f,
        m_mirrorRenderSubRect.y + m_mirrorRenderSubRect.height * 0.5f};
    UnityXRVector2 vSourceUVSize = {m_mirrorRenderSubRect.width,
                                    m_mirrorRenderSubRect.height};
    UnityXRVector2 vDestUVCenter = {vDestUVRect.x + vDestUVRect.width * 0.5f,
                                    vDestUVRect.y + vDestUVRect.height * 0.5f};
    UnityXRVector2 vDestUVSize = {vDestUVRect.width, vDestUVRect.height};

    if (flRatio > 1.0f) {
        vSourceUVSize.x /= flRatio;
    } else {
        vSourceUVSize.y *= flRatio;
    }

    vSourceUV0 = {vSourceUVCenter.x - (vSourceUVSize.x * 0.5f),
                  vSourceUVCenter.y - (vSourceUVSize.y * 0.5f)};
    vSourceUV1 = {vSourceUV0.x + vSourceUVSize.x,
                  vSourceUV0.y + vSourceUVSize.y};
    vDestUV0 = {vDestUVCenter.x - vDestUVSize.x * 0.5f,
                vDestUVCenter.y - vDestUVSize.y * 0.5f};
    vDestUV1 = {vDestUV0.x + vDestUVSize.x, vDestUV0.y + vDestUVSize.y};

    // Set blit params
    (*pBlitDescriptor).blitParamsCount = 1;
    (*pBlitDescriptor).blitParams[0].srcTexId = m_pMirrorTexture;
    (*pBlitDescriptor).blitParams[0].srcTexArraySlice = nTextureArraySlice;
    (*pBlitDescriptor).blitParams[0].srcRect = {vSourceUV0.x, vSourceUV0.y,
                                                vSourceUV1.x - vSourceUV0.x,
                                                vSourceUV1.y - vSourceUV0.y};
    (*pBlitDescriptor).blitParams[0].destRect = {vDestUV0.x, vDestUV0.y,
                                                 vDestUV1.x - vDestUV0.x,
                                                 vDestUV1.y - vDestUV0.y};

    return kUnitySubsystemErrorCodeSuccess;
#endif
}

void OpenVRDisplayProvider::SetMirrorMode(int val) {
    if (old_m_nMirrorMode != val) {
        XR_TRACE(PLUGIN_LOG_PREFIX "Set active mirror mode (%d)\n", val);
    }
    old_m_nMirrorMode = val;
    m_nMirrorMode = val;
}

static void OpenVRMatrix3x4ToUnity(const vr::HmdMatrix34_t &ovrm,
                                   UnityXRMatrix4x4 &out) {
    float flTmpMatrix[] = {ovrm.m[0][0], ovrm.m[1][0], ovrm.m[2][0], 0.0f,
                           ovrm.m[0][1], ovrm.m[1][1], ovrm.m[2][1], 0.0f,
                           ovrm.m[0][2], ovrm.m[1][2], ovrm.m[2][2], 0.0f,
                           ovrm.m[0][3], ovrm.m[1][3], ovrm.m[2][3], 1.0f};

    out.columns[0].x = flTmpMatrix[(0 * 4) + 0];
    out.columns[1].x = flTmpMatrix[(1 * 4) + 0];
    out.columns[2].x = -flTmpMatrix[(2 * 4) + 0];
    out.columns[3].x = flTmpMatrix[(3 * 4) + 0];

    out.columns[0].y = flTmpMatrix[(0 * 4) + 1];
    out.columns[1].y = flTmpMatrix[(1 * 4) + 1];
    out.columns[2].y = -flTmpMatrix[(2 * 4) + 1];
    out.columns[3].y = flTmpMatrix[(3 * 4) + 1];

    out.columns[0].z = -flTmpMatrix[(0 * 4) + 2];
    out.columns[1].z = -flTmpMatrix[(1 * 4) + 2];
    out.columns[2].z = flTmpMatrix[(2 * 4) + 2];
    out.columns[3].z = -flTmpMatrix[(3 * 4) + 2];

    out.columns[0].w = flTmpMatrix[(0 * 4) + 3];
    out.columns[1].w = flTmpMatrix[(1 * 4) + 3];
    out.columns[2].w = -flTmpMatrix[(2 * 4) + 3];
    out.columns[3].w = flTmpMatrix[(3 * 4) + 3];
}

void OpenVRDisplayProvider::SetupMirror() {
#if 0
    // Set XR Stats
    if (s_pXRStats) {
        s_pXRStats->RegisterStatSource(s_DisplayHandle);
        m_nNumDroppedFrames = s_pXRStats->RegisterStatDefinition(
            s_DisplayHandle, "OpenVR.DroppedFrames", kUnityXRStatOptionNone);
        m_nNumFramePresents = s_pXRStats->RegisterStatDefinition(
            s_DisplayHandle, "OpenVR.FramePresents", kUnityXRStatOptionNone);
        m_flFrameTimeReference = s_pXRStats->RegisterStatDefinition(
            s_DisplayHandle, "OpenVR.FrameTimeReferenceSecs",
            kUnityXRStatOptionNone);
        m_flGPURenderTimeInMs = s_pXRStats->RegisterStatDefinition(
            s_DisplayHandle, "OpenVR.GPURenderTimeMs", kUnityXRStatOptionNone);
        m_flCPURenderTimeInMs = s_pXRStats->RegisterStatDefinition(
            s_DisplayHandle, "OpenVR.CPURenderTimeMs", kUnityXRStatOptionNone);
        m_flCPUIdelTimeInMs = s_pXRStats->RegisterStatDefinition(
            s_DisplayHandle, "OpenVR.CPUIdleTimeMs", kUnityXRStatOptionNone);
        m_flCompositorRenderTimeInMs = s_pXRStats->RegisterStatDefinition(
            s_DisplayHandle, "OpenVR.CompositorRenderTimeMs",
            kUnityXRStatOptionNone);
    }
#endif

    // Get recommended render target size based on currently active hmd,
    // assuming horizontal side by side.
    GetEyeTextureDimensions(m_nEyeHeight, m_nEyeWidth);
    m_nEyeMirrorWidth = m_nEyeWidth;
    m_nEyeMirrorHeight = m_nEyeHeight;

    // Set default mirror render subrect extents
    if (!m_bIsRenderViewportScaling) {
        m_mirrorRenderSubRect.x = k_flDefaultSubRectX;
        m_mirrorRenderSubRect.y = k_flDefaultSubRectY;
        m_mirrorRenderSubRect.width = k_flDefaultSubRectWidth;
        m_mirrorRenderSubRect.height = k_flDefaultSubRectHeight;
    }

    // Set default render subrects
    if (m_bIsUsingCustomMirrorMode &&
        m_nMirrorMode == kUnityXRMirrorBlitDistort) {
        m_mirrorRenderSubRect = {0.0f, 0.0f, 1.0f, 1.0f};
    } else {
        UnityXRVector2 mirrorResolution = GetRecommendedMirrorResolution();
        m_nRenderMirrorWidth = (uint32_t)mirrorResolution.x;
        m_nRenderMirrorHeight = (uint32_t)mirrorResolution.y;

        if (m_nRenderMirrorWidth > 0 && m_nRenderMirrorHeight > 0 &&
            m_nRenderMirrorWidth < m_nEyeWidth &&
            m_nRenderMirrorHeight < m_nEyeHeight) {
            float flRenderWidth = (float)m_nRenderMirrorWidth;
            float flRenderHeight = (float)m_nRenderMirrorHeight;
            float flEyeWidth = (float)m_nEyeWidth;
            float flEyeHeight = (float)m_nEyeHeight;

            float aspectRatio = flRenderWidth / flRenderHeight;
            m_mirrorRenderSubRect.x = (flEyeWidth - flRenderWidth) / flEyeWidth;
            m_mirrorRenderSubRect.y =
                1.0f - ((flEyeHeight - flRenderHeight) / flEyeHeight);
            m_mirrorRenderSubRect.width = 1.0f - m_mirrorRenderSubRect.x;
            m_mirrorRenderSubRect.height =
                m_mirrorRenderSubRect.width / aspectRatio;
        }
    }

    // Do not open shared texture until we've actually sent one frame to the
    // compositor
    if (!m_bIsHeadsetResolutionSet) return;

    if (m_nMirrorMode == kUnityXRMirrorBlitDistort) {
        SetupOverlayMirror();
    }
}

void OpenVRDisplayProvider::SetupRenderPass(
    const EEye eEye, const UnityXRFrameSetupHints *pFrameHints,
    UnityXRNextFrameDesc *pTargetFrame) {
    // Setup vars for the render params
    uint32_t nParamsCount;
    uint32_t nRenderPasses;

    int32_t nRenderPassesCount;
    int32_t nRenderParamsCount;
    int32_t nTextureArraySlice;

    switch (m_renderingMode) {
        case EVRStereoRenderingModes::MultiPass:
            // Multi-pass values
            nParamsCount = 0;
            nRenderPasses = static_cast<int>(eEye);

            nRenderPassesCount = 2;
            nRenderParamsCount = 1;
            nTextureArraySlice = 0;
            break;
        case EVRStereoRenderingModes::SinglePassInstanced:
            // Single pass and single pass instance values
            nParamsCount = static_cast<int>(eEye);
            nRenderPasses = 0;

            nRenderPassesCount = 1;
            nRenderParamsCount = 2;
            nTextureArraySlice = static_cast<int>(eEye);
            break;
        case EVRStereoRenderingModes::SingleCamera:
            // Single Camera values
            nParamsCount = 0;
            nRenderPasses = 0;

            nRenderPassesCount = 1;
            nRenderParamsCount = 1;
            nTextureArraySlice = 0;
            break;

        default:
            XR_TRACE_ERROR(XR_TRACE_PTR,
                           PLUGIN_LOG_PREFIX
                           "Unrecognized stereo render mode: %d\n",
                           (int)m_renderingMode);
            return;
    }

    // Set the number of render passes for this frame
    pTargetFrame->renderPassesCount = nRenderPassesCount;

    // Setup base render pass properties
    UnityXRNextFrameDesc::UnityXRRenderPass &renderPass =
        pTargetFrame->renderPasses[nRenderPasses];
    renderPass.textureId =
        m_UnityTextures[m_nCurFrame % m_nNumStages][nRenderPasses];
    renderPass.renderParamsCount = nRenderParamsCount;
    renderPass.cullingPassIndex = nRenderPasses;

    // Setup base render pass parameters
    UnityXRNextFrameDesc::UnityXRRenderPass::UnityXRRenderParams &renderParams =
        renderPass.renderParams[nParamsCount];
    renderParams.deviceAnchorToEyePose = GetEyePose(eEye);
    renderParams.projection = GetProjection(eEye, pFrameHints->appSetup.zNear,
                                            pFrameHints->appSetup.zFar);
    renderParams.viewportRect = {0.0f, 0.0f, 1.0f, 1.0f};
    renderParams.textureArraySlice = nTextureArraySlice;

    // Set an occlusion mesh (hidden area mesh) if there's a valid one
    if (eEye == EEye::Left && m_pOcclusionMeshLeftEye != 0) {
        renderParams.occlusionMeshId = m_pOcclusionMeshLeftEye;
    } else if (eEye == EEye::Right && m_pOcclusionMeshRightEye != 0) {
        renderParams.occlusionMeshId = m_pOcclusionMeshRightEye;
    }
}

UnityXROcclusionMeshId OpenVRDisplayProvider::SetupOcclusionMesh(EEye eEye) {
    return k_nInvalidUnityXROcclusionMeshId;
#if 0
    // Grab the hidden area mesh from SteamVR
    vr::HiddenAreaMesh_t vrHiddenMesh = vr::VRSystem()->GetHiddenAreaMesh(
        (vr::EVREye)eEye, vr::k_eHiddenAreaMesh_Standard);

    if (vrHiddenMesh.pVertexData == NULL || vrHiddenMesh.unTriangleCount == 0) {
        XR_TRACE(PLUGIN_LOG_PREFIX
                 "No hidden area mesh available for eye[%i] in active "
                 "hmd\n",
                 (int)eEye);
        return k_nInvalidUnityXROcclusionMeshId;
    }

    // Setup our vertex and index arrays
    std::vector<uint32_t> vIndices(
        (long long)vrHiddenMesh.unTriangleCount * (long long)3, UINT32_MAX);
    std::vector<UnityXRVector2> vVertices;
    vVertices.reserve(vIndices.size());

    for (size_t v1 = 0; v1 < vIndices.size();
         v1++)  // Go through each vertex from OpenVR (may contain duplicates)
    {
        if (vIndices[v1] != UINT32_MAX)  // Skip if already assigned (i.e. was
                                         // found to be a duplicate)
            continue;

        uint32_t nIndex = (uint32_t)vVertices.size();  // Record new index
        vIndices[v1] = nIndex;

        const auto &v = vrHiddenMesh.pVertexData[v1].v;  // Keep this vertex
        vVertices.push_back({v[0], v[1]});

        for (size_t v2 = v1 + 1; v2 < vVertices.size();
             v2++)  // Check remaining for duplicate vertices
        {
            static const float k_flThreshold = 1e-9f;
            if (GetDistanceSquared2D(v, vrHiddenMesh.pVertexData[v2].v) <
                k_flThreshold) {
                vIndices[v2] = nIndex;  // Remap this index to point to similar
                                        // earlier vertex
            }
        }
    }

    // Create a Unity occlusion mesh
    UnityXROcclusionMeshId pOcclusionMeshId;
    UnitySubsystemErrorCode res = s_pXRDisplay->CreateOcclusionMesh(
        s_DisplayHandle, (uint32_t)vVertices.size(), (uint32_t)vIndices.size(),
        &pOcclusionMeshId);

    if (res != kUnitySubsystemErrorCodeSuccess) {
        XR_TRACE(PLUGIN_LOG_PREFIX
                 "Error creating occlusion mesh for eye[%i]: [%i]\n",
                 (int)eEye, res);
        return k_nInvalidUnityXROcclusionMeshId;
    }

    // Setup the Unity occlusion mesh
    res = s_pXRDisplay->SetOcclusionMesh(
        s_DisplayHandle, pOcclusionMeshId, vVertices.data(),
        (uint32_t)vVertices.size(), vIndices.data(), (uint32_t)vIndices.size());

    if (res != kUnitySubsystemErrorCodeSuccess) {
        XR_TRACE(PLUGIN_LOG_PREFIX
                 "Error creating occlusion mesh for eye[%i]: [%i]\n",
                 eEye, res);
        return k_nInvalidUnityXROcclusionMeshId;
    }

    // Finally, return the occlusion mesh id to caller
    return pOcclusionMeshId;
#endif
}

float OpenVRDisplayProvider::GetDistanceSquared2D(const float *pVector1,
                                                  const float *pVector2) {
    float flDiffX = pVector2[0] - pVector1[0];
    float flDiffY = pVector2[1] - pVector1[1];

    return (flDiffX * flDiffX) + (flDiffY * flDiffY);
}

const UnityXRVector2 OpenVRDisplayProvider::GetRecommendedMirrorResolution() {
    float flWidth = vr::k_unHeadsetViewMaxWidth;
    float flHeight = vr::k_unHeadsetViewMaxHeight;
    // flHeight = flWidth / k_flDockedViewAspecRatio;

    return UnityXRVector2{flWidth, flHeight};
}

UnityXRPose OpenVRDisplayProvider::GetEyePose(EEye eye) {
    UnityXRPose ret = {0};
    XRQuaternion quat{0, 0, 0, 1};

    // this actually wants eye to head, I think.
    XRVector3 pos{0, 0, -NominalHeadToEye};
    if (eye != EEye::CenterOrBoth) {
        pos.x = (eye == EEye::Left) ? SinglePassInstancedCamIPD / -2.f
                                    : SinglePassInstancedCamIPD / 2.f;
    }
    ret.position = pos;

    if (m_bRotateEyes) {
        if (eye == EEye::Left) {
            quat = XRQuaternion(0.f, 0.f, 0.7071f, 0.7071f);
        }
        if (eye == EEye::Right) {
            quat = XRQuaternion(0.f, 0.f, -0.7071f, 0.7071f);
        }
    }
    ret.rotation = quat;
    return ret;
}

static inline std::ostream &operator<<(std::ostream &os, XRVector4 const &vec) {
    os << vec.x << "," << vec.y << "," << vec.z << "," << vec.w;
    return os;
}
static inline std::ostream &operator<<(std::ostream &os,
                                       XRMatrix4x4 const &mat) {
    os << mat.GetRow(0) << "\n"
       << mat.GetRow(1) << "\n"
       << mat.GetRow(2) << "\n"
       << mat.GetRow(3);
    return os;
}
XRMatrix4x4 makeOrtho(float top, float bottom, float left, float right, float n,
                      float f) {
    float nearval = n < k_flNear ? k_flNear : n;
    float farval = f < k_flNear ? k_flFar : f;
    XRMatrix4x4 ret = XRMatrix4x4::identity;
    ret.Set(0, 0, 2.f / (right - left));
    ret.Set(1, 1, 2.f / (top - bottom));
    ret.Set(2, 2, -2.f / (farval - nearval));
    // trailing comments are to coax clang-format into doing what looks best.
    ret.SetCol(3, XRVector4(-(right + left) / (right - left),          //
                            -(top + bottom) / (top - bottom),          //
                            -(farval + nearval) / (farval - nearval),  //
                            1));

    std::ostringstream oss;
    oss << "makeOrtho: \n" << ret;
    XR_TRACE(oss.str().c_str());
    if (!isMatrixValid(ret)) {
        DEBUGLOG("Got non-invertible ortho matrix!");
    }
    return ret;
}

static inline UnityXRProjection makePerspective(float tanL, float tanR,
                                                float tanT, float tanB,
                                                float flNear, float flFar) {
    UnityXRProjection ret;
    ret.type = kUnityXRProjectionTypeMatrix;

    float x, y, a, b, c, d, e;

    float nearval = flNear < k_flNear ? k_flNear : flNear;
    float farval = flFar < k_flNear ? k_flFar : flFar;
    float left = tanL * nearval;
    float right = tanR * nearval;
    float top = tanB * nearval;
    float bottom = tanT * nearval;

    x = (2.0F * nearval) / (right - left);
    //! @todo Not sure why the negative here is needed - not needed in the
    //! original Valve code, but doesn't work right here without it.
    y = -(2.0F * nearval) / (top - bottom);
    a = (right + left) / (right - left);
    b = (top + bottom) / (top - bottom);
    c = -(farval + nearval) / (farval - nearval);
    d = -(2.0f * farval * nearval) / (farval - nearval);
    e = -1.0f;

    // clang-format off
	ret.data.matrix.columns[0].x = x;     ret.data.matrix.columns[1].x = 0.0f;  ret.data.matrix.columns[2].x = a;   ret.data.matrix.columns[3].x = 0.0f;
	ret.data.matrix.columns[0].y = 0.0f;  ret.data.matrix.columns[1].y = y;     ret.data.matrix.columns[2].y = b;   ret.data.matrix.columns[3].y = 0.0f;
	ret.data.matrix.columns[0].z = 0.0f;  ret.data.matrix.columns[1].z = 0.0f;  ret.data.matrix.columns[2].z = c;   ret.data.matrix.columns[3].z = d;
	ret.data.matrix.columns[0].w = 0.0f;  ret.data.matrix.columns[1].w = 0.0f;  ret.data.matrix.columns[2].w = e;   ret.data.matrix.columns[3].w = 0.0f;
    // clang-format on

    if (!isMatrixValid(ret.data.matrix)) {
        DEBUGLOG("Got non-invertible projection matrix!");
    }
    return ret;
}

static inline UnityXRMatrix4x4 toUnity(DirectX::XMFLOAT4X4 const &val) {
    UnityXRMatrix4x4 ret;
    ret.columns[0] = {val._11, val._21, val._31, val._41};
    ret.columns[1] = {val._12, val._22, val._32, val._42};
    ret.columns[2] = {val._13, val._23, val._33, val._43};
    ret.columns[3] = {val._14, val._24, val._34, val._44};
    return ret;
}

UnityXRProjection OpenVRDisplayProvider::GetProjection(EEye eye, float flNear,
                                                       float flFar) {
    UnityXRProjection ret;
    ret.type = kUnityXRProjectionTypeMatrix;

    // clamp unreasonably small values for near/far
    // Unity will sometimes ask for near/far values of 0, which we must respond
    // sensibly to.
    if (flNear < k_flNear) {
        // XR_TRACE(PLUGIN_LOG_PREFIX "clamped near %f to %f\n", flNear,
        // k_flNear);
        flNear = k_flNear;
    }
    if (flFar < k_flNear) {
        // XR_TRACE(PLUGIN_LOG_PREFIX "clamped far %f to %f\n", flFar, k_flFar);
        flFar = k_flFar;
    }
#ifdef REALORTHO
    if (m_renderingMode == EVRStereoRenderingModes::SingleCamera) {
        // DEBUGLOG("near: " << flNear << " far: " << flFar << " eye: " <<
        // (int)eye);
        float top = OrthoHalfSize;
        float bottom = -top;
        float right = AspectRatio * top;
        float left = -right;
        if (eye == EEye::CenterOrBoth) {
            // Calculate combined left + right eye combined projection
            // Use the max extent's for each eye
        } else if (eye == EEye::Left) {
            // 0 might makes Unity sad.
            right = 0.1f;
        } else {
            // 0 might make Unity sad.
            left = -0.1f;
        }

#ifndef NDEBUG
        XR_TRACE(PLUGIN_LOG_PREFIX
                 "%i EYE PROJECTION: %f %f %f %f\n\tNear: %f Far: %f\n",
                 eye, left, right, top, bottom, flNear, flFar);
#endif
        // orthographic now
        //! @todo switch to real projection later
        ret.data.matrix = makeOrtho(top, bottom, left, right, flNear, flFar);

        ret.data.matrix =
            toUnity(DirectX::SimpleMath::Matrix::CreateOrthographic(
                OrthoHalfSize * AspectRatio * 2, OrthoHalfSize * 2, flNear,
                flFar));
        return ret;
    }
#else

    if (m_renderingMode == EVRStereoRenderingModes::SingleCamera) {
        ret = makePerspective(SingleCamFov.left, SingleCamFov.right,
                              SingleCamFov.top, SingleCamFov.bottom, flNear,
                              flFar);

        return ret;
    }
#endif
    {
        float vrL, vrR, vrT, vrB;
        if (eye == EEye::CenterOrBoth) {
            // Calculate combined left + right eye combined projection
            // Use the max extent's for each eye
            vrL = LeftFovRuntime.left;
            vrR = RightFovRuntime.right;
            vrT = RightFovRuntime.top;
            vrB = LeftFovRuntime.bottom;

        } else if (eye == EEye::Left) {
            vrL = LeftFovRuntime.left;
            vrR = LeftFovRuntime.right;
            vrT = LeftFovRuntime.top;
            vrB = LeftFovRuntime.bottom;

        } else {
            vrL = RightFovRuntime.left;
            vrR = RightFovRuntime.right;
            vrT = RightFovRuntime.top;
            vrB = RightFovRuntime.bottom;
        }

#ifndef NDEBUG
        XR_TRACE(PLUGIN_LOG_PREFIX
                 "%i EYE PROJECTION: %f %f %f %f\n\tNear: %f Far: %f\n",
                 eye, vrL / flNear, vrR / flNear, vrT / flNear, vrB / flNear,
                 flNear, flFar);
#endif
        ret = makePerspective(vrL, vrR, vrT, vrB, flNear, flFar);
    }
    return ret;
}

static inline float computeEyePullback(float ipd, float vertFovRadians,
                                       float aspect) {
    return 0.5f * ipd / tanf(0.5f * vertFovRadians * aspect);
}

void OpenVRDisplayProvider::SetupCullingPass(
    EEye eye, const UnityXRFrameSetupHints *frameHints,
    UnityXRNextFrameDesc::UnityXRCullingPass &cullingPass) {
    cullingPass.separation = SinglePassInstancedCamIPD;

    UnityXRPose pose = GetEyePose(eye);
    float eyePullback = 0.f;
    UnityXRProjection projection;

    // clamp unreasonably small values for near/far
    // Unity will sometimes ask for near/far values of 0, which we must respond
    // sensibly to.
    float flNear = (frameHints->appSetup.zNear < k_flNear)
                       ? k_flNear
                       : frameHints->appSetup.zNear;
    float flFar = (frameHints->appSetup.zFar < k_flNear)
                      ? k_flFar
                      : frameHints->appSetup.zFar;
#ifdef REALORTHO
    if (m_renderingMode == EVRStereoRenderingModes::SingleCamera) {
#if 0
        // Compute a perspective frustum that includes the image plane, since
        // orthographic culling doesn't work in Unity.
        float distance = (flNear + flFar) / 2.f;
        eyePullback = computeEyePullback(cullingPass.separation, 90.f * DEG2RAD,
                                         AspectRatio);

        projection = makePerspective(
            // ratio times top divided by distance is ratio times 1
            -AspectRatio, AspectRatio,
            // top divided by distance is 1, etc.
            1.f, -1.f, flNear, flFar);
#endif

        projection = GetProjection(eye, flNear, flFar);
    } else
#endif
    {
        projection = GetProjection(eye, flNear, flFar);
        assert(projection.type == kUnityXRProjectionTypeMatrix);
        float aspect = projection.data.matrix.columns[1].y /
                       projection.data.matrix.columns[0].x;

        float vertFov =
            (2.0f * (float)atan(1.0f / projection.data.matrix.columns[1].y));
        eyePullback =
            computeEyePullback(cullingPass.separation, vertFov, aspect);
    }

    pose.position.z = pose.position.z - eyePullback;
    cullingPass.deviceAnchorToCullingPose = pose;
    cullingPass.projection = projection;
}

UnitySubsystemErrorCode OpenVRDisplayProvider::CreateEyeTextures(
    const UnityXRFrameSetupHints *frameHints) {
    // One texture per eye, per stage
    int nNumTextures = 2;

    // Grab texture size from renderer
    uint32_t eyeWidth, eyeHeight;
    GetEyeTextureDimensions(eyeHeight, eyeWidth);

    // Apply scale
    float eyeWidthScaled =
        eyeWidth * frameHints->appSetup.textureResolutionScale;
    float eyeHeightScaled =
        eyeHeight * frameHints->appSetup.textureResolutionScale;
    eyeWidth = (uint32_t)eyeWidthScaled * 2;
    eyeHeight = (uint32_t)eyeHeightScaled * 2;

    // Create textures
    for (int stage = 0; stage < m_nNumStages; ++stage) {
        for (int eye = 0; eye < nNumTextures; ++eye) {
            UnityXRRenderTextureDesc unityDesc;
            memset(&unityDesc, 0, sizeof(UnityXRRenderTextureDesc));
            unityDesc.colorFormat = kUnityXRRenderTextureFormatRGBA32;
            unityDesc.depthFormat = kUnityXRDepthTextureFormat24bitOrGreater;
            unityDesc.color.nativePtr = (void *)kUnityXRRenderTextureIdDontCare;
            unityDesc.depth.nativePtr = (void *)kUnityXRRenderTextureIdDontCare;
            unityDesc.width = eyeWidth;
            unityDesc.height = eyeHeight;

            if (m_bIsUsingSRGB) {
                unityDesc.flags |= kUnityXRRenderTextureFlagsSRGB;
            }

            if (m_renderingMode ==
                EVRStereoRenderingModes::SinglePassInstanced) {
                unityDesc.textureArrayLength = 2;
            }

            // Create an UnityXRRenderTextureId for the native texture so we can
            // tell unity to render to it later.
            UnityXRRenderTextureId unityTexId;
            UnitySubsystemErrorCode res = s_pXRDisplay->CreateTexture(
                s_DisplayHandle, &unityDesc, &unityTexId);
            if (res != kUnitySubsystemErrorCodeSuccess) {
                XR_TRACE(PLUGIN_LOG_PREFIX "Error creating texture: [%i]\n",
                         res);
                return res;
            }

            m_UnityTextures[stage][eye] = unityTexId;
            m_pNativeColorTextures[stage][eye] =
                nullptr;  // this is just registering a creation request, we'll
                          // grab the native textures later
            m_pNativeDepthTextures[stage][eye] = nullptr;
        }
    }

    m_bTexturesCreated = true;
    return kUnitySubsystemErrorCodeSuccess;
}

void OpenVRDisplayProvider::DestroyEyeTextures(UnitySubsystemHandle handle) {
    for (int i = 0; i < m_nNumStages; ++i) {
        for (int eye = 0; eye < 2; ++eye) {
            if (m_UnityTextures[i][eye] != 0) {
                s_pXRDisplay->DestroyTexture(handle, m_UnityTextures[i][eye]);
            }
        }
    }

    m_bTexturesCreated = false;
}

void *OpenVRDisplayProvider::GetNativeEyeTexture(int stage, int eye) {
    if (m_pNativeColorTextures[stage][eye] == nullptr) {
        UnityXRRenderTextureId unityTexId = m_UnityTextures[stage][eye];

        UnityXRRenderTextureDesc unityDesc;
        memset(&unityDesc, 0, sizeof(UnityXRRenderTextureDesc));

        UnitySubsystemErrorCode res = s_pXRDisplay->QueryTextureDesc(
            s_DisplayHandle, unityTexId, &unityDesc);
        if (res != kUnitySubsystemErrorCodeSuccess) {
            XR_TRACE(PLUGIN_LOG_PREFIX "Error querying texture: [%i]\n", res);
            return nullptr;
        }

        m_pNativeColorTextures[stage][eye] = unityDesc.color.nativePtr;
        XR_TRACE(PLUGIN_LOG_PREFIX "Created Native Color: %x\n",
                 unityDesc.color.nativePtr);

        m_pNativeDepthTextures[stage][eye] = unityDesc.depth.nativePtr;
        XR_TRACE(PLUGIN_LOG_PREFIX "Created Native Depth: %x\n",
                 unityDesc.depth.nativePtr);
    }

    return m_pNativeColorTextures[stage][eye];
}

void OpenVRDisplayProvider::ReleaseOverlayPointers() {
#ifndef __linux__
    m_pMirrorTextureDX = nullptr;
#endif
}

bool OpenVRDisplayProvider::HasOverlayPointer() {
#ifndef __linux__
    return m_pMirrorTextureDX != nullptr;
#else
    return false;
#endif
}

void OpenVRDisplayProvider::SetupOverlayMirror() {
    m_bOverlayFallback = true;

    if (m_bOverlayFallback) {
        // Fallback to left eye texture
        SetMirrorMode(kUnityXRMirrorBlitLeftEye);
        m_nPrevMirrorMode = kUnityXRMirrorBlitLeftEye;
        m_pMirrorTexture = m_UnityTextures[0][0];
        m_bIsSteamVRViewAvailable = false;

        SetupMirror();  // setup with the left eye
    }
}

void OpenVRDisplayProvider::GetEyeTextureDimensions(uint32_t &height,
                                                    uint32_t &width) const {
    if (renderer_) {
        height = renderer_->getHeight();
        width = renderer_->getWidth() / 2;
    } else {
        // Hard code a good guess
        height = 1600;
        width = 1440;
    }
    if (m_renderingMode == EVRStereoRenderingModes::SingleCamera) {
        // single texture for full width
        width *= 2;
    }
}

bool RegisterDisplayLifecycleProvider(
    OpenVRProviderContext *pOpenProviderContext) {
    XR_TRACE(PLUGIN_LOG_PREFIX "Display lifecyle provider registered\n");

    s_pXRDisplay =
        UnityInterfaces::Get().GetInterface<IUnityXRDisplayInterface>();
    s_pProviderContext = pOpenProviderContext;
    s_pXRStats = UnityInterfaces::Get().GetInterface<IUnityXRStats>();

    UnityLifecycleProvider displayLifecycleHandler = {0};

    pOpenProviderContext->displayProvider = new OpenVRDisplayProvider;
    displayLifecycleHandler.userData = pOpenProviderContext->displayProvider;
    displayLifecycleHandler.Initialize = &Lifecycle_Initialize;
    displayLifecycleHandler.Start = &Lifecycle_Start;
    displayLifecycleHandler.Stop = &Lifecycle_Stop;
    displayLifecycleHandler.Shutdown = &Lifecycle_Shutdown;

    UnitySubsystemErrorCode result = s_pXRDisplay->RegisterLifecycleProvider(
        PluginDllName, DisplayProviderName, &displayLifecycleHandler);

    if (result != kUnitySubsystemErrorCodeSuccess) {
        XR_TRACE(PLUGIN_LOG_PREFIX
                 "[Error] Unable to register display lifecyle provider: "
                 "[%i]\n",
                 result);
        return false;
    }

    return true;
}


//Han Custom Code
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
SetParamsForSinglePassInstancedCameraDisplayCPP(float IPD) {
    SinglePassInstancedCamIPD = IPD;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
SetParamsForSinglePassInstancedCameraFOV(float widthHalfAngle, float heightHalfAngle) {
    LeftFovRuntime = {-widthHalfAngle, widthHalfAngle, heightHalfAngle, -heightHalfAngle};
    RightFovRuntime = {-widthHalfAngle, widthHalfAngle, heightHalfAngle, -heightHalfAngle};
}
