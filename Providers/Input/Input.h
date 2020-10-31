// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/Providers/Input/Input.h
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "CommonTypes.h"
#include "OpenVRProviderContext.h"
#include "OpenVRSystem.h"
#include "ProviderInterface/IUnityXRInput.h"
#include "ProviderInterface/XRMath.h"
#include "Shared.h"
#include "Singleton.h"

#include <optional>
#include <string>
#include <vector>

bool RegisterInputLifecycleProvider(
    OpenVRProviderContext *pOpenProviderContext);

struct TrackedPose {
    bool isTracked = false;
    XRVector3 position{};
    XRQuaternion orientation{};
    XRVector3 velocity{};
    XRVector3 angularVelocity{};
};

class MetaViewInputProvider : public Singleton<MetaViewInputProvider> {
  public:
    MetaViewInputProvider();

    UnitySubsystemErrorCode Tick(UnitySubsystemHandle handle,
                                 UnityXRInputUpdateType updateType);
    UnitySubsystemErrorCode FillDeviceDefinition(
        UnitySubsystemHandle handle, UnityXRInternalInputDeviceId deviceId,
        UnityXRInputDeviceDefinition *deviceDefinition);
    UnitySubsystemErrorCode UpdateDeviceState(
        UnitySubsystemHandle handle, UnityXRInternalInputDeviceId deviceId,
        UnityXRInputUpdateType updateType,
        UnityXRInputDeviceState *deviceState);
    UnitySubsystemErrorCode HandleEvent(UnitySubsystemHandle handle,
                                        unsigned int eventType,
                                        UnityXRInternalInputDeviceId deviceId,
                                        void *buffer, unsigned int size);

    UnitySubsystemErrorCode HandleRecenter(UnitySubsystemHandle handle);
    UnitySubsystemErrorCode HandleHapticImpulse(
        UnitySubsystemHandle handle, UnityXRInternalInputDeviceId deviceId,
        int channel, float amplitude, float duration);
    UnitySubsystemErrorCode QueryHapticCapabilities(
        UnitySubsystemHandle handle, UnityXRInternalInputDeviceId deviceId,
        UnityXRHapticCapabilities *capabilities);
    UnitySubsystemErrorCode HandleHapticStop(
        UnitySubsystemHandle handle, UnityXRInternalInputDeviceId deviceId);

    UnitySubsystemErrorCode QueryTrackingOriginMode(
        UnitySubsystemHandle handle,
        UnityXRInputTrackingOriginModeFlags *trackingOriginMode);
    UnitySubsystemErrorCode QuerySupportedTrackingOriginModes(
        UnitySubsystemHandle handle,
        UnityXRInputTrackingOriginModeFlags *supportedTrackingOriginModes);
    UnitySubsystemErrorCode HandleSetTrackingOriginMode(
        UnitySubsystemHandle handle,
        UnityXRInputTrackingOriginModeFlags trackingOriginMode);

    UnitySubsystemErrorCode TryGetDeviceStateAtTime(
        UnitySubsystemHandle handle, UnityXRTimeStamp time,
        UnityXRInternalInputDeviceId deviceId, UnityXRInputDeviceState *state);

    UnitySubsystemErrorCode Start();
    void Stop(UnitySubsystemHandle handle);

    void GfxThread_UpdateDevices();

  private:
    enum class EDeviceStatus { None, Connect, Disconnect };

    enum class HMDFeature {
        TrackingState,
        IsTracked,
        DevicePosition,
        DeviceRotation,
        DeviceVelocity,
        DeviceAngularVelocity,
        LeftEyePosition,
        LeftEyeRotation,
        LeftEyeVelocity,
        LeftEyeAngularVelocity,
        RightEyePosition,
        RightEyeRotation,
        RightEyeVelocity,
        RightEyeAngularVelocity,
        CenterEyePosition,
        CenterEyeRotation,
        CenterEyeVelocity,
        CenterEyeAngularVelocity,
        Total
    };

    enum class ControllerFeature {
        TrackingState,
        IsTracked,
        DevicePosition,
        DeviceRotation,
        DeviceVelocity,
        DeviceAngularVelocity,
#if 0
        Primary2DAxis,
        Primary2DAxisClick,
        Primary2DAxisTouch,
        Secondary2DAxis,
        Secondary2DAxisTouch,
        Secondary2DAxisClick,
        Trigger,
        TriggerButton,
        TriggerTouch,
        Grip,
        GripButton,
        GripTouch,
        GripGrab,
        GripCapacitive,
        Primary,
        PrimaryButton,
        PrimaryTouch,
        Secondary,
        SecondaryButton,
        SecondaryTouch,
        MenuButton,
        MenuTouch,
        BumperButton,
        Tip,
        TipButton,
        TipTouch,
#endif
        Total
    };

    enum class TrackerFeature {
        TrackingState,
        IsTracked,
        DevicePosition,
        DeviceRotation,
        DeviceVelocity,
        DeviceAngularVelocity,
        Total
    };

    static int hmdFeatureIndices[static_cast<int>(HMDFeature::Total)];
    static int
        controllerFeatureIndices[static_cast<int>(ControllerFeature::Total)];
    static int trackerFeatureIndices[static_cast<int>(TrackerFeature::Total)];
    static const int kUnityXRInputUpdateTypeCount = 2;
    bool m_Started = false;

    struct TrackedDevice {
        UnityXRInternalInputDeviceId deviceId =
            kUnityInvalidXRInternalInputDeviceId;
        uint32_t nativeDeviceIndex = 0;
        UnityXRInputDeviceCharacteristics characteristics =
            kUnityXRInputDeviceCharacteristicsNone;
        EDeviceStatus deviceStatus = EDeviceStatus::None;
        EDeviceStatus deviceChangeForNextUpdate = EDeviceStatus::None;
        TrackedPose trackingPose[kUnityXRInputUpdateTypeCount];

        TrackedDevice(uint32_t index, UnityXRInternalInputDeviceId id,
                      UnityXRInputDeviceCharacteristics characteristics)
            : deviceId(id),
              nativeDeviceIndex(index),
              characteristics(characteristics),
              deviceStatus(EDeviceStatus::None),
              deviceChangeForNextUpdate(EDeviceStatus::Connect),
              trackingPose() {}

        std::optional<std::string> GetDeviceName() const;
    };

    std::vector<MetaViewInputProvider::TrackedDevice> m_TrackedDevices;

    inline TrackedDevice *GetTrackedDeviceByDeviceId(
        UnityXRInternalInputDeviceId id) {
        for (auto &trackedDevice : m_TrackedDevices) {
            if (trackedDevice.deviceId == id) return &trackedDevice;
        }
        return nullptr;
    }
    inline TrackedDevice *GetTrackedDeviceByNativeIndex(uint32_t idx) {
        for (auto &trackedDevice : m_TrackedDevices) {
            if (trackedDevice.nativeDeviceIndex == idx) return &trackedDevice;
        }
        return nullptr;
    }

    UnitySubsystemErrorCode GetHapticCapabilities(
        UnityXRInternalInputDeviceId deviceId,
        UnityXRHapticCapabilities *capabilities);
    UnityXRInternalInputDeviceId GenerateUniqueDeviceId() const;

    void GfxThread_UpdateConnectedDevices(
        const TrackedPose *currentDevicePoses);
    static UnityXRMatrix4x4 GetEyeTransform(EEye eye);
    UnitySubsystemErrorCode Internal_UpdateDeviceState(
        UnitySubsystemHandle handle, const TrackedDevice &device,
        const TrackedPose &trackingPose, UnityXRInputDeviceState *deviceState,
        bool updateNonTrackingData);
    static void InternalToUnityTracking(
        const TrackedPose &internalPose,
        std::optional<UnityXRMatrix4x4> postTransform,
        UnityXRVector3 &outPosition, UnityXRVector4 &outRotation,
        UnityXRVector3 &outVelocity, UnityXRVector3 &outAngularVelocity);
    void GfxThread_CopyPoses(const TrackedPose *currentDevicePoses,
                             const TrackedPose *futureDevicePoses);
};
