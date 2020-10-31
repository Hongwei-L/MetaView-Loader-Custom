// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/com.valve.openvr/Runtime/DeviceLayouts.cs
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#if ENABLE_VR && UNITY_INPUT_SYSTEM && !PACKAGE_DOCS_GENERATION
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.Controls;
using UnityEngine.InputSystem.Layouts;
using UnityEngine.InputSystem.XR;
using UnityEngine.Scripting;

namespace Unity.XR.MetaView
{
    [InputControlLayout(displayName = "HMD")]
    [Preserve]
    public class HMD : XRHMD
    {
        [InputControl(noisy = true)]
        [Preserve]
        public Vector3Control deviceVelocity { get; private set; }
        [InputControl(noisy = true)]
        [Preserve]
        public Vector3Control deviceAngularVelocity { get; private set; }
        [InputControl(noisy = true)]
        [Preserve]
        public Vector3Control leftEyeVelocity { get; private set; }
        [InputControl(noisy = true)]
        [Preserve]
        public Vector3Control leftEyeAngularVelocity { get; private set; }
        [InputControl(noisy = true)]
        [Preserve]
        public Vector3Control rightEyeVelocity { get; private set; }
        [InputControl(noisy = true)]
        [Preserve]
        public Vector3Control rightEyeAngularVelocity { get; private set; }
        [InputControl(noisy = true)]
        [Preserve]
        public Vector3Control centerEyeVelocity { get; private set; }
        [InputControl(noisy = true)]
        [Preserve]
        public Vector3Control centerEyeAngularVelocity { get; private set; }

        protected override void FinishSetup()
        {
            base.FinishSetup();

            deviceVelocity = GetChildControl<Vector3Control>("deviceVelocity");
            deviceAngularVelocity = GetChildControl<Vector3Control>("deviceAngularVelocity");

            leftEyeVelocity = GetChildControl<Vector3Control>("leftEyeVelocity");
            leftEyeAngularVelocity = GetChildControl<Vector3Control>("leftEyeAngularVelocity");
            rightEyeVelocity = GetChildControl<Vector3Control>("rightEyeVelocity");
            rightEyeAngularVelocity = GetChildControl<Vector3Control>("rightEyeAngularVelocity");
            centerEyeVelocity = GetChildControl<Vector3Control>("centerEyeVelocity");
            centerEyeAngularVelocity = GetChildControl<Vector3Control>("centerEyeAngularVelocity");
        }
    }
}
#endif
