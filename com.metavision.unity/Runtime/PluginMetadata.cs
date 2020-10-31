// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED

namespace Unity.XR.MetaView
{
    public class PluginMetadata
    {
        public const string PluginDllName = "XRSDKMetaView";
        public const string DisplayProviderName = "Meta View Display";

        public const string InputProviderName = "Meta View Headset Input";

        public const string VendorName = "Meta View";
        public const string ManufacturerName = "Meta View, Inc.";

        public const string DebugLogPrefix = "<b>[Meta View]</b> ";

        public const string StreamingAssetsSubdir = "MetaView";

        // Must match the class name of the Settings class!
        public const string StreamingAssetsSettingsFilename = "Settings.asset";

        public const string PackageId = "com.metavision.unity";

        public const string SettingsKey = "Unity.XR.MetaView.Settings";
    }
}
