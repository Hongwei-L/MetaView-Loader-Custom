// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on
// Windows-classic-samples/Samples/DisplayCoreCustomCompositor/cpp/DisplayCoreCustomCompositor.cpp:
// Copyright (c) Microsoft Corporation
// SPDX-License-Identifier: MIT

#include "RenderParam.h"

#include <windows.devices.display.core.interop.h>
#include <winrt/Windows.Graphics.DirectX.h>

static inline LUID LuidFromAdapterId(
    winrt::Windows::Graphics::DisplayAdapterId id) {
    return {id.LowPart, id.HighPart};
}

// Import things into the winrt namespace, removing extra qualifications.
namespace winrt {
using winrt::Windows::Devices::Display::Core::DisplayAdapter;
}  // namespace winrt

namespace metaview {

RenderParam::RenderParam(winrt::DisplayManager manager_,
                         winrt::DisplayDevice device_,
                         winrt::DisplayTarget target_, winrt::DisplayPath path_)
    : manager(std::move(manager_)),
      device(std::move(device_)),
      target(std::move(target_)),
      path(std::move(path_)) {}

RenderParam::~RenderParam() { manager.ReleaseTarget(target); }

winrt::com_ptr<IDXGIAdapter4> RenderParam::getDXGIAdapter() const {
    const winrt::DisplayAdapter adapter = target.Adapter();
    winrt::com_ptr<IDXGIFactory6> factory;
    factory.capture(&CreateDXGIFactory2, 0);

    // Find the GPU that the target is connected to
    winrt::com_ptr<IDXGIAdapter4> dxgiAdapter;
    dxgiAdapter.capture(factory, &IDXGIFactory6::EnumAdapterByLuid,
                        LuidFromAdapterId(adapter.Id()));
    return dxgiAdapter;
}

std::pair<winrt::com_ptr<ID3D11Device5>, winrt::com_ptr<ID3D11DeviceContext>>
RenderParam::createBasicD3D11Device() const {
    auto dxgiAdapter = getDXGIAdapter();

    // Create the D3D device and context from the adapter
    D3D_FEATURE_LEVEL featureLevel;
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> d3dContext;
    winrt::check_hresult(D3D11CreateDevice(
        dxgiAdapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, device.put(), &featureLevel, d3dContext.put()));
    return {device.as<ID3D11Device5>(), d3dContext};
}
std::pair<Texture2DPtr, RenderTargetViewPtr> RenderParam::ConvertSurface(
    winrt::com_ptr<ID3D11Device5> const& d3dDevice,
    winrt::DisplaySurface const& surface) {
    auto deviceInterop = device.as<IDisplayDeviceInterop>();

    auto surfaceRaw = surface.as<::IInspectable>();

    // Share the DisplaySurface across devices using a handle
    winrt::handle surfaceHandle;
    winrt::check_hresult(deviceInterop->CreateSharedHandle(
        surfaceRaw.get(), nullptr, GENERIC_ALL, nullptr, surfaceHandle.put()));

    winrt::com_ptr<ID3D11Texture2D> texture;
    // Call OpenSharedResource1 on the D3D device to get the ID3D11Texture2D
    texture.capture(d3dDevice, &ID3D11Device5::OpenSharedResource1,
                    surfaceHandle.get());
    D3D11_TEXTURE2D_DESC surfaceDesc = {};
    texture->GetDesc(&surfaceDesc);

    D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MipSlice = 0;
    viewDesc.Format = surfaceDesc.Format;

    winrt::com_ptr<ID3D11RenderTargetView> rtv;
    // Create a render target view for the surface
    winrt::check_hresult(
        d3dDevice->CreateRenderTargetView(texture.get(), &viewDesc, rtv.put()));
    return std::make_pair(std::move(texture), std::move(rtv));
}

}  // namespace metaview
