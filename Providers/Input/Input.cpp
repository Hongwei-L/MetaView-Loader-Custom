// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/Providers/Input/Input.cpp
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#include "Input.h"

#include "Metadata.h"
#include "ProviderInterface/XRMath.h"
#include "UnityInterfaces.h"
#include "Util.h"

#include "UserProjectSettings.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <sstream>

static constexpr auto ManufacturerName = "Meta View, Inc.";

static OpenVRProviderContext *s_pProviderContext;
static IUnityXRInputInterface *s_Input = nullptr;
int MetaViewInputProvider::hmdFeatureIndices[static_cast<int>(
    HMDFeature::Total)] = {};
int MetaViewInputProvider::controllerFeatureIndices[static_cast<int>(
    ControllerFeature::Total)] = {};
int MetaViewInputProvider::trackerFeatureIndices[static_cast<int>(
    TrackerFeature::Total)] = {};

const static unsigned int kHapticsNumChannels = 1;

// Han Custom Code
// this IPD can be changed in runtime.
static float SinglePassInstancedCamIPD = NominalIpd;

static UnitySubsystemErrorCode UNITY_INTERFACE_API
Tick(UnitySubsystemHandle handle, void *userData,
     UnityXRInputUpdateType updateType) {
    MetaViewInputProvider *input = (MetaViewInputProvider *)userData;

    return input->Tick(handle, updateType);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
FillDeviceDefinition(UnitySubsystemHandle handle, void *userData,
                     UnityXRInternalInputDeviceId deviceId,
                     UnityXRInputDeviceDefinition *deviceDefinition) {
    MetaViewInputProvider *input = (MetaViewInputProvider *)userData;

    return input->FillDeviceDefinition(handle, deviceId, deviceDefinition);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API UpdateDeviceState(
    UnitySubsystemHandle handle, void *userData,
    UnityXRInternalInputDeviceId deviceId, UnityXRInputUpdateType updateType,
    UnityXRInputDeviceState *deviceState) {
    if (!userData) return kUnitySubsystemErrorCodeInvalidArguments;

    MetaViewInputProvider *input = (MetaViewInputProvider *)userData;

    if (!input) return kUnitySubsystemErrorCodeInvalidArguments;

    return input->UpdateDeviceState(handle, deviceId, updateType, deviceState);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API HandleEvent(
    UnitySubsystemHandle handle, void *userData, unsigned int eventType,
    UnityXRInternalInputDeviceId deviceId, void *buffer, unsigned int size) {
    if (!userData) return kUnitySubsystemErrorCodeInvalidArguments;

    MetaViewInputProvider *input = (MetaViewInputProvider *)userData;

    if (!input) return kUnitySubsystemErrorCodeInvalidArguments;

    return input->HandleEvent(handle, eventType, deviceId, buffer, size);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
HandleRecenter(UnitySubsystemHandle handle, void *userData) {
    if (!userData) return kUnitySubsystemErrorCodeInvalidArguments;

    MetaViewInputProvider *input = (MetaViewInputProvider *)userData;

    if (!input) return kUnitySubsystemErrorCodeInvalidArguments;

    return input->HandleRecenter(handle);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
HandleHapticImpulse(UnitySubsystemHandle handle, void *userData,
                    UnityXRInternalInputDeviceId deviceId, int channel,
                    float amplitude, float duration) {
    // Haptics are unsupported
    return kUnitySubsystemErrorCodeFailure;
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
HandleHapticBuffer(UnitySubsystemHandle handle, void *userData,
                   UnityXRInternalInputDeviceId deviceId, int channel,
                   unsigned int bufferSize, const unsigned char *const buffer) {
    // Haptics are unsupported
    return kUnitySubsystemErrorCodeFailure;
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
QueryHapticCapabilities(UnitySubsystemHandle handle, void *userData,
                        UnityXRInternalInputDeviceId deviceId,
                        UnityXRHapticCapabilities *capabilities) {
    if (!userData) return kUnitySubsystemErrorCodeInvalidArguments;

    MetaViewInputProvider *input = (MetaViewInputProvider *)userData;

    if (!input) return kUnitySubsystemErrorCodeInvalidArguments;

    return input->QueryHapticCapabilities(handle, deviceId, capabilities);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
HandleHapticStop(UnitySubsystemHandle handle, void *userData,
                 UnityXRInternalInputDeviceId deviceId) {
    // Haptics are unsupported
    return kUnitySubsystemErrorCodeFailure;
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API QueryTrackingOriginMode(
    UnitySubsystemHandle handle, void *userData,
    UnityXRInputTrackingOriginModeFlags *trackingOriginMode) {
    if (!userData) return kUnitySubsystemErrorCodeInvalidArguments;

    MetaViewInputProvider *input = (MetaViewInputProvider *)userData;

    if (!input) return kUnitySubsystemErrorCodeInvalidArguments;

    return input->QueryTrackingOriginMode(handle, trackingOriginMode);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
QuerySupportedTrackingOriginModes(
    UnitySubsystemHandle handle, void *userData,
    UnityXRInputTrackingOriginModeFlags *supportedTrackingOriginModes) {
    if (!userData) return kUnitySubsystemErrorCodeInvalidArguments;

    MetaViewInputProvider *input = (MetaViewInputProvider *)userData;

    if (!input) return kUnitySubsystemErrorCodeInvalidArguments;

    return input->QuerySupportedTrackingOriginModes(
        handle, supportedTrackingOriginModes);
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API HandleSetTrackingOriginMode(
    UnitySubsystemHandle handle, void *userData,
    UnityXRInputTrackingOriginModeFlags trackingOriginMode) {
    if (!userData) return kUnitySubsystemErrorCodeInvalidArguments;

    MetaViewInputProvider *input = (MetaViewInputProvider *)userData;

    if (!input) return kUnitySubsystemErrorCodeInvalidArguments;

    return input->HandleSetTrackingOriginMode(handle, trackingOriginMode);
}

UnitySubsystemErrorCode UNITY_INTERFACE_API TryGetDeviceStateAtTime(
    UnitySubsystemHandle handle, void *userData, UnityXRTimeStamp time,
    UnityXRInternalInputDeviceId deviceId, UnityXRInputDeviceState *state) {
    if (!userData) return kUnitySubsystemErrorCodeInvalidArguments;

    MetaViewInputProvider *input = (MetaViewInputProvider *)userData;

    if (!input) return kUnitySubsystemErrorCodeInvalidArguments;

    return input->TryGetDeviceStateAtTime(handle, time, deviceId, state);
}

static inline UnityXRInputDeviceCharacteristics
GetCharacteristicsForDeviceIndex(uint32_t deviceIndex) {
    /**
     * @todo just hard-coding based on device index for now.
     */
    switch (deviceIndex) {
        case 0:
            // HMD
            return (UnityXRInputDeviceCharacteristics)(
                kUnityXRInputDeviceCharacteristicsHeadMounted |
                kUnityXRInputDeviceCharacteristicsTrackedDevice);
        case 1:
            // tracking reference
            return (UnityXRInputDeviceCharacteristics)(
                kUnityXRInputDeviceCharacteristicsTrackingReference |
                kUnityXRInputDeviceCharacteristicsTrackedDevice);
        case 2:
            // ambidextrous controller
            return (UnityXRInputDeviceCharacteristics)(
                kUnityXRInputDeviceCharacteristicsTrackedDevice |
                kUnityXRInputDeviceCharacteristicsController |
                kUnityXRInputDeviceCharacteristicsHeldInHand);
        case 3:
            // left controller
            return (UnityXRInputDeviceCharacteristics)(
                kUnityXRInputDeviceCharacteristicsHeldInHand |
                kUnityXRInputDeviceCharacteristicsController |
                kUnityXRInputDeviceCharacteristicsTrackedDevice |
                kUnityXRInputDeviceCharacteristicsLeft);
        case 4:
            // right controller
            return (UnityXRInputDeviceCharacteristics)(
                kUnityXRInputDeviceCharacteristicsHeldInHand |
                kUnityXRInputDeviceCharacteristicsController |
                kUnityXRInputDeviceCharacteristicsTrackedDevice |
                kUnityXRInputDeviceCharacteristicsRight);

        default:
            XR_TRACE(
                "Get characteristics for device with invalid native index. "
                "DeviceIndex: %d\n",
                deviceIndex);
            break;
    }

    return kUnityXRInputDeviceCharacteristicsNone;
}

MetaViewInputProvider::MetaViewInputProvider() {
    // we want to add the HMD only right now.

    uint32_t index = 0;

    m_TrackedDevices.emplace_back(index, index,
                                  GetCharacteristicsForDeviceIndex(index));
}

UnitySubsystemErrorCode MetaViewInputProvider::Tick(
    UnitySubsystemHandle handle, UnityXRInputUpdateType updateType) {
    OpenVRSystem::Get().Update();

    if (updateType == kUnityXRInputUpdateTypeBeforeRender)
        return kUnitySubsystemErrorCodeSuccess;

    // Connect/Disconnect devices marked for change
    for (auto deviceIter = m_TrackedDevices.begin();
         deviceIter != m_TrackedDevices.end();) {
        if (deviceIter->deviceStatus == EDeviceStatus::None &&
            deviceIter->deviceChangeForNextUpdate == EDeviceStatus::Connect) {
            s_Input->InputSubsystem_DeviceConnected(handle,
                                                    deviceIter->deviceId);
            deviceIter->deviceChangeForNextUpdate = EDeviceStatus::None;
            deviceIter->deviceStatus = EDeviceStatus::Connect;
            XR_TRACE(PLUGIN_LOG_PREFIX
                     "Device connected (status change). Handle: %d. "
                     "NativeIndex: %d. UnityID: %d\n",
                     handle, deviceIter->nativeDeviceIndex,
                     deviceIter->deviceId);
        }

        if (deviceIter->deviceChangeForNextUpdate ==
            EDeviceStatus::Disconnect) {
            if (deviceIter->deviceStatus == EDeviceStatus::Connect) {
                s_Input->InputSubsystem_DeviceDisconnected(
                    handle, deviceIter->deviceId);
                XR_TRACE(PLUGIN_LOG_PREFIX
                         "Device disconnected (status change). Handle: %d. "
                         "NativeIndex: %d. UnityID: %d\n",
                         handle, deviceIter->nativeDeviceIndex,
                         deviceIter->deviceId);
            }
            deviceIter = m_TrackedDevices.erase(deviceIter);
        } else
            ++deviceIter;
    }

    return kUnitySubsystemErrorCodeSuccess;
}

std::optional<std::string> MetaViewInputProvider::TrackedDevice::GetDeviceName()
    const {
    // Headset
    if ((characteristics & kUnityXRInputDeviceCharacteristicsHeadMounted) ==
        kUnityXRInputDeviceCharacteristicsHeadMounted) {
        return {std::string{VENDOR_NAME_DEFINE " Headset"}};
    }

    // Controller
    if ((characteristics & kUnityXRInputDeviceCharacteristicsHeldInHand) ==
            kUnityXRInputDeviceCharacteristicsHeldInHand ||
        (characteristics & kUnityXRInputDeviceCharacteristicsController) ==
            kUnityXRInputDeviceCharacteristicsController) {
        if ((characteristics & kUnityXRInputDeviceCharacteristicsLeft) ==
            kUnityXRInputDeviceCharacteristicsLeft) {
            return {std::string{"Left " VENDOR_NAME_DEFINE " Controller"}};
        }
        if ((characteristics & kUnityXRInputDeviceCharacteristicsRight) ==
            kUnityXRInputDeviceCharacteristicsRight) {
            return {std::string{"Right " VENDOR_NAME_DEFINE " Controller"}};
        }
        return {std::string{VENDOR_NAME_DEFINE " Controller"}};
    }

    // Tracking Reference
    if ((characteristics &
         kUnityXRInputDeviceCharacteristicsTrackingReference) ==
        kUnityXRInputDeviceCharacteristicsTrackingReference) {
        return {std::string{VENDOR_NAME_DEFINE " Tracking Reference"}};
    }

    // Misc tracked device
    if ((characteristics & kUnityXRInputDeviceCharacteristicsTrackedDevice) ==
        kUnityXRInputDeviceCharacteristicsTrackedDevice) {
        return {std::string{VENDOR_NAME_DEFINE " Tracked Device"}};
    }

    // something else?
    return std::nullopt;
}

UnitySubsystemErrorCode MetaViewInputProvider::FillDeviceDefinition(
    UnitySubsystemHandle handle, UnityXRInternalInputDeviceId deviceId,
    UnityXRInputDeviceDefinition *deviceDefinition) {
    auto device = GetTrackedDeviceByDeviceId(deviceId);
    if (!device) return kUnitySubsystemErrorCodeFailure;

    s_Input->DeviceDefinition_SetManufacturer(deviceDefinition,
                                              ManufacturerName);

    auto deviceName = device->GetDeviceName();
    if (!deviceName) return kUnitySubsystemErrorCodeFailure;

    XR_TRACE(PLUGIN_LOG_PREFIX
             "Found device NativeIndex:(%d) UnityIndex:(%d) with name: (%s)\n",
             device->nativeDeviceIndex, deviceId, deviceName.value().c_str());

    s_Input->DeviceDefinition_SetName(deviceDefinition, deviceName->c_str());
    s_Input->DeviceDefinition_SetCharacteristics(deviceDefinition,
                                                 device->characteristics);
    s_Input->DeviceDefinition_SetCanQueryForDeviceStateAtTime(deviceDefinition,
                                                              true);

    if ((device->characteristics &
         kUnityXRInputDeviceCharacteristicsHeadMounted) ==
        kUnityXRInputDeviceCharacteristicsHeadMounted) {
        hmdFeatureIndices[static_cast<int>(HMDFeature::TrackingState)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Tracking State",
                kUnityXRInputFeatureTypeDiscreteStates,
                kUnityXRInputFeatureUsageTrackingState);
        hmdFeatureIndices[static_cast<int>(HMDFeature::IsTracked)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Is Tracked", kUnityXRInputFeatureTypeBinary,
                kUnityXRInputFeatureUsageIsTracked);
        hmdFeatureIndices[static_cast<int>(HMDFeature::DevicePosition)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Position",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDevicePosition);
        hmdFeatureIndices[static_cast<int>(HMDFeature::DeviceRotation)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Rotation",
                kUnityXRInputFeatureTypeRotation,
                kUnityXRInputFeatureUsageDeviceRotation);
        hmdFeatureIndices[static_cast<int>(HMDFeature::DeviceVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Velocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDeviceVelocity);
        hmdFeatureIndices[static_cast<int>(HMDFeature::DeviceAngularVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - AngularVelocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDeviceAngularVelocity);
        hmdFeatureIndices[static_cast<int>(HMDFeature::LeftEyePosition)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "LeftEye - Position",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageLeftEyePosition);
        hmdFeatureIndices[static_cast<int>(HMDFeature::LeftEyeRotation)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "LeftEye - Rotation",
                kUnityXRInputFeatureTypeRotation,
                kUnityXRInputFeatureUsageLeftEyeRotation);
        hmdFeatureIndices[static_cast<int>(HMDFeature::LeftEyeVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "LeftEye - Velocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageLeftEyeVelocity);
        hmdFeatureIndices[static_cast<int>(
            HMDFeature::LeftEyeAngularVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "LeftEye - AngularVelocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageLeftEyeAngularVelocity);
        hmdFeatureIndices[static_cast<int>(HMDFeature::RightEyePosition)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "RightEye - Position",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageRightEyePosition);
        hmdFeatureIndices[static_cast<int>(HMDFeature::RightEyeRotation)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "RightEye - Rotation",
                kUnityXRInputFeatureTypeRotation,
                kUnityXRInputFeatureUsageRightEyeRotation);
        hmdFeatureIndices[static_cast<int>(HMDFeature::RightEyeVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "RightEye - Velocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageRightEyeVelocity);
        hmdFeatureIndices[static_cast<int>(
            HMDFeature::RightEyeAngularVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "RightEye - AngularVelocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageRightEyeAngularVelocity);
        hmdFeatureIndices[static_cast<int>(HMDFeature::CenterEyePosition)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "CenterEye - Position",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageCenterEyePosition);
        hmdFeatureIndices[static_cast<int>(HMDFeature::CenterEyeRotation)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "CenterEye - Rotation",
                kUnityXRInputFeatureTypeRotation,
                kUnityXRInputFeatureUsageCenterEyeRotation);
        hmdFeatureIndices[static_cast<int>(HMDFeature::CenterEyeVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "CenterEye - Velocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageCenterEyeVelocity);
        hmdFeatureIndices[static_cast<int>(
            HMDFeature::CenterEyeAngularVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "CenterEye - AngularVelocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageCenterEyeAngularVelocity);
    } else if ((device->characteristics &
                kUnityXRInputDeviceCharacteristicsHeldInHand) ==
               kUnityXRInputDeviceCharacteristicsHeldInHand) {
        controllerFeatureIndices[static_cast<int>(
            ControllerFeature::DevicePosition)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Position",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDevicePosition);
        controllerFeatureIndices[static_cast<int>(
            ControllerFeature::DeviceRotation)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Rotation",
                kUnityXRInputFeatureTypeRotation,
                kUnityXRInputFeatureUsageDeviceRotation);
        controllerFeatureIndices[static_cast<int>(
            ControllerFeature::DeviceVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Velocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDeviceVelocity);
        controllerFeatureIndices[static_cast<int>(
            ControllerFeature::DeviceAngularVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - AngularVelocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDeviceAngularVelocity);
        controllerFeatureIndices[static_cast<int>(
            ControllerFeature::TrackingState)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "TrackingState",
                kUnityXRInputFeatureTypeDiscreteStates,
                kUnityXRInputFeatureUsageTrackingState);
        controllerFeatureIndices[static_cast<int>(
            ControllerFeature::IsTracked)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "IsTracked", kUnityXRInputFeatureTypeBinary,
                kUnityXRInputFeatureUsageIsTracked);
    } else if ((device->characteristics &
                kUnityXRInputDeviceCharacteristicsTrackingReference) ==
               kUnityXRInputDeviceCharacteristicsTrackingReference) {
        trackerFeatureIndices[static_cast<int>(
            TrackerFeature::DevicePosition)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Position",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDevicePosition);
        trackerFeatureIndices[static_cast<int>(
            TrackerFeature::DeviceRotation)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Rotation",
                kUnityXRInputFeatureTypeRotation,
                kUnityXRInputFeatureUsageDeviceRotation);
        trackerFeatureIndices[static_cast<int>(
            TrackerFeature::DeviceVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Velocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDeviceVelocity);
        trackerFeatureIndices[static_cast<int>(
            TrackerFeature::DeviceAngularVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - AngularVelocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDeviceAngularVelocity);
        trackerFeatureIndices[static_cast<int>(TrackerFeature::TrackingState)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "TrackingState",
                kUnityXRInputFeatureTypeDiscreteStates,
                kUnityXRInputFeatureUsageTrackingState);
        trackerFeatureIndices[static_cast<int>(TrackerFeature::IsTracked)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "IsTracked", kUnityXRInputFeatureTypeBinary,
                kUnityXRInputFeatureUsageIsTracked);
    } else if ((device->characteristics &
                kUnityXRInputDeviceCharacteristicsTrackedDevice) ==
               kUnityXRInputDeviceCharacteristicsTrackedDevice) {
        trackerFeatureIndices[static_cast<int>(
            TrackerFeature::DevicePosition)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Position",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDevicePosition);
        trackerFeatureIndices[static_cast<int>(
            TrackerFeature::DeviceRotation)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Rotation",
                kUnityXRInputFeatureTypeRotation,
                kUnityXRInputFeatureUsageDeviceRotation);
        trackerFeatureIndices[static_cast<int>(
            TrackerFeature::DeviceVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - Velocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDeviceVelocity);
        trackerFeatureIndices[static_cast<int>(
            TrackerFeature::DeviceAngularVelocity)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "Device - AngularVelocity",
                kUnityXRInputFeatureTypeAxis3D,
                kUnityXRInputFeatureUsageDeviceAngularVelocity);
        trackerFeatureIndices[static_cast<int>(TrackerFeature::TrackingState)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "TrackingState",
                kUnityXRInputFeatureTypeDiscreteStates,
                kUnityXRInputFeatureUsageTrackingState);
        trackerFeatureIndices[static_cast<int>(TrackerFeature::IsTracked)] =
            s_Input->DeviceDefinition_AddFeatureWithUsage(
                deviceDefinition, "IsTracked", kUnityXRInputFeatureTypeBinary,
                kUnityXRInputFeatureUsageIsTracked);
    }

    return kUnitySubsystemErrorCodeSuccess;
}

static void OpenVRMatrix3x4ToUnity(const vr::HmdMatrix34_t &ovrm,
                                   UnityXRMatrix4x4 &out) {
    float tmpMatrix[] = {ovrm.m[0][0], ovrm.m[1][0], ovrm.m[2][0], 0.0f,
                         ovrm.m[0][1], ovrm.m[1][1], ovrm.m[2][1], 0.0f,
                         ovrm.m[0][2], ovrm.m[1][2], ovrm.m[2][2], 0.0f,
                         ovrm.m[0][3], ovrm.m[1][3], ovrm.m[2][3], 1.0f};

    out.columns[0].x = tmpMatrix[(0 * 4) + 0];
    out.columns[1].x = tmpMatrix[(1 * 4) + 0];
    out.columns[2].x = -tmpMatrix[(2 * 4) + 0];
    out.columns[3].x = tmpMatrix[(3 * 4) + 0];

    out.columns[0].y = tmpMatrix[(0 * 4) + 1];
    out.columns[1].y = tmpMatrix[(1 * 4) + 1];
    out.columns[2].y = -tmpMatrix[(2 * 4) + 1];
    out.columns[3].y = tmpMatrix[(3 * 4) + 1];

    out.columns[0].z = -tmpMatrix[(0 * 4) + 2];
    out.columns[1].z = -tmpMatrix[(1 * 4) + 2];
    out.columns[2].z = tmpMatrix[(2 * 4) + 2];
    out.columns[3].z = -tmpMatrix[(3 * 4) + 2];

    out.columns[0].w = tmpMatrix[(0 * 4) + 3];
    out.columns[1].w = tmpMatrix[(1 * 4) + 3];
    out.columns[2].w = -tmpMatrix[(2 * 4) + 3];
    out.columns[3].w = tmpMatrix[(3 * 4) + 3];
}

static inline XRMatrix4x4 quatToMatrix(XRQuaternion const &q) {
    //! @todo handedness change needed because of Unity being left-handed?
    const float w = q.w;
    const float x = q.x;
    const float y = q.y;
    const float z = q.z;
    const float x2 = x * x;
    const float y2 = y * y;
    const float z2 = z * z;
    const float w2 = w * w;
    const float xy = x * y;
    const float xz = x * z;
    const float wz = w * z;
    const float yz = y * z;
    const float wx = w * x;
    return XRMatrix4x4(1 - 2 * y2 - 2 * z2,    // m00
                       2 * xy + 2 * wz,        // m01
                       2 * xz - 2 * w * y,     // m02
                       0,                      // m03
                       2 * xy - 2 * wz,        // m10
                       1.f - 2 * x2 - 2 * z2,  // m11
                       2 * yz + 2 * wx,        // m12
                       0,                      // m13
                       2 * xz + 2 * w * y,     // m20
                       2 * yz - 2 * wx,        // m21
                       1.f - 2 * x2 - 2 * y2,  // m22
                       0,                      // m23
                       0,                      // m30
                       0,                      // m31
                       0,                      // m32
                       1                       // m33
    );
}

void MetaViewInputProvider::InternalToUnityTracking(
    const TrackedPose &internalPose,
    std::optional<UnityXRMatrix4x4> postTransform, UnityXRVector3 &outPosition,
    UnityXRVector4 &outRotation, UnityXRVector3 &outVelocity,
    UnityXRVector3 &outAngularVelocity) {
    XRMatrix4x4 trackingToReference = quatToMatrix(internalPose.orientation);
    trackingToReference.SetCol(
        3, XRVector4(internalPose.position.x, internalPose.position.y,
                     internalPose.position.z, 1));

    if (postTransform) {
        trackingToReference *= reinterpret_cast<XRMatrix4x4 &>(*postTransform);
    }

    outPosition = trackingToReference.Translation();

    XRQuaternion xrQuaternion;
    MatrixToQuaternion(trackingToReference, xrQuaternion);
    outRotation = xrQuaternion;
    outRotation.w *= -1;

    //! @todo transform the velocity?
    outVelocity = internalPose.velocity;
    outVelocity.z *= -1;

    outAngularVelocity = internalPose.angularVelocity;
    outAngularVelocity.z *= -1;
}

static inline XRMatrix4x4 MakeTranslation(float x, float y, float z) {
    XRMatrix4x4 ret = XRMatrix4x4::identity;
    ret.SetCol(3, XRVector4(x, y, z, 1.f));
    return ret;
}

static inline UnityXRMatrix4x4 GetEyeTransformReal(EEye eye) {
    assert(eye != EEye::CenterOrBoth);
    if (eye == EEye::CenterOrBoth) return XRMatrix4x4::identity;
    switch (eye) {
        case EEye::Left:
            return MakeTranslation(SinglePassInstancedCamIPD / -2.f, 0, 0);
        case EEye::Right:
            return MakeTranslation(SinglePassInstancedCamIPD / 2.f, 0, 0);
        case EEye::CenterOrBoth:
        default:
            return XRMatrix4x4::identity;
    }
}

UnityXRMatrix4x4 MetaViewInputProvider::GetEyeTransform(EEye eye) {
    auto ret = GetEyeTransformReal(eye);
    assert(isMatrixValid(ret));
    return ret;
}

static UnityXRTimeStamp GetCurrentUnityTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch())
                  .count();
    return static_cast<UnityXRTimeStamp>(ms);
}

UnitySubsystemErrorCode MetaViewInputProvider::Internal_UpdateDeviceState(
    UnitySubsystemHandle handle, const TrackedDevice &device,
    const TrackedPose &trackingPose, UnityXRInputDeviceState *deviceState,
    bool updateNonTrackingData) {
    int trackingState = kUnityXRInputTrackingStatePosition |
                        kUnityXRInputTrackingStateRotation |
                        kUnityXRInputTrackingStateVelocity |
                        kUnityXRInputTrackingStateAngularVelocity;

    if (!trackingPose.isTracked) trackingState = kUnityXRInputTrackingStateNone;

    XRVector3 devicePosition, deviceVelocity, deviceAngularVelocity;
    XRQuaternion deviceRotation;
    if ((device.characteristics &
         kUnityXRInputDeviceCharacteristicsHeadMounted) ==
        kUnityXRInputDeviceCharacteristicsHeadMounted) {
        s_Input->DeviceState_SetDiscreteStateValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::TrackingState)],
            trackingState);
        s_Input->DeviceState_SetBinaryValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::IsTracked)],
            trackingPose.isTracked);

        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::DevicePosition)],
            devicePosition);
        s_Input->DeviceState_SetRotationValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::DeviceRotation)],
            deviceRotation);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::DeviceVelocity)],
            deviceVelocity);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(
                HMDFeature::DeviceAngularVelocity)],
            deviceAngularVelocity);

        UnityXRVector3 leftEyePosition, leftEyeVelocity, leftEyeAngularVelocity;
        UnityXRVector4 leftEyeRotation;
        UnityXRMatrix4x4 leftEyeTransform = GetEyeTransform(EEye::Left);
        InternalToUnityTracking(trackingPose, {leftEyeTransform},
                                leftEyePosition, leftEyeRotation,
                                leftEyeVelocity, leftEyeAngularVelocity);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::LeftEyePosition)],
            leftEyePosition);
        s_Input->DeviceState_SetRotationValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::LeftEyeRotation)],
            leftEyeRotation);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::LeftEyeVelocity)],
            leftEyeVelocity);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(
                HMDFeature::LeftEyeAngularVelocity)],
            leftEyeAngularVelocity);

        UnityXRVector3 rightEyePosition, rightEyeVelocity,
            rightEyeAngularVelocity;
        UnityXRVector4 rightEyeRotation;
        UnityXRMatrix4x4 rightEyeTransform = GetEyeTransform(EEye::Right);
        InternalToUnityTracking(trackingPose, {rightEyeTransform},
                                rightEyePosition, rightEyeRotation,
                                rightEyeVelocity, rightEyeAngularVelocity);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::RightEyePosition)],
            rightEyePosition);
        s_Input->DeviceState_SetRotationValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::RightEyeRotation)],
            rightEyeRotation);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::RightEyeVelocity)],
            rightEyeVelocity);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(
                HMDFeature::RightEyeAngularVelocity)],
            rightEyeAngularVelocity);

        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::CenterEyePosition)],
            (leftEyePosition + rightEyePosition) * 0.5f);
        s_Input->DeviceState_SetRotationValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::CenterEyeRotation)],
            XRQuaternion::Lerp(
                reinterpret_cast<XRQuaternion &>(leftEyeRotation),
                reinterpret_cast<XRQuaternion &>(rightEyeRotation), 0.5f));
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(HMDFeature::CenterEyeVelocity)],
            (rightEyeVelocity + rightEyeVelocity) * 0.5f);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            hmdFeatureIndices[static_cast<int>(
                HMDFeature::CenterEyeAngularVelocity)],
            (leftEyeAngularVelocity + rightEyeAngularVelocity) * 0.5f);
    } else if ((device.characteristics &
                kUnityXRInputDeviceCharacteristicsHeldInHand) ==
               kUnityXRInputDeviceCharacteristicsHeldInHand) {
        s_Input->DeviceState_SetDiscreteStateValue(
            deviceState,
            controllerFeatureIndices[static_cast<int>(
                ControllerFeature::TrackingState)],
            trackingState);
        s_Input->DeviceState_SetBinaryValue(
            deviceState,
            controllerFeatureIndices[static_cast<int>(
                ControllerFeature::IsTracked)],
            trackingPose.isTracked);

        InternalToUnityTracking(trackingPose, std::nullopt, devicePosition,
                                deviceRotation, deviceVelocity,
                                deviceAngularVelocity);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            controllerFeatureIndices[static_cast<int>(
                ControllerFeature::DevicePosition)],
            devicePosition);
        s_Input->DeviceState_SetRotationValue(
            deviceState,
            controllerFeatureIndices[static_cast<int>(
                ControllerFeature::DeviceRotation)],
            deviceRotation);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            controllerFeatureIndices[static_cast<int>(
                ControllerFeature::DeviceVelocity)],
            deviceVelocity);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            controllerFeatureIndices[static_cast<int>(
                ControllerFeature::DeviceAngularVelocity)],
            deviceAngularVelocity);
    } else if ((device.characteristics &
                kUnityXRInputDeviceCharacteristicsTrackingReference) ==
               kUnityXRInputDeviceCharacteristicsTrackingReference) {
        s_Input->DeviceState_SetDiscreteStateValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(
                TrackerFeature::TrackingState)],
            trackingState);
        s_Input->DeviceState_SetBinaryValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(TrackerFeature::IsTracked)],
            trackingPose.isTracked);

        InternalToUnityTracking(trackingPose, std::nullopt, devicePosition,
                                deviceRotation, deviceVelocity,
                                deviceAngularVelocity);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(
                TrackerFeature::DevicePosition)],
            devicePosition);
        s_Input->DeviceState_SetRotationValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(
                TrackerFeature::DeviceRotation)],
            deviceRotation);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(
                TrackerFeature::DeviceVelocity)],
            deviceVelocity);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(
                TrackerFeature::DeviceAngularVelocity)],
            deviceAngularVelocity);
    } else if ((device.characteristics &
                kUnityXRInputDeviceCharacteristicsTrackedDevice) ==
               kUnityXRInputDeviceCharacteristicsTrackedDevice) {
        s_Input->DeviceState_SetDiscreteStateValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(
                TrackerFeature::TrackingState)],
            trackingState);
        s_Input->DeviceState_SetBinaryValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(TrackerFeature::IsTracked)],
            trackingPose.isTracked);

        InternalToUnityTracking(trackingPose, std::nullopt, devicePosition,
                                deviceRotation, deviceVelocity,
                                deviceAngularVelocity);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(
                TrackerFeature::DevicePosition)],
            devicePosition);
        s_Input->DeviceState_SetRotationValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(
                TrackerFeature::DeviceRotation)],
            deviceRotation);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(
                TrackerFeature::DeviceVelocity)],
            deviceVelocity);
        s_Input->DeviceState_SetAxis3DValue(
            deviceState,
            trackerFeatureIndices[static_cast<int>(
                TrackerFeature::DeviceAngularVelocity)],
            deviceAngularVelocity);
    }

    return kUnitySubsystemErrorCodeSuccess;
}

UnitySubsystemErrorCode MetaViewInputProvider::UpdateDeviceState(
    UnitySubsystemHandle handle, UnityXRInternalInputDeviceId deviceId,
    UnityXRInputUpdateType updateType, UnityXRInputDeviceState *deviceState) {
    auto device = GetTrackedDeviceByDeviceId(deviceId);
    if (!device) return kUnitySubsystemErrorCodeFailure;

    UnitySubsystemErrorCode errorCode = Internal_UpdateDeviceState(
        handle, *device, device->trackingPose[updateType], deviceState, true);
    if (errorCode != kUnitySubsystemErrorCodeSuccess) return errorCode;

    s_Input->DeviceState_SetDeviceTime(deviceState, GetCurrentUnityTimestamp());
    return kUnitySubsystemErrorCodeSuccess;
}

static UnitySubsystemErrorCode RecenterTrackingOrigin() {
    //! @todo
    return kUnitySubsystemErrorCodeFailure;
}

template <class T>
inline static T clamp(const T &t, const T &t0, const T &t1) {
    return (t < t0) ? t0 : ((t > t1) ? t1 : t);
}

UnitySubsystemErrorCode MetaViewInputProvider::GetHapticCapabilities(
    UnityXRInternalInputDeviceId deviceId,
    UnityXRHapticCapabilities *capabilities) {
    auto device = GetTrackedDeviceByDeviceId(deviceId);
    if (!device) return kUnitySubsystemErrorCodeInvalidArguments;

    capabilities->numChannels = 0;
    capabilities->supportsImpulse = false;
    capabilities->supportsBuffer = false;
    capabilities->bufferFrequencyHz = 0;
    capabilities->bufferMaxSize = 0;
    capabilities->bufferOptimalSize = 0;
    return kUnitySubsystemErrorCodeSuccess;
}

UnitySubsystemErrorCode MetaViewInputProvider::HandleEvent(
    UnitySubsystemHandle handle, unsigned int eventType,
    UnityXRInternalInputDeviceId deviceId, void *buffer, unsigned int size) {
    // No Custom events supported at this time.
    return kUnitySubsystemErrorCodeFailure;
}

UnitySubsystemErrorCode MetaViewInputProvider::HandleRecenter(
    UnitySubsystemHandle handle) {
    return RecenterTrackingOrigin();
}

UnitySubsystemErrorCode MetaViewInputProvider::HandleHapticImpulse(
    UnitySubsystemHandle handle, UnityXRInternalInputDeviceId deviceId,
    int channel, float amplitude, float duration) {
    return kUnitySubsystemErrorCodeFailure;
}

UnitySubsystemErrorCode MetaViewInputProvider::QueryHapticCapabilities(
    UnitySubsystemHandle handle, UnityXRInternalInputDeviceId deviceId,
    UnityXRHapticCapabilities *capabilities) {
    return GetHapticCapabilities(deviceId, capabilities);
}

UnitySubsystemErrorCode MetaViewInputProvider::HandleHapticStop(
    UnitySubsystemHandle handle, UnityXRInternalInputDeviceId deviceId) {
    // not supported by a variety of controllers
    return kUnitySubsystemErrorCodeFailure;
}

UnitySubsystemErrorCode MetaViewInputProvider::QueryTrackingOriginMode(
    UnitySubsystemHandle handle,
    UnityXRInputTrackingOriginModeFlags *trackingOriginMode) {
    *trackingOriginMode = kUnityXRInputTrackingOriginModeFloor;

    return kUnitySubsystemErrorCodeSuccess;
}

UnitySubsystemErrorCode
MetaViewInputProvider::QuerySupportedTrackingOriginModes(
    UnitySubsystemHandle handle,
    UnityXRInputTrackingOriginModeFlags *supportedTrackingOriginModes) {
    *supportedTrackingOriginModes = (UnityXRInputTrackingOriginModeFlags)(

        kUnityXRInputTrackingOriginModeFloor);
    return kUnitySubsystemErrorCodeFailure;
}

UnitySubsystemErrorCode MetaViewInputProvider::HandleSetTrackingOriginMode(
    UnitySubsystemHandle handle,
    UnityXRInputTrackingOriginModeFlags trackingOriginMode) {
    UnityXRInputTrackingOriginModeFlags previousTrackingOriginMode;
    if (QueryTrackingOriginMode(handle, &previousTrackingOriginMode) ==
        kUnitySubsystemErrorCodeFailure)
        return kUnitySubsystemErrorCodeFailure;
#if 0
    vr::ETrackingUniverseOrigin originType;
    switch (trackingOriginMode) {
        case kUnityXRInputTrackingOriginModeDevice:
            originType = vr::TrackingUniverseSeated;
            break;

        case kUnityXRInputTrackingOriginModeFloor:
            originType = vr::TrackingUniverseStanding;
            break;

        default:
            return kUnitySubsystemErrorCodeFailure;
            break;
    }
    OpenVRSystem::Get().GetCompositor()->SetTrackingSpace(originType);
#endif

    if (previousTrackingOriginMode != trackingOriginMode)
        s_Input->InputSubsystem_TrackingOriginUpdated(handle);

    return kUnitySubsystemErrorCodeSuccess;
}

UnitySubsystemErrorCode MetaViewInputProvider::TryGetDeviceStateAtTime(
    UnitySubsystemHandle handle, UnityXRTimeStamp time,
    UnityXRInternalInputDeviceId deviceId, UnityXRInputDeviceState *state) {
    UnityXRTimeStamp currentTimestamp = GetCurrentUnityTimestamp();
    UnityXRTimeStamp timestampDelta = time - currentTimestamp;
    constexpr float kMillisecondsInSecond = 1000.0f;
    float deltaTimeInSeconds =
        static_cast<float>(timestampDelta) / kMillisecondsInSecond;

    TrackedPose trackedDevicesAtTimestamp[vr::k_unMaxTrackedDeviceCount];
#if 0
    vr::ETrackingUniverseOrigin trackingSpace =
        OpenVRSystem::Get().GetCompositor()->GetTrackingSpace();
    OpenVRSystem::Get().GetSystem()->GetDeviceToAbsoluteTrackingPose(
        trackingSpace, deltaTimeInSeconds, trackedDevicesAtTimestamp,
        vr::k_unMaxTrackedDeviceCount);
#endif

    auto device = GetTrackedDeviceByDeviceId(deviceId);
    if (!device) return kUnitySubsystemErrorCodeFailure;

    assert(device->nativeDeviceIndex < vr::k_unMaxTrackedDeviceCount);
    if (device->nativeDeviceIndex >= vr::k_unMaxTrackedDeviceCount)
        return kUnitySubsystemErrorCodeFailure;

    UnitySubsystemErrorCode errorCode = Internal_UpdateDeviceState(
        handle, *device, trackedDevicesAtTimestamp[device->nativeDeviceIndex],
        state, false);
    if (errorCode != kUnitySubsystemErrorCodeSuccess) return errorCode;

    s_Input->DeviceState_SetDeviceTime(state, time);
    return kUnitySubsystemErrorCodeSuccess;
}

UnityXRInternalInputDeviceId MetaViewInputProvider::GenerateUniqueDeviceId()
    const {
    //! @todo this is a very slow way to achieve this
    std::vector<UnityXRInternalInputDeviceId> sortedDeviceIds;
    for (auto device : m_TrackedDevices)
        sortedDeviceIds.emplace_back(device.deviceId);

    std::sort(sortedDeviceIds.begin(),
              sortedDeviceIds.end());  // wasn't necessarily sorted.

    UnityXRInternalInputDeviceId uniqueId = 0;
    for (auto deviceId : sortedDeviceIds) {
        if (deviceId == uniqueId)
            uniqueId++;
        else
            break;
    }

    return uniqueId;
}

void MetaViewInputProvider::GfxThread_UpdateConnectedDevices(
    const TrackedPose *currentDevicePoses) {
    const uint32_t n = static_cast<uint32_t>(m_TrackedDevices.size());
    for (uint32_t arrayIndex = 0; arrayIndex < n; ++arrayIndex) {
        auto existingDevice = &m_TrackedDevices[arrayIndex];

        bool isConnected = true;

        if (!isConnected) {
            // Device was in list but is no longer tracked, mark for disconnect
            existingDevice->deviceChangeForNextUpdate =
                EDeviceStatus::Disconnect;

            XR_TRACE(PLUGIN_LOG_PREFIX
                     "Device disconnecting (disconnection reported). "
                     "NativeIndex: %d. UnityID: %d\n",
                     arrayIndex, existingDevice->deviceId);
        }
    }
}

void MetaViewInputProvider::GfxThread_CopyPoses(
    const TrackedPose *currentDevicePoses,
    const TrackedPose *futureDevicePoses) {
    for (auto &trackedDevice : m_TrackedDevices) {
        trackedDevice.trackingPose[kUnityXRInputUpdateTypeDynamic] =
            futureDevicePoses[trackedDevice.nativeDeviceIndex];
        trackedDevice.trackingPose[kUnityXRInputUpdateTypeBeforeRender] =
            currentDevicePoses[trackedDevice.nativeDeviceIndex];
    }
}

// Called from the graphics thread in post-present to get connected devices and
// update poses. The graphics thread will have a sync fence with the main loop,
// so thread synchronization is not further necessary.
void MetaViewInputProvider::GfxThread_UpdateDevices() {
    if (!m_Started) return;

    TrackedPose trackedDevicesCurrent[vr::k_unMaxTrackedDeviceCount];
    TrackedPose trackedDevicesFuture[vr::k_unMaxTrackedDeviceCount];

    const size_t n = m_TrackedDevices.size();
    for (size_t i = 0; i < n; ++i) {
        //! @todo get tracking here
        trackedDevicesCurrent[i].isTracked = true;
        trackedDevicesFuture[i].isTracked = true;
    }
    GfxThread_UpdateConnectedDevices(trackedDevicesCurrent);
    GfxThread_CopyPoses(trackedDevicesCurrent, trackedDevicesFuture);
}

UnitySubsystemErrorCode MetaViewInputProvider::Start() {
    m_Started = true;

    return kUnitySubsystemErrorCodeSuccess;
}

void MetaViewInputProvider::Stop(UnitySubsystemHandle handle) {
    m_Started = false;

    for (auto deviceIter = m_TrackedDevices.begin();
         deviceIter != m_TrackedDevices.end();) {
        if (deviceIter->deviceStatus == EDeviceStatus::Connect) {
            XR_TRACE(PLUGIN_LOG_PREFIX
                     "Device disconnected (stopping provider). Handle: %d. "
                     "DeviceID: %d\n",
                     handle, deviceIter->deviceId);
            s_Input->InputSubsystem_DeviceDisconnected(handle,
                                                       deviceIter->deviceId);
        }
        deviceIter = m_TrackedDevices.erase(deviceIter);
    }
    m_TrackedDevices.clear();
}

UnitySubsystemErrorCode UNITY_INTERFACE_API
Lifecycle_Initialize(UnitySubsystemHandle handle, void *userData) {
    // Register to the provider context
    if (s_pProviderContext) {
        s_pProviderContext->inputProvider = &MetaViewInputProvider::Get();
    }

    UnityXRInputProvider inputProvider = {0};

    inputProvider.userData = &MetaViewInputProvider::Get();
    inputProvider.Tick = &Tick;
    inputProvider.FillDeviceDefinition = &FillDeviceDefinition;
    inputProvider.UpdateDeviceState = &UpdateDeviceState;
    inputProvider.HandleEvent = &HandleEvent;
    inputProvider.HandleRecenter = &HandleRecenter;
    inputProvider.HandleHapticImpulse = &HandleHapticImpulse;
    inputProvider.HandleHapticBuffer = &HandleHapticBuffer;
    inputProvider.QueryHapticCapabilities = &QueryHapticCapabilities;
    inputProvider.HandleHapticStop = &HandleHapticStop;
    inputProvider.QueryTrackingOriginMode = &QueryTrackingOriginMode;
    inputProvider.QuerySupportedTrackingOriginModes =
        &QuerySupportedTrackingOriginModes;
    inputProvider.HandleSetTrackingOriginMode = &HandleSetTrackingOriginMode;
    inputProvider.TryGetDeviceStateAtTime = &TryGetDeviceStateAtTime;

    s_Input->RegisterInputProvider(handle, &inputProvider);

    return kUnitySubsystemErrorCodeSuccess;
}

static UnitySubsystemErrorCode UNITY_INTERFACE_API
Lifecycle_Start(UnitySubsystemHandle handle, void *userData) {
    return MetaViewInputProvider::Get().Start();
}

static void UNITY_INTERFACE_API Lifecycle_Stop(UnitySubsystemHandle handle,
                                               void *userData) {
    MetaViewInputProvider::Get().Stop(handle);
}

static void UNITY_INTERFACE_API Lifecycle_Shutdown(UnitySubsystemHandle handle,
                                                   void *userData) {}

bool RegisterInputLifecycleProvider(
    OpenVRProviderContext *pOpenProviderContext) {
    XR_TRACE(PLUGIN_LOG_PREFIX "Input lifecycle provider registered\n");

    s_Input = UnityInterfaces::Get().GetInterface<IUnityXRInputInterface>();
    s_pProviderContext = pOpenProviderContext;

    UnityLifecycleProvider inputLifecycleHandler = {0};

    inputLifecycleHandler.userData = nullptr;
    inputLifecycleHandler.Initialize = &Lifecycle_Initialize;
    inputLifecycleHandler.Start = &Lifecycle_Start;
    inputLifecycleHandler.Stop = &Lifecycle_Stop;
    inputLifecycleHandler.Shutdown = &Lifecycle_Shutdown;

    s_Input->RegisterLifecycleProvider(PluginDllName, InputProviderName,
                                       &inputLifecycleHandler);

    return true;
}


//Han Custom Code

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
SetParamsForSinglePassInstancedCameraInputCPP(float IPD) {
    SinglePassInstancedCamIPD = IPD;
}
