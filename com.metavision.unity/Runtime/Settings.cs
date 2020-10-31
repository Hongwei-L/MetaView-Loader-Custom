// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/com.valve.openvr/Runtime/OpenVRSettings.cs
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR;

#if UNITY_XR_MANAGEMENT
using UnityEngine.XR.Management;
#endif

namespace Unity.XR.MetaView
{
#if UNITY_XR_MANAGEMENT
    [XRConfigurationData(PluginMetadata.VendorName, PluginMetadata.SettingsKey)]
#endif
    [System.Serializable]
    public class Settings : ScriptableObject
    {
        public enum StereoRenderingModes
        {
            MultiPass = 0,
            SinglePassInstanced = 1,
            SingleCamera = 2,
        }


        public enum MirrorViewModes
        {
            None = 0,
            Left = 1,
            Right = 2,
            Distort = 3,
        }

        public enum ScanoutOptions
        {
            None = 0,
            Rotate = 1,
        }
        public enum GameViewOptions
        {
            Disable = 0,
            Enable = 1,
        }

        [SerializeField, Tooltip("Set the Stereo Rendering Method")]
        public StereoRenderingModes StereoRenderingMode = StereoRenderingModes.SinglePassInstanced;

        [SerializeField, Tooltip("Which eye to use when rendering the headset view to the main window (none, left, or right)")]
        public MirrorViewModes MirrorView = MirrorViewModes.Right;

        [SerializeField, Tooltip("Whether to rotate eyes before rendering, to compensate for scanout")]
        public ScanoutOptions RotateEyes = ScanoutOptions.Rotate;

        // To modify at runtime, see Settings.SetGameView
        [SerializeField, Tooltip("Whether to also render to the 'Game View' window")]
        public GameViewOptions RenderGameView = GameViewOptions.Enable;

        private static void CreateDirectory(DirectoryInfo directory)
        {
            if (directory.Parent.Exists == false)
                CreateDirectory(directory.Parent);

            if (directory.Exists == false)
                directory.Create();
        }

        public ushort GetStereoRenderingMode()
        {
            return (ushort)StereoRenderingMode;
        }

        public MirrorViewModes GetMirrorViewMode()
        {
            return MirrorView;
        }

        /// <summary>
        /// Sets the mirror view mode (left, right, distort) at runtime.
        /// </summary>
        /// <param name="newMode">left, right, distort</param>
        public void SetMirrorViewMode(MirrorViewModes newMode)
        {
            MirrorView = newMode;
            SetMirrorViewMode((ushort)newMode);
        }

        /// <summary>
        /// Sets the game view settings at runtime.
        /// </summary>
        /// <param name="newOption">enable or disable</param>
        public void SetGameView(GameViewOptions newOption)
        {
            RenderGameView = newOption;
            List<XRDisplaySubsystem> xrDisplayList = new List<XRDisplaySubsystem>();
            SubsystemManager.GetInstances(xrDisplayList);
            if (xrDisplayList.Count == 1)
            {
                XRDisplaySubsystem display = xrDisplayList[0];
                // This line turns off rendering to the default game view.
                display.disableLegacyRenderer = (newOption == GameViewOptions.Disable);
            }
        }

        public static Settings GetSettings(bool create = true)
        {
            Settings settings = null;
#if UNITY_EDITOR
            UnityEditor.EditorBuildSettings.TryGetConfigObject<Settings>(PluginMetadata.SettingsKey, out settings);
#else
            settings = Settings.s_Settings;
#endif

            if (settings == null && create)
                settings = Settings.CreateInstance<Settings>();

            return settings;
        }

        [DllImport(PluginMetadata.PluginDllName, CharSet = CharSet.Auto)]
        public static extern void SetMirrorViewMode(ushort mirrorViewMode);

        [DllImport(PluginMetadata.PluginDllName, CharSet = CharSet.Auto)]
        public static extern void TellPluginRotateEyes(ushort rotateEyes);

#if UNITY_EDITOR
        public void Awake()
        {
        }
#else
        public static Settings s_Settings;

        public void Awake()
        {
            s_Settings = this;
        }
#endif
    }
}
