// Copyright (c) 2020, Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/Providers/OpenVRSystem.h
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "Model/DirectDisplayManager.h"
#include "Model/DisplayDetection.h"

#include <ProviderInterface/IUnityXRPreInit.h>
#include "OpenVR/openvr.h"
#include "Singleton.h"

extern "C" typedef void (*TickCallback)(int);

class OpenVRSystem : public Singleton<OpenVRSystem> {
  public:
    OpenVRSystem();
    ~OpenVRSystem();

    bool Initialize();
    bool Shutdown();

    bool GetInitialized();

    /* -- XRPreInit functions -- */
    bool GetGraphicsAdapterId(void *userData, UnityXRPreInitRenderer renderer,
                              uint64_t rendererData, uint64_t *adapterId);
    bool GetVulkanInstanceExtensions(void *userData, uint32_t namesCapacityIn,
                                     uint32_t *namesCountOut,
                                     char *namesString);
    bool GetVulkanDeviceExtensions(void *userData, uint32_t namesCapacityIn,
                                   uint32_t *namesCountOut, char *namesString);
    /* -- end XRPreInit functions -- */

    int GetFrameIndex() { return m_FrameIndex; }
    void SetFrameIndex(int frameIndex) { m_FrameIndex = frameIndex; }
    bool Update();

    metaview::InitErrors GetInitializationResult();

    void SetTickCallback(TickCallback newTickCallback) {
        tickCallback = newTickCallback;
    }
    /**
     * @brief Get the implementation that handles display detection.
     *
     * @pre GetInitialized() must be true due to a call to Initialize() not
     * followed by a call to Shutdown().
     *
     * @return metaview::IDisplayDetection&
     */
    metaview::IDisplayDetection &GetDisplayDetection() {
        return *displayDetection_;
    }
    metaview::DirectDisplayManager &GetDirectDisplayManager() {
        return *directDisplayManager_;
    }

    /**
     * @brief Attempt to re-enumerate displays if there was an error or it
     * hasn't been done yet.
     *
     */
    void ReEnumerateDisplaysIfNeeded() {
        if (displayDetection_->getStatus() != metaview::InitErrors::None) {
            displayDetection_->enumerateDisplays(*directDisplayManager_);
        }
    }

  private:
    uint64_t graphicsAdapterId;
    int m_FrameIndex;
    std::unique_ptr<metaview::IDisplayDetection> displayDetection_;
    std::unique_ptr<metaview::DirectDisplayManager> directDisplayManager_;

    TickCallback tickCallback;
};
