// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#include "CommonTypes.h"
#include "Metadata.h"
#include "ProviderInterface/IUnityInterface.h"
#include "ProviderInterface/IUnityXRPreInit.h"
#include "ProviderInterface/IUnityXRTrace.h"
#include "ProviderInterface/UnitySubsystemTypes.h"
#include "UnityInterfaces.h"

#include "Display/Display.h"
#include "Input/Input.h"
#include "OpenVRProviderContext.h"
#include "OpenVRSystem.h"
#include "UserProjectSettings.h"

static OpenVRProviderContext *s_pOpenVRProviderContext{};

UnitySubsystemErrorCode Load_Display(OpenVRProviderContext &);
UnitySubsystemErrorCode Load_Input(OpenVRProviderContext &);

IUnityXRTrace *s_pXRTrace = nullptr;

static bool ReportError(const char *subsystemProviderName,
                        UnitySubsystemErrorCode err) {
    if (err != kUnitySubsystemErrorCodeSuccess) {
        XR_TRACE_ERROR(s_pOpenVRProviderContext->trace,
                       PLUGIN_LOG_PREFIX
                       "[Error] Error loading subsystem: %s (%d)\n",
                       subsystemProviderName, err);
        return true;
    }
    return false;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityPluginLoad(IUnityInterfaces *unityInterfaces) {
    try {
        // Setup interfaces
        UnityInterfaces::Get().SetUnityInterfaces(unityInterfaces);
        s_pXRTrace = unityInterfaces->Get<IUnityXRTrace>();

        // Setup provider context
        s_pOpenVRProviderContext = new OpenVRProviderContext;
        s_pOpenVRProviderContext->trace = unityInterfaces->Get<IUnityXRTrace>();

        XR_TRACE(PLUGIN_LOG_PREFIX "Registering providers\n");
        RegisterDisplayLifecycleProvider(s_pOpenVRProviderContext);
        RegisterInputLifecycleProvider(s_pOpenVRProviderContext);
    } catch (std::exception const &e) {
        OutputDebugStringA(__FUNCTION__ ": Exception ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
    }
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload() {
    s_pXRTrace = nullptr;
}

static bool UNITY_INTERFACE_API GetPreInitFlags(void *userData,
                                                uint64_t *flags) {
    *flags = 0;

    return true;
}

static bool UNITY_INTERFACE_API
GetGraphicsAdapterId(void *userData, UnityXRPreInitRenderer renderer,
                     uint64_t rendererData, uint64_t *adapterId) {
    try {
        if (OpenVRSystem::Get().GetInitialized()) {
            return OpenVRSystem::Get().GetGraphicsAdapterId(
                userData, renderer, rendererData, adapterId);
        }
    } catch (std::exception const &e) {
        OutputDebugStringA(__FUNCTION__ ": Exception ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
    }

    return false;
}

static bool UNITY_INTERFACE_API
GetVulkanInstanceExtensions(void *userData, uint32_t namesCapacityIn,
                            uint32_t *namesCountOut, char *namesString) {
    try {
        return OpenVRSystem::Get().GetVulkanInstanceExtensions(
            userData, namesCapacityIn, namesCountOut, namesString);
    }

    catch (std::exception const &e) {
        OutputDebugStringA(__FUNCTION__ ": Exception ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        return false;
    }
}

static bool UNITY_INTERFACE_API
GetVulkanDeviceExtensions(void *userData, uint32_t namesCapacityIn,
                          uint32_t *namesCountOut, char *namesString) {
    try {
        return OpenVRSystem::Get().GetVulkanDeviceExtensions(
            userData, namesCapacityIn, namesCountOut, namesString);

    } catch (std::exception const &e) {
        OutputDebugStringA(__FUNCTION__ ": Exception ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        return false;
    }
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
XRSDKPreInit(IUnityInterfaces *interfaces) {
    try {
        IUnityXRPreInit *preInit = interfaces->Get<IUnityXRPreInit>();

        UnityXRPreInitProvider provider = {0};

        provider.userData = nullptr;
        provider.GetPreInitFlags = GetPreInitFlags;
        provider.GetGraphicsAdapterId = GetGraphicsAdapterId;
        provider.GetVulkanInstanceExtensions = GetVulkanInstanceExtensions;
        provider.GetVulkanDeviceExtensions = GetVulkanDeviceExtensions;

        preInit->RegisterPreInitProvider(&provider);

        XR_TRACE(PLUGIN_LOG_PREFIX "XRPreInitProvider registered\n");
    }

    catch (std::exception const &e) {
        OutputDebugStringA(__FUNCTION__ ": Exception ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
    }
}
