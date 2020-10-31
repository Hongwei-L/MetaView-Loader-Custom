// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/com.valve.openvr/Editor/OpenVRLoaderMetadata.cs
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#if XR_MGMT_GTE_320

using System.Collections;
using System.Collections.Generic;
using Unity.XR.MetaView;
using UnityEditor;
using UnityEngine;
using UnityEngine.XR.Management;
using UnityEditor.XR.Management.Metadata;

namespace Unity.XR.MetaView.Editor
{
    class MetaViewPackage : IXRPackage
    {
        private class LoaderMetadata : IXRLoaderMetadata
        {
            public string loaderName { get; set; }
            public string loaderType { get; set; }
            public List<BuildTargetGroup> supportedBuildTargets { get; set; }
        }

        private class PackageMetadata : IXRPackageMetadata
        {
            public string packageName { get; set; }
            public string packageId { get; set; }
            public string settingsType { get; set; }
            public List<IXRLoaderMetadata> loaderMetadata { get; set; }
        }

        private static IXRPackageMetadata s_Metadata = new PackageMetadata()
        {
            packageName = PluginMetadata.VendorName + " XR Plugin",
            packageId = PluginMetadata.PackageId,
            settingsType = typeof(Settings).FullName,
            loaderMetadata = new List<IXRLoaderMetadata>() {
                new LoaderMetadata() {
                        loaderName = PluginMetadata.VendorName +  " Loader",
                        loaderType = typeof(MetaViewLoader).FullName,
                        supportedBuildTargets = new List<BuildTargetGroup>() {
                            BuildTargetGroup.Standalone
                        }
                    },
                }
        };

        public IXRPackageMetadata metadata => s_Metadata;

        public bool PopulateNewSettingsInstance(ScriptableObject obj)
        {
            Settings packageSettings = obj as Settings;
            if (packageSettings != null)
            {

                // Do something here if you need to...
            }
            return false;

        }
    }
}

#endif
