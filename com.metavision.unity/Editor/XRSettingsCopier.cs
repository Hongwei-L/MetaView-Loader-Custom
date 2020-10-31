// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/com.valve.openvr/Editor/OpenVRSettingsCopier.cs
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using UnityEditor.Callbacks;
using System.IO;
using System;
using System.Linq;

#if UNITY_XR_MANAGEMENT
using UnityEngine.XR;
using UnityEngine.Experimental.XR;
using UnityEditor.XR.Management;
using UnityEngine.XR.Management;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
#endif

using Unity.XR.MetaView;

namespace Unity.XR.MetaView.Editor
{
    public class XRSettingsCopier
    {
        private const string defaultAssetPath = "Assets/XR/Settings/" + PluginMetadata.StreamingAssetsSettingsFilename;

        private static void CreatePath(string path)
        {
            string[] split = defaultAssetPath.Split('/');
            for (int splitIndex = 1; splitIndex < split.Length; splitIndex++)
            {
                string splitPath = string.Join("/", split, 0, splitIndex);
                if (AssetDatabase.IsValidFolder(splitPath) == false)
                {
                    AssetDatabase.CreateFolder(string.Join("/", split, 0, splitIndex - 1), split[splitIndex - 1]);
                    Debug.Log("Created: " + splitPath);
                }
            }
        }

        [PostProcessBuildAttribute(1)]
        public static void OnPostprocessBuild(BuildTarget target, string pathToBuiltProject)
        {
            if (target != BuildTarget.StandaloneWindows && target != BuildTarget.StandaloneWindows64)
                return;

#if UNITY_XR_MANAGEMENT
            //make sure we have the xr settings
            XRGeneralSettings generalSettings = XRGeneralSettingsPerBuildTarget.XRGeneralSettingsForBuildTarget(BuildPipeline.GetBuildTargetGroup(EditorUserBuildSettings.activeBuildTarget));
            if (generalSettings == null)
                return;

            //make sure our loader is checked
            bool hasLoader = generalSettings.Manager.loaders.Any(loader => loader is MetaViewLoader);
            if (hasLoader == false)
                return;
#endif

            Settings settings = Settings.GetSettings();

            string settingsAssetPath = AssetDatabase.GetAssetPath(settings);
            if (string.IsNullOrEmpty(settingsAssetPath))
            {
                CreatePath(defaultAssetPath);
                UnityEditor.AssetDatabase.CreateAsset(settings, defaultAssetPath);
                settingsAssetPath = AssetDatabase.GetAssetPath(settings);
            }


            FileInfo buildPath = new FileInfo(pathToBuiltProject);
            string buildName = buildPath.Name.Replace(buildPath.Extension, "");
            DirectoryInfo buildDirectory = buildPath.Directory;

            string dataDirectory = Path.Combine(buildDirectory.FullName, buildName + "_Data");
            if (Directory.Exists(dataDirectory) == false)
            {
                string vsDebugDataDirectory = Path.Combine(buildDirectory.FullName, "build/bin/" + buildName + "_Data");
                if (Directory.Exists(vsDebugDataDirectory) == false)
                {
                    Debug.LogError(PluginMetadata.DebugLogPrefix + "Could not find data directory at: " + dataDirectory + ". Also checked vs debug at: " + vsDebugDataDirectory);
                }
                else
                {
                    dataDirectory = vsDebugDataDirectory;
                }
            }

            string streamingAssets = Path.Combine(dataDirectory, "StreamingAssets");
            if (Directory.Exists(streamingAssets) == false)
                Directory.CreateDirectory(streamingAssets);

            string streamingSubdir = Path.Combine(streamingAssets, PluginMetadata.StreamingAssetsSubdir);
            if (Directory.Exists(streamingSubdir) == false)
                Directory.CreateDirectory(streamingSubdir);

            Debug.Log("settingsAssetPath: " + settingsAssetPath);

            FileInfo currentSettingsPath = new FileInfo(settingsAssetPath);
            FileInfo newSettingsPath = new FileInfo(Path.Combine(streamingSubdir, PluginMetadata.StreamingAssetsSettingsFilename));

            if (newSettingsPath.Exists)
            {
                newSettingsPath.IsReadOnly = false;
                newSettingsPath.Delete();
            }

            File.Copy(currentSettingsPath.FullName, newSettingsPath.FullName);

            Debug.Log(PluginMetadata.DebugLogPrefix + "Copied settings to build directory: " + newSettingsPath.FullName);
        }
    }
}
