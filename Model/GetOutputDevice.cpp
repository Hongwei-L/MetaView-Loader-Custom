// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED

#include "GetOutputDevice.h"

#include <Windows.devices.display.core.interop.h>
#include <dxgi.h>
#include <winrt/base.h>

#include <iostream>

namespace metaview {
uint64_t getDefaultLuid() {
#if 0
    D3D11_CREATE_DEVICE_FLAG createDeviceFlags{};
    D3D_FEATURE_LEVEL acceptable = D3D_FEATURE_LEVEL_11_1;
    D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;
    D3D_FEATURE_LEVEL found{};
    // winrt::com_ptr<ID3D11Device> device;
    // device.capture(D3D11CreateDevice());
#endif
    winrt::com_ptr<IDXGIFactory1> factory;
    factory.capture(CreateDXGIFactory1);
    winrt::com_ptr<IDXGIAdapter1> adapter;

    std::vector<winrt::com_ptr<IDXGIAdapter1>> adapters;
    UINT i = 0;
    while (factory->EnumAdapters1(i, adapter.put()) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        std::wcout << L"Adapter " << i << ": " << desc.Description
                   << L" - LUID " << Int64FromLuid(desc.AdapterLuid)
                   << std::endl;

        adapters.push_back(adapter);
        adapter = nullptr;
        ++i;
    }
    if (adapters.empty()) {
        std::cerr << "No adapters!?" << std::endl;
        return 0;
    }
    //! @todo do we need to allow non-primary GPUs?
    DXGI_ADAPTER_DESC1 desc{};
    adapters.front()->GetDesc1(&desc);
    return (uint64_t)Int64FromLuid(desc.AdapterLuid);
}
}  // namespace metaview
