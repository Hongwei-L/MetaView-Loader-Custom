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

#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <winrt/Windows.Devices.Display.Core.h>
#include <winrt/base.h>

#include <atomic>
#include <type_traits>

// Import things into the winrt namespace, removing extra qualifications.
namespace winrt {
using winrt::Windows::Devices::Display::Core::DisplayDevice;
using winrt::Windows::Devices::Display::Core::DisplayManager;
using winrt::Windows::Devices::Display::Core::DisplayPath;
using winrt::Windows::Devices::Display::Core::DisplaySurface;
using winrt::Windows::Devices::Display::Core::DisplayTarget;
}  // namespace winrt

namespace metaview {

using Texture2DPtr = winrt::com_ptr<ID3D11Texture2D>;
using RenderTargetViewPtr = winrt::com_ptr<ID3D11RenderTargetView>;
/**
 * "Owns" the direct rendering that we have set up.
 */
struct RenderParam {
    /**
     * @brief Construct a new Render Param object.
     *
     * Should only be called from within DirectDisplayManager
     *
     * @param manager_ The WinRT display manager.
     * @param device_
     * @param target_ The DisplayTarget that we are directly rendering to.
     * @param path_
     */
    RenderParam(winrt::DisplayManager manager_, winrt::DisplayDevice device_,
                winrt::DisplayTarget target_, winrt::DisplayPath path_);

    /**
     * @brief Destroy the Render Param object and release the DisplayTarget.
     */
    ~RenderParam();

    // Cannot copy or move assign/construct
    RenderParam(RenderParam const&) = delete;
    RenderParam(RenderParam&&) = delete;
    RenderParam& operator=(RenderParam const&) = delete;
    RenderParam& operator=(RenderParam&&) = delete;

    /**
     * @brief Get the DXGI adapter corresponding to this display.
     *
     * @return winrt::com_ptr<IDXGIAdapter4>
     */
    winrt::com_ptr<IDXGIAdapter4> getDXGIAdapter() const;

    /**
     * @brief Create a basic D3D11 device and immediate context.
     *
     * @return std::pair<winrt::com_ptr<ID3D11Device5>,
     * winrt::com_ptr<ID3D11DeviceContext>>
     */
    std::pair<winrt::com_ptr<ID3D11Device5>,
              winrt::com_ptr<ID3D11DeviceContext>>
    createBasicD3D11Device() const;
    /**
     * @brief Take the DisplaySurface, and return the D3D11 texture and render
     * target view.
     *
     * @param[in] d3dDevice Direct3D11 device to convert to/share with.
     * @param[in] surface Surface allocated for rendering
     * @return texture, rtv pair corresponding to the supplied surface
     */
    std::pair<Texture2DPtr, RenderTargetViewPtr> ConvertSurface(
        winrt::com_ptr<ID3D11Device5> const& d3dDevice,
        winrt::DisplaySurface const& surface);

    winrt::DisplayDevice device{nullptr};
    winrt::DisplayTarget target{nullptr};
    winrt::DisplayPath path{nullptr};
    winrt::DisplayManager manager{nullptr};
};

}  // namespace metaview
