// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on
// Windows-classic-samples/Samples/DisplayCoreCustomCompositor/cpp/DisplayCoreCustomCompositor.cpp:
// Copyright (c) Microsoft Corporation
// SPDX-License-Identifier: MIT

#pragma once

#include "RenderParam.h"

#include <winrt/Windows.Devices.Display.Core.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

// Import things into the winrt namespace, removing extra qualifications.
namespace winrt {
using winrt::Windows::Devices::Display::DisplayMonitor;
using winrt::Windows::Devices::Display::Core::DisplayAdapter;
using winrt::Windows::Devices::Display::Core::DisplayDevice;
using winrt::Windows::Devices::Display::Core::DisplayManager;
using winrt::Windows::Devices::Display::Core::DisplayPath;
using winrt::Windows::Devices::Display::Core::DisplayTarget;
}  // namespace winrt

namespace metaview {

/**
 * A display found by enumeration.
 */
class Display {
  public:
    /**
     * Constructor.
     */
    Display(winrt::DisplayTarget target, winrt::DisplayMonitor monitor);

    /**
     * Copy constructor
     */
    Display(Display const&) = default;

    /**
     * Get the human-readable name of this display.
     */
    std::wstring_view getDisplayName() const;

    /**
     * Is this an HMD?
     */
    bool isHMD() const;

    /**
     * Get the EDID data.
     */
    std::vector<std::uint8_t> getEDID() const;

    /**
     * Is this a Meta View display?
     */
    bool isMetaViewDisplay() const;

    /**
     * Is this a direct-mode-capable Meta View display?
     */
    bool isDirectMetaViewDisplay() const;

    /**
     * Get the DisplayTarget associated with this display.
     */
    winrt::DisplayTarget getTarget() const;

    /**
     * Get the adapter LUID associated with this display, as a 64-bit integer
     */
    int64_t getAdapter() const;

  private:
    winrt::DisplayTarget target_;
    winrt::DisplayMonitor monitor_;
};

/**
 * Object for performing display enumeration and "direct-mode" display
 * configuration.
 */
class DirectDisplayManager {
  public:
    /**
     * Constructor.
     */
    DirectDisplayManager();

    /**
     * @brief Destroy the Direct Display Manager object
     */
    ~DirectDisplayManager();

    /**
     * Get a list of all connected and direct-mode-capable Meta View displays.
     *
     * Ideally there is only one.
     */
    std::vector<Display> getDirectMetaViewDisplays();

    /**
     * Get a list of all connected displays.
     *
     * This is mostly for use in UI when we can't find a Meta View device.
     */
    std::vector<Display> getAllDisplays();

    /**
     * Set up direct display on the single display we've found.
     *
     * If successful, a non-null pointer to render params will be returned. This
     * object will essentially "own" the direct display: when it is destroyed,
     * the display target is released.
     */
    std::unique_ptr<RenderParam> setUpDirectDisplay(
        winrt::DisplayTarget target);

    //! @overload
    std::unique_ptr<RenderParam> setUpDirectDisplay(Display const& display) {
        return setUpDirectDisplay(display.getTarget());
    }

    // Cannot copy or move.
    DirectDisplayManager(DirectDisplayManager const&) = delete;
    DirectDisplayManager(DirectDisplayManager&&) = delete;
    DirectDisplayManager& operator=(DirectDisplayManager const&) = delete;
    DirectDisplayManager& operator=(DirectDisplayManager&&) = delete;

  private:
    winrt::DisplayManager manager_;
};
}  // namespace metaview
