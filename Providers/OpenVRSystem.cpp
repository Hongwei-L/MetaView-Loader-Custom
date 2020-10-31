// Copyright (c) 2020, Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/Providers/OpenVRSystem.cpp
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#include "OpenVRSystem.h"

#include "CommonTypes.h"
#include "Metadata.h"
#include "ProviderInterface/IUnityGraphics.h"
#include "ProviderInterface/IUnityInterface.h"
#include "UnityInterfaces.h"
#include "UserProjectSettings.h"

#include "Model/Log.h"

#include <cassert>

using namespace metaview;

OpenVRSystem::OpenVRSystem() : m_FrameIndex(0), tickCallback(nullptr) {
    XR_TRACE(PLUGIN_LOG_PREFIX "static Initialize\n");

    if (!UserProjectSettings::InEditor()) {
        // if we're not coming from an editor then we need to initialize
        // immediately. This seems like a bug that unity should fix. (keithb
        // 1/28/2020)
        Initialize();
    }
}

OpenVRSystem::~OpenVRSystem() {
    // Frequently the trace interface has already been freed.
    s_pXRTrace = nullptr;
    Shutdown();
}

bool OpenVRSystem::GetInitialized() {
    return (directDisplayManager_ != nullptr);
}

bool OpenVRSystem::Initialize() {
    if (!GetInitialized()) {
        XR_TRACE(PLUGIN_LOG_PREFIX "Starting Initialize\n");

        winrt::init_apartment(winrt::apartment_type::single_threaded);
        directDisplayManager_ =
            std::make_unique<metaview::DirectDisplayManager>();
        XR_TRACE(PLUGIN_LOG_PREFIX "DisplayManager created\n");
        displayDetection_ = metaview::IDisplayDetection::create();
        XR_TRACE(PLUGIN_LOG_PREFIX "DisplayDetection created\n");

        displayDetection_->enumerateDisplays(*directDisplayManager_);
        XR_TRACE(PLUGIN_LOG_PREFIX "Enumeration complete\n");

        UserProjectSettings::Initialize();
    }

    XR_TRACE(PLUGIN_LOG_PREFIX "is initialized\n");
    return true;
}

bool OpenVRSystem::Shutdown() {
    XR_TRACE(PLUGIN_LOG_PREFIX "Shutdown\n");
    tickCallback = nullptr;
    displayDetection_.reset();
    directDisplayManager_.reset();
    if (GetInitialized()) {
        winrt::uninit_apartment();
    }

    return true;
}

bool OpenVRSystem::Update() {
    m_FrameIndex++;

    if (tickCallback) {
        tickCallback(m_FrameIndex);
    }

    return true;
}

bool OpenVRSystem::GetGraphicsAdapterId(void *userData,
                                        UnityXRPreInitRenderer renderer,
                                        uint64_t rendererData,
                                        uint64_t *adapterId) {
    if (!GetInitialized()) {
        DEBUGLOG("Not yet initialized.");
        return false;
    }

    switch (renderer) {
        case UnityXRPreInitRenderer::kUnityXRPreInitRendererD3D11: {
            ReEnumerateDisplaysIfNeeded();
            graphicsAdapterId = displayDetection_->getLuid();
            DEBUGLOG("Want to use LUID " << graphicsAdapterId);
            DEBUGLOG("Status: " << to_string(displayDetection_->getStatus()));

            break;
        }

        default:
            break;
    }
    *adapterId = (uint64_t)(&graphicsAdapterId);

    return graphicsAdapterId != 0;
}

bool OpenVRSystem::GetVulkanInstanceExtensions(void *userData,
                                               uint32_t namesCapacityIn,
                                               uint32_t *namesCountOut,
                                               char *namesString) {
    return false;
#if 0
    if (namesCapacityIn == 0) {
        *namesCountOut =
            m_VRCompositor->GetVulkanInstanceExtensionsRequired(nullptr, 0);
        return true;
    }

    assert(namesCapacityIn >=
           m_VRCompositor->GetVulkanInstanceExtensionsRequired(nullptr, 0));
    assert(namesString != nullptr);

    m_VRCompositor->GetVulkanInstanceExtensionsRequired(namesString,
                                                        namesCapacityIn);
    *namesCountOut = namesCapacityIn;

    return true;
#endif
}

bool OpenVRSystem::GetVulkanDeviceExtensions(void *userData,
                                             uint32_t namesCapacityIn,
                                             uint32_t *namesCountOut,
                                             char *namesString) {
    return false;
#if 0
    if (namesCapacityIn == 0) {
        *namesCountOut = m_VRCompositor->GetVulkanDeviceExtensionsRequired(
            nullptr, nullptr, 0);
        return true;
    }

    assert(namesCapacityIn >= m_VRCompositor->GetVulkanDeviceExtensionsRequired(
                                  nullptr, nullptr, 0));
    assert(namesString != nullptr);

    m_VRCompositor->GetVulkanDeviceExtensionsRequired(nullptr, namesString,
                                                      namesCapacityIn);
    *namesCountOut = namesCapacityIn;

    return true;
#endif
}

metaview::InitErrors OpenVRSystem::GetInitializationResult() {
    if (!displayDetection_) {
        return InitErrors::NotInitialized;
    }
    return displayDetection_->getStatus();
}

extern "C" uint32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
GetInitializationResult() {
    try {
        OutputDebugStringA("In " __FUNCTION__);
        return static_cast<uint32_t>(
            OpenVRSystem::Get().GetInitializationResult());
    } catch (std::exception const &e) {
        OutputDebugStringA(__FUNCTION__ ": Exception ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        return 1;  // unknown error
    }
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
RegisterTickCallback(TickCallback newTickCallback) {
    OutputDebugStringA("In " __FUNCTION__);
    try {
        OpenVRSystem::Get().SetTickCallback(newTickCallback);
    } catch (std::exception const &e) {
        OutputDebugStringA(__FUNCTION__ ": Exception ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
    }
}
