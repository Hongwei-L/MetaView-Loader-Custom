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

#include <d3d11_4.h>
#include <winrt/Windows.Devices.Display.Core.h>

#include <vector>

// Import things into the winrt namespace, removing extra qualifications.
namespace winrt {
using winrt::Windows::Devices::Display::Core::DisplayFence;
using winrt::Windows::Devices::Display::Core::DisplayScanout;
using winrt::Windows::Devices::Display::Core::DisplaySource;
using winrt::Windows::Devices::Display::Core::DisplaySurface;
using winrt::Windows::Devices::Display::Core::DisplayTaskPool;
}  // namespace winrt

namespace metaview {
class Renderer {
  public:
    /**
     * @brief Construct a new Renderer object
     *
     * @param params The render params from
     * DirectDisplayManager::setupDirectDisplay()
     * @param numSurfaces How many surfaces you want to switch between. 2 is a
     * common number.
     * @param d3dDev Your D3D11Device. If not supplied, a very basic one will be
     * created with RenderParams.
     */
    explicit Renderer(std::unique_ptr<RenderParam>&& params,
                      size_t numSurfaces = 2, ID3D11Device* d3dDev = nullptr);

    /**
     * @brief Destroy the Renderer object and release the direct display
     * ownership.
     */
    ~Renderer();

    /**
     * @brief Get the textures corresponding to each swapchain image
     *
     * @return std::vector<winrt::com_ptr<ID3D11Texture2D>> const&
     */
    std::vector<winrt::com_ptr<ID3D11Texture2D>> const& getSwapchainImages()
        const noexcept {
        return textures_;
    }

    /**
     * @brief Get the RenderTargetViews corresponding to each swapchain image
     *
     * @return std::vector<winrt::com_ptr<ID3D11RenderTargetView>> const&
     */
    std::vector<winrt::com_ptr<ID3D11RenderTargetView>> const&
    getSwapchainRTVs() const noexcept {
        return rtvs_;
    }

    /**
     * @brief Get display width in pixels
     *
     * @return uint32_t
     */
    uint32_t getWidth() const noexcept { return width_; }

    /**
     * @brief Get display height in pixels
     *
     * @return uint32_t
     */
    uint32_t getHeight() const noexcept { return height_; }

    /**
     * @brief Get the immediate device context referenced by the renderer.
     *
     * @return winrt::com_ptr<ID3D11DeviceContext4> const&
     */
    winrt::com_ptr<ID3D11DeviceContext4> const& getImmediateContext()
        const noexcept {
        return d3dContext_;
    }
    /**
     * @brief Get the device referenced by the renderer.
     *
     * @return winrt::com_ptr<ID3D11DeviceContext> const&
     */
    winrt::com_ptr<ID3D11Device5> const& getDevice() const noexcept {
        return d3dDevice_;
    }

    /**
     * @brief Call before rendering, to block.
     *
     * @return the swapchain image index to render to.
     */
    int waitFrame();

    /**
     * @brief Call when you are done rendering.
     */
    void endFrame();

    /**
     * @brief Render a solid black screen.
     *
     * Performs a full waitFrame(), endFrame() sequence internally.
     */
    void blankScreen();

  private:
    /**
     * @brief Increment a value modulo the number of surfaces.
     *
     * @param[in,out] i The value to increment.
     */
    void incrementModuloSize(int32_t& i) const noexcept;

    /**
     * @brief Create the fence objects at construction time.
     */
    void createFence();

    std::unique_ptr<RenderParam> params_;
    size_t numSurfaces_;
    //! to know where to render
    winrt::DisplaySource source_;
    //! for scheduling presents
    winrt::DisplayTaskPool taskPool_;
    //! surface index
    int32_t waitedIndex_ = -1;
    //! surface index
    int32_t endedIndex_ = -1;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    std::vector<winrt::DisplaySurface> primaries_;
    std::vector<winrt::DisplayScanout> scanouts_;
    std::vector<winrt::com_ptr<ID3D11Texture2D>> textures_;
    std::vector<winrt::com_ptr<ID3D11RenderTargetView>> rtvs_;

    winrt::com_ptr<ID3D11Device5> d3dDevice_;
    winrt::com_ptr<ID3D11DeviceContext4> d3dContext_;
    winrt::com_ptr<ID3D11Fence> d3dFence_;

    winrt::DisplayFence displayFence_{nullptr};

    uint64_t fenceValue_{0};
};
}  // namespace metaview
