// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on
// Windows-classic-samples/Samples/DisplayCoreCustomCompositor/cpp/DisplayCoreCustomCompositor.cpp:
// Copyright (c) Microsoft Corporation
// SPDX-License-Identifier: MIT

#include "ModeSelection.h"

#include <winrt/Windows.Devices.Display.Core.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "ModeComparison.h"

namespace winrt {
using winrt::Windows::Devices::Display::Core::DisplayModeQueryOptions;
using winrt::Windows::Devices::Display::Core::DisplayPath;
using winrt::Windows::Devices::Display::Core::DisplayPathScaling;
using winrt::Windows::Foundation::Collections::IVectorView;
using winrt::Windows::Graphics::DirectX::DirectXPixelFormat;
}  // namespace winrt

namespace metaview {

winrt::DisplayModeInfo getBestMode(winrt::DisplayState& state,
                                   winrt::DisplayTarget const& target) {
    winrt::DisplayPath path = state.ConnectTarget(target);

    // Set some values that we know we want
    path.IsInterlaced(false);
    path.Scaling(winrt::DisplayPathScaling::Identity);

    path.SourcePixelFormat(winrt::DirectXPixelFormat::R8G8B8A8UIntNormalized);

    // Get a list of modes for only the preferred resolution
    winrt::IVectorView<winrt::DisplayModeInfo> modes =
        path.FindModes(winrt::DisplayModeQueryOptions::OnlyPreferredResolution);

    ModeInfo bestMode;
    for (auto&& mode : modes) {
        ModeInfo wrappedModeInfo{mode};
        if (wrappedModeInfo.betterThan(bestMode)) {
            bestMode = wrappedModeInfo;
        }
    }
    if (bestMode.modeInfo == nullptr) {
        return {nullptr};
    }

    // Set the properties on the path
    path.ApplyPropertiesFromMode(bestMode.modeInfo);
    return bestMode.modeInfo;
}

}  // namespace metaview
