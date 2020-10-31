// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on
// Windows-classic-samples/Samples/DisplayCoreCustomCompositor/cpp/DisplayCoreCustomCompositor.cpp:
// Copyright (c) Microsoft Corporation
// SPDX-License-Identifier: MIT

#include "DirectDisplayManager.h"

#include <windows.devices.display.core.interop.h>
#include <winrt/Windows.Devices.Display.Core.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.DirectX.h>

#include <algorithm>

#include "ModeSelection.h"

// Import things into the winrt namespace, removing extra qualifications.
namespace winrt {
using winrt::Windows::Devices::Display::DisplayMonitorDescriptorKind;
using winrt::Windows::Devices::Display::DisplayMonitorUsageKind;
using winrt::Windows::Devices::Display::Core::DisplayManagerOptions;
using winrt::Windows::Devices::Display::Core::DisplayModeQueryOptions;
using winrt::Windows::Devices::Display::Core::DisplayPath;
using winrt::Windows::Devices::Display::Core::DisplayPathScaling;
using winrt::Windows::Devices::Display::Core::DisplayState;
using winrt::Windows::Devices::Display::Core::DisplayStateApplyOptions;
using winrt::Windows::Foundation::Collections::IVectorView;
using winrt::Windows::Graphics::DirectX::DirectXPixelFormat;
}  // namespace winrt

using std::vector;

namespace metaview {

// EDID PNPIDs are packed strangely. Characters A-Z are assigned values 1 thru
// 26, represented by 5 bits per character. The three characters then fit into
// two bytes, with one padding bit at the beginning. (Big endian)

constexpr uint16_t edidPnpidCharToInt(char letter) {
    return static_cast<uint16_t>((letter >= 'a') ? (letter - 'a')
                                                 : (letter - 'A')) +
           1;
}

constexpr uint16_t edidPnpidTo16Bit(char a, char b, char c) {
    return (edidPnpidCharToInt(a) << 10) | (edidPnpidCharToInt(b) << 5) |
           edidPnpidCharToInt(c);
}

constexpr std::pair<uint8_t, uint8_t> edidPnpidToBytes(char a, char b, char c) {
    uint16_t val16 = edidPnpidTo16Bit(a, b, c);
    return {static_cast<uint8_t>((val16 & 0xff00) >> 8),
            static_cast<uint8_t>((val16 & 0x00ff))};
}

// Production PNPID
constexpr std::pair<uint8_t, uint8_t> pnpidCFR =
    edidPnpidToBytes('C', 'F', 'R');

// Old Meta PNPID, used by some test hardware
constexpr std::pair<uint8_t, uint8_t> pnpidMVA =
    edidPnpidToBytes('M', 'V', 'A');

Display::Display(winrt::DisplayTarget target, winrt::DisplayMonitor monitor)
    : target_(target), monitor_(monitor) {}
std::wstring_view Display::getDisplayName() const {
    return monitor_.DisplayName();
}

bool Display::isHMD() const {
    return target_.UsageKind() == winrt::DisplayMonitorUsageKind::HeadMounted;
}
vector<std::uint8_t> Display::getEDID() const {
    winrt::com_array<uint8_t> edidBuffer =
        monitor_.GetDescriptor(winrt::DisplayMonitorDescriptorKind::Edid);
    return {edidBuffer.begin(), edidBuffer.end()};
}

bool Display::isMetaViewDisplay() const {
    auto edid = getEDID();
    std::pair<uint8_t, uint8_t> pnpid = {edid[8], edid[9]};
    if (pnpid == pnpidCFR) {
        return true;
    }
    // For testing purposes
    if (pnpid == pnpidMVA) {
        return true;
    }
    return false;
}

bool Display::isDirectMetaViewDisplay() const {
    return isMetaViewDisplay() && isHMD();
}

winrt::DisplayTarget Display::getTarget() const { return target_; }

int64_t Display::getAdapter() const {
    auto id = target_.Adapter().Id();
    return (static_cast<int64_t>(id.HighPart) << 32) | (id.LowPart);
}

DirectDisplayManager::DirectDisplayManager()
    : manager_(
          winrt::DisplayManager::Create(winrt::DisplayManagerOptions::None)) {}

DirectDisplayManager::~DirectDisplayManager() { manager_.Close(); }

vector<Display> DirectDisplayManager::getAllDisplays() {
    vector<Display> ret;
    winrt::IVectorView<winrt::DisplayTarget> targets =
        manager_.GetCurrentTargets();

    for (auto&& target : targets) {
        if (!target.IsConnected()) {
            continue;
        }
        // You can look at a DisplayMonitor to inspect the EDID of the device
        winrt::DisplayMonitor monitor = target.TryGetMonitor();
        if (monitor == nullptr) {
            continue;
        }
        ret.emplace_back(target, monitor);
    }
    return ret;
}

vector<Display> DirectDisplayManager::getDirectMetaViewDisplays() {
    auto displays = getAllDisplays();
    // Remove everything that isn't a direct-mode-capable Meta View display.
    displays.erase(std::remove_if(displays.begin(), displays.end(),
                                  [](Display const& d) {
                                      return !d.isDirectMetaViewDisplay();
                                  }),
                   displays.end());
    return displays;
}

std::unique_ptr<RenderParam> DirectDisplayManager::setUpDirectDisplay(
    winrt::DisplayTarget target) {
    // The winrt method wants a container of targets
    auto myTargets = winrt::single_threaded_vector<winrt::DisplayTarget>();
    myTargets.Append(target);

    // Create a state object for setting modes on the targets
    auto stateResult = manager_.TryAcquireTargetsAndCreateEmptyState(myTargets);
    check_hresult(stateResult.ExtendedErrorCode());
    winrt::DisplayState state = stateResult.State();

    // Find our best mode.
    winrt::DisplayModeInfo bestMode = getBestMode(state, target);

    if (bestMode == nullptr) {
        // we failed
        throw std::runtime_error("Could not find suitable mode");
    }

    //! @todo TryFunctionalize here first?

    // Now that we've decided on modes to use for all of the targets, apply all
    // the modes in one-shot
    auto applyResult = state.TryApply(winrt::DisplayStateApplyOptions::None);
    check_hresult(applyResult.ExtendedErrorCode());

    // Re-read the current state to see the final state that was applied (with
    // all properties)
    stateResult = manager_.TryAcquireTargetsAndReadCurrentState(myTargets);
    check_hresult(stateResult.ExtendedErrorCode());
    state = stateResult.State();

    auto path = state.GetPathForTarget(target);
    if (path == nullptr) {
        throw std::runtime_error(
            "Failed to take display - usually fixed by a reboot.");
    }
    auto displayDevice = manager_.CreateDisplayDevice(target.Adapter());
    std::unique_ptr<RenderParam> params =
        std::make_unique<RenderParam>(manager_, displayDevice, target, path);
    return params;
}

}  // namespace metaview
