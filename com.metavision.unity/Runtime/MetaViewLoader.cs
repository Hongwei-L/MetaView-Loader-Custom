// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/com.valve.openvr/Runtime/OpenVRLoader.cs
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#if UNITY_XR_MANAGEMENT
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.XR;
using UnityEngine.Experimental.XR;
using UnityEngine.XR.Management;
using System.IO;
using System.Runtime.CompilerServices;

#if UNITY_INPUT_SYSTEM
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.Layouts;
using UnityEngine.InputSystem.XR;
#endif

#if UNITY_EDITOR
using UnityEditor;
using UnityEditor.Build;
#endif

namespace Unity.XR.MetaView
{
#if UNITY_INPUT_SYSTEM
#if UNITY_EDITOR
    [InitializeOnLoad]
#endif
    static class InputLayoutLoader
    {
        static InputLayoutLoader()
        {
            RegisterInputLayouts();
        }

        public static void RegisterInputLayouts()
        {
            InputSystem.RegisterLayout<Unity.XR.MetaView.HMD>("HMD",
                matches: new InputDeviceMatcher()
                    .WithInterface(XRUtilities.InterfaceMatchAnyVersion)
                    .WithManufacturer(PluginMetadata.ManufacturerName)
                    .WithProduct(@"Headset")
            );

        }
    }
#endif

    public class MetaViewLoader : XRLoaderHelper
#if UNITY_EDITOR
    , IXRLoaderPreInit
#endif
    {
        private static List<XRDisplaySubsystemDescriptor> s_DisplaySubsystemDescriptors = new List<XRDisplaySubsystemDescriptor>();
        private static List<XRInputSubsystemDescriptor> s_InputSubsystemDescriptors = new List<XRInputSubsystemDescriptor>();


        public XRDisplaySubsystem displaySubsystem
        {
            get
            {
                return GetLoadedSubsystem<XRDisplaySubsystem>();
            }
        }

        public XRInputSubsystem inputSubsystem
        {
            get
            {
                return GetLoadedSubsystem<XRInputSubsystem>();
            }
        }

        public override bool Initialize()
        {
#if UNITY_INPUT_SYSTEM
            //InputLayoutLoader.RegisterInputLayouts();
#endif


            //this only works at the right time in editor. In builds we use a different method (reading the asset manually)
#if UNITY_EDITOR
            Settings settings = Settings.GetSettings();
            if (settings != null)
            {

                UserDefinedSettings userDefinedSettings;
                userDefinedSettings.stereoRenderingMode = (ushort)settings.GetStereoRenderingMode();
                userDefinedSettings.mirrorViewMode = (ushort)settings.GetMirrorViewMode();
                userDefinedSettings.rotateEyes = (ushort)(settings.RotateEyes);

                SetUserDefinedSettings(userDefinedSettings);
            }
#else
            NotifyInBuild();
#endif

            CreateSubsystem<XRDisplaySubsystemDescriptor, XRDisplaySubsystem>(s_DisplaySubsystemDescriptors, PluginMetadata.DisplayProviderName);

            InitErrors result = GetInitializationResult();

            if (result != InitErrors.None)
            {
                DestroySubsystem<XRDisplaySubsystem>();
                Debug.LogError(PluginMetadata.DebugLogPrefix + "Could not initialize. Error code: " + result.ToString());
                return false;
            }
            CreateSubsystem<XRInputSubsystemDescriptor, XRInputSubsystem>(s_InputSubsystemDescriptors, PluginMetadata.InputProviderName);

            Settings.GameViewOptions gameView = Settings.GetSettings().RenderGameView;
            // This line turns off rendering to the default game view, if desired.
            // See also Settings.SetGameView to change during runtime
            displaySubsystem.disableLegacyRenderer = (gameView == Settings.GameViewOptions.Disable);

            // OpenVREvents.Initialize();
            // TickCallbackDelegate callback = TickCallback;
            // RegisterTickCallback(callback);
            // callback(0);

            Debug.Log(PluginMetadata.DebugLogPrefix + "Runtime loader initialized");
            return displaySubsystem != null && inputSubsystem != null;
        }

        private void WatchForReload()
        {
#if UNITY_EDITOR
            UnityEditor.AssemblyReloadEvents.beforeAssemblyReload += DisableTickOnReload;
#endif
        }
        private void CleanupReloadWatcher()
        {
#if UNITY_EDITOR
            UnityEditor.AssemblyReloadEvents.beforeAssemblyReload -= DisableTickOnReload;
#endif
        }

        public override bool Start()
        {
            running = true;
            WatchForReload();

            StartSubsystem<XRDisplaySubsystem>();
            StartSubsystem<XRInputSubsystem>();

            SetupFileSystemWatchers();

            Debug.Log(PluginMetadata.DebugLogPrefix + "Start");
            return true;
        }

        private void SetupFileSystemWatchers()
        {
            SetupFileSystemWatcher();
        }

        private bool running = false;

#if UNITY_METRO || ENABLE_IL2CPP
        private FileInfo watcherFile;
        private System.Threading.Thread watcherThread;
        private void SetupFileSystemWatcher()
        {
            watcherThread = new System.Threading.Thread(new System.Threading.ThreadStart(ManualFileWatcherLoop));
            watcherThread.Start();
        }

        private void ManualFileWatcherLoop()
        {
            watcherFile = new System.IO.FileInfo(mirrorViewPath);
            long lastLength = -1;
            while (running)
            {
                if (watcherFile.Exists)
                {
                    long currentLength = watcherFile.Length;
                    if (lastLength != currentLength)
                    {
                        OnChanged(null, null);
                        lastLength = currentLength;
                    }
                }
                else
                {
                    lastLength = -1;
                }
                System.Threading.Thread.Sleep(1000);
            }
        }

        private void DestroyMirrorModeWatcher()
        {
            if (watcherThread != null)
            {
                watcherThread.Abort();
                watcherThread = null;
            }
        }

#else
        private FileInfo watcherFile;
        private System.IO.FileSystemWatcher watcher;
        private void SetupFileSystemWatcher()
        {
            try
            {
                settings = Settings.GetSettings();

                // Listen for changes in the mirror mode file
                if (watcher == null && running)
                {
                    watcherFile = new System.IO.FileInfo(mirrorViewPath);
                    watcher = new System.IO.FileSystemWatcher(watcherFile.DirectoryName, watcherFile.Name);
                    watcher.NotifyFilter = System.IO.NotifyFilters.LastWrite;
                    watcher.Created += OnChanged;
                    watcher.Changed += OnChanged;
                    watcher.EnableRaisingEvents = true;
                    if (watcherFile.Exists)
                        OnChanged(null, null);
                }
            }
            catch { }
        }

        private void DestroyMirrorModeWatcher()
        {
            if (watcher != null)
            {
                watcher.Created -= OnChanged;
                watcher.Changed -= OnChanged;
                watcher.EnableRaisingEvents = false;
                watcher.Dispose();
                watcher = null;
            }
        }
#endif

        private const string mirrorViewPath = "mirrorview.cfg";
        private Settings settings;

        private void OnChanged(object source, System.IO.FileSystemEventArgs e)
        {
            ReadMirrorModeConfig();
        }

        /// This allows end users to switch mirror view modes at runtime with a file.
        /// To use place a file called mirrorview.cfg in the same directory as the executable (or root of project).
        /// The file should be one line with the following key/value:
        /// MirrorViewMode=left
        /// Acceptable values are left, right, and none.
        private void ReadMirrorModeConfig()
        {
            try
            {
                var lines = System.IO.File.ReadAllLines(mirrorViewPath);
                foreach (var line in lines)
                {
                    var split = line.Split('=');
                    if (split.Length == 2)
                    {
                        var key = split[0];
                        if (key == "MirrorViewMode")
                        {
                            string stringMode = split[1];
                            Settings.MirrorViewModes mode = Settings.MirrorViewModes.None;
                            if (stringMode.Equals("left", System.StringComparison.CurrentCultureIgnoreCase))
                                mode = Settings.MirrorViewModes.Left;
                            else if (stringMode.Equals("right", System.StringComparison.CurrentCultureIgnoreCase))
                                mode = Settings.MirrorViewModes.Right;
                            else if (stringMode.Equals("none", System.StringComparison.CurrentCultureIgnoreCase))
                                mode = Settings.MirrorViewModes.None;
                            else
                            {
                                Debug.LogError(PluginMetadata.DebugLogPrefix + "Invalid mode specified in mirrorview.cfg. Options are: Left, Right, None.");
                            }

                            Debug.Log(PluginMetadata.DebugLogPrefix + "Mirror View Mode changed via file to: " + mode.ToString());
                            Settings.SetMirrorViewMode((ushort)mode); //bypass the local set.

                        }
                    }
                }
            }
            catch
            { }
        }

        private UnityEngine.Events.UnityEvent[] events;

        public override bool Stop()
        {
            running = false;
            CleanupTick();
            CleanupReloadWatcher();
            DestroyMirrorModeWatcher();

            StopSubsystem<XRInputSubsystem>();
            StopSubsystem<XRDisplaySubsystem>();

            Debug.Log(PluginMetadata.DebugLogPrefix + "Stop");
            return true;
        }

        public override bool Deinitialize()
        {
            CleanupTick();
            CleanupReloadWatcher();
            DestroyMirrorModeWatcher();

            DestroySubsystem<XRInputSubsystem>();
            DestroySubsystem<XRDisplaySubsystem>();

            return true;
        }

        public bool SetupForSingleCamera(float left, float right, float top, float bottom)
        {

            List<XRDisplaySubsystem> xrDisplayList = new List<XRDisplaySubsystem>();
            SubsystemManager.GetInstances(xrDisplayList);
            if (xrDisplayList.Count != 1)
            {
                return false;
            }
            XRDisplaySubsystem display = xrDisplayList[0];
            if (display.GetRenderPassCount() != 1)
            {
                return false;
            }

            display.GetRenderPass(0, out var renderPass);
            if (renderPass.GetRenderParameterCount() != 1)
            {
                return false;
            }

            SetProjectionParamsForSingleCamera(left, right, top, bottom);
            Debug.Log($"{PluginMetadata.DebugLogPrefix}Sent single-cam projection parameters to native plugin.");
            return true;
        }

        public bool SetupForSinglePassInstancedCamera(float IPD)
        {
            SetParamsForSinglePassInstancedCameraInputCPP(IPD);
            SetParamsForSinglePassInstancedCameraDisplayCPP(IPD);
            Debug.Log($"{PluginMetadata.DebugLogPrefix}Sent single pass instanced camera parameters to native plugin.");
            return true;
        }

        private static void CleanupTick()
        {
            RegisterTickCallback(null);
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
        struct UserDefinedSettings
        {
            public ushort stereoRenderingMode;
            public ushort mirrorViewMode;
            public ushort rotateEyes;
        }

        [DllImport(PluginMetadata.PluginDllName, CharSet = CharSet.Auto)]
        private static extern void SetUserDefinedSettings(UserDefinedSettings settings);

        [DllImport(PluginMetadata.PluginDllName, CharSet = CharSet.Auto)]
        private static extern void NotifyInBuild();

        [DllImport(PluginMetadata.PluginDllName, CharSet = CharSet.Auto)]
        static extern InitErrors GetInitializationResult();

        [DllImport(PluginMetadata.PluginDllName, CharSet = CharSet.Auto)]
        private static extern void SetProjectionParamsForSingleCamera(float left, float right, float top, float bottom);

        [DllImport(PluginMetadata.PluginDllName, CharSet = CharSet.Auto)]
        private static extern void SetParamsForSinglePassInstancedCameraInputCPP(float IPD);

        [DllImport(PluginMetadata.PluginDllName, CharSet = CharSet.Auto)]
        private static extern void SetParamsForSinglePassInstancedCameraDisplayCPP(float IPD);

        [DllImport(PluginMetadata.PluginDllName, CharSet = CharSet.Auto)]
        static extern void RegisterTickCallback([MarshalAs(UnmanagedType.FunctionPtr)] TickCallbackDelegate callbackPointer);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate void TickCallbackDelegate(int value);

        [AOT.MonoPInvokeCallback(typeof(TickCallbackDelegate))]
        public static void TickCallback(int value)
        {
            // OpenVREvents.Update();
        }

#if UNITY_EDITOR
        public string GetPreInitLibraryName(BuildTarget buildTarget, BuildTargetGroup buildTargetGroup)
        {
            return PluginMetadata.PluginDllName;
        }

        private static void DisableTickOnReload()
        {
            CleanupTick();
        }
#endif
    }
}
#endif
