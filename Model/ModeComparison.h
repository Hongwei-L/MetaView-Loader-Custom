// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED

#pragma once

#include <winrt/Windows.Devices.Display.Core.h>

namespace winrt {
using winrt::Windows::Devices::Display::Core::DisplayModeInfo;
}  // namespace winrt

namespace metaview {

/**
 * Mode information struct.
 *
 * Exists solely to neatly extract the mode selection logic.
 */
struct ModeInfo {
    ModeInfo() : modeInfo(nullptr) {}
    ModeInfo(winrt::DisplayModeInfo info) : modeInfo(info) {}

    winrt::DisplayModeInfo modeInfo;

    //! Refresh rate in Hz.
    double getRate() const;

    /**
     * Comparison for mode info.
     *
     * This is easier to understand than operator<, and we only really want the
     * "best" mode.
     */
    bool betterThan(ModeInfo const& other) const noexcept;
};
}  // namespace metaview