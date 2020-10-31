// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED

#pragma once

#include "ModeComparison.h"

#include <winrt/Windows.Devices.Display.Core.h>

namespace winrt {
using winrt::Windows::Devices::Display::Core::DisplayModeInfo;
using winrt::Windows::Devices::Display::Core::DisplayState;
using winrt::Windows::Devices::Display::Core::DisplayTarget;
}  // namespace winrt

namespace metaview {

/**
 * Get the best mode for the state and target, creating a path in your state and
 * setting it.
 *
 * Returns the best mode found, for informational purposes.
 */
winrt::DisplayModeInfo getBestMode(winrt::DisplayState& state,
                                   winrt::DisplayTarget const& target);
}  // namespace metaview