// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED

#include <winrt/Windows.Devices.Display.Core.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <iostream>

// Import things into the winrt namespace, removing extra qualifications.
namespace winrt {
using winrt::Windows::Devices::Display::Core::DisplayAdapter;
using winrt::Windows::Devices::Display::Core::DisplayManager;
using winrt::Windows::Devices::Display::Core::DisplayManagerOptions;
using winrt::Windows::Devices::Display::Core::DisplayTarget;
using winrt::Windows::Foundation::IInspectable;
using winrt::Windows::Foundation::Collections::IMapView;
using winrt::Windows::Foundation::Collections::IVectorView;
}  // namespace winrt

int main() {
    auto displayManager =
        winrt::DisplayManager::Create(winrt::DisplayManagerOptions::None);

    winrt::IVectorView<winrt::DisplayTarget> targets =
        displayManager.GetCurrentTargets();
    winrt::IVectorView<winrt::DisplayAdapter> adapters =
        displayManager.GetCurrentAdapters();
    for (winrt::DisplayAdapter adapter : adapters) {
        std::wcout << L"- " << adapter.DeviceInterfacePath().c_str() << L" - "
                   << adapter.SourceCount() << L" hardware sources"
                   << std::endl;
        winrt::IMapView<winrt::guid, winrt::IInspectable> props =
            adapter.Properties();

        for (auto pair : props) {
            std::wcout << L"  " << winrt::to_hstring(pair.Key()).c_str()
                       << std::endl;
        }

        for (winrt::DisplayTarget const& target : targets) {
            if (target.Adapter().Id() == adapter.Id()) {
                std::wcout << L"  - (Adapter relative id: "
                           << target.AdapterRelativeId() << L") "
                           << target.StableMonitorId().c_str() << std::endl;
            }
        }
    }
    return 0;
}
