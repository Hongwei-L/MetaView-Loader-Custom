// Copyright (c) 2020, Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
#pragma once

/**
 * @brief The stem of the plugin binary filename
 *
 * This must match the build system (CMake),
 * com.metavision.unity/Runtime/PluginMetadata.cs, and
 * com.metavision.unity/Runtime/UnitySubsystemsManifest.json
 */
constexpr const char PluginDllName[] = "XRSDKMetaView";

/**
 * @brief The name of the "Display Provider".
 *
 * This must match com.metavision.unity/Runtime/PluginMetadata.cs and
 * com.metavision.unity/Runtime/UnitySubsystemsManifest.json
 */
constexpr const char DisplayProviderName[] = "Meta View Display";

/**
 * @brief The name of the "Input Provider".
 *
 * This must match com.metavision.unity/Runtime/PluginMetadata.cs and
 * com.metavision.unity/Runtime/UnitySubsystemsManifest.json
 */
constexpr const char InputProviderName[] = "Meta View Headset Input";

/**
 * @brief Used to prefix console log messages
 */
#define PLUGIN_LOG_PREFIX "[Meta View] "

/**
 * @brief Used to compute input device names.
 *
 * See also the constants defined at the top of Providers/Input/Input.cpp
 */
#define VENDOR_NAME_DEFINE "Meta View"

/**
 * @brief Nominal IPD.
 *
 * Taken from the rendering rig.
 */
constexpr float NominalIpd = 0.061f;

/**
 * @brief Nominal head to eye distance (in z)
 *
 * @todo populate with a more useful value
 */
constexpr float NominalHeadToEye = 0.f;

/**
 * @brief Used to store plugin settings.
 *
 * This must match com.metavision.unity/Runtime/PluginMetadata.cs
 */
constexpr const char StreamingAssetsSubdir[] = "MetaView";

/**
 * @brief Used to store plugin settings.
 *
 * This must match com.metavision.unity/Runtime/PluginMetadata.cs and the class
 * name of the Settings class.
 */
constexpr const char StreamingAssetsSettingsFilename[] = "Settings.asset";
