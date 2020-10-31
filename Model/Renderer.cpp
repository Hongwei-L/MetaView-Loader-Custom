// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on
// Windows-classic-samples/Samples/DisplayCoreCustomCompositor/cpp/DisplayCoreCustomCompositor.cpp:
// Copyright (c) Microsoft Corporation
// SPDX-License-Identifier: MIT

#include "Renderer.h"

#include <windows.devices.display.core.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.h>

// Import things into the winrt namespace, removing extra qualifications.
namespace winrt {
using namespace winrt::Windows::Graphics::DirectX;

using winrt::Windows::Devices::Display::Core::DisplayPrimaryDescription;
using winrt::Windows::Devices::Display::Core::DisplayTask;
using winrt::Windows::Graphics::SizeInt32;

}  // namespace winrt

using std::vector;

namespace metaview {
/**
 * @brief Helper free function to make it easier to QueryInterface on something
 * that's not already held in a winrt::com_ptr.
 *
 * Just call your com_ptr's `capture` method passing this function and the
 * pointer to start with.
 */
static inline HRESULT FreeQueryInterface(::IUnknown* ptr, winrt::guid guid,
                                         void** dst) {
    return ptr->QueryInterface(guid, dst);
}

Renderer::Renderer(std::unique_ptr<RenderParam>&& params, size_t numSurfaces,
                   ID3D11Device* d3dDev)
    : params_(std::move(params)),
      numSurfaces_(numSurfaces),
      source_(params_->device.CreateScanoutSource(params_->target)),
      taskPool_(params_->device.CreateTaskPool()),
      primaries_(numSurfaces, nullptr),
      scanouts_(numSurfaces, nullptr),
      textures_(numSurfaces, nullptr),
      rtvs_(numSurfaces_, nullptr) {
    winrt::com_ptr<ID3D11DeviceContext> context;
    if (d3dDev != nullptr) {
        d3dDevice_.capture(FreeQueryInterface, d3dDev);
        auto deviceBase = d3dDevice_.as<ID3D11Device>();
        d3dDevice_->GetImmediateContext(context.put());
    } else {
        std::tie(d3dDevice_, context) = params_->createBasicD3D11Device();
    }
    context.as(d3dContext_);

    createFence();

    winrt::SizeInt32 sourceResolution =
        params_->path.SourceResolution().Value();
    width_ = static_cast<uint32_t>(sourceResolution.Width);
    height_ = static_cast<uint32_t>(sourceResolution.Height);

    winrt::Direct3D11::Direct3DMultisampleDescription multisampleDesc = {};
    multisampleDesc.Count = 1;
    // Create a surface format description for the primaries
    winrt::DisplayPrimaryDescription primaryDesc{
        width_,
        height_,
        params_->path.SourcePixelFormat(),
        winrt::DirectXColorSpace::RgbFullG22NoneP709,
        false,
        multisampleDesc};

    for (size_t surfaceIndex = 0; surfaceIndex < numSurfaces_; surfaceIndex++) {
        primaries_[surfaceIndex] =
            params_->device.CreatePrimary(params_->target, primaryDesc);
        scanouts_[surfaceIndex] = params_->device.CreateSimpleScanout(
            source_, primaries_[surfaceIndex], 0, 1);
        std::tie(textures_[surfaceIndex], rtvs_[surfaceIndex]) =
            params_->ConvertSurface(d3dDevice_, primaries_[surfaceIndex]);
        // Clear to a non-black color
        float clearColor[4] = {(surfaceIndex == 0) ? 1.f : 0.f,
                               (surfaceIndex == 1) ? 1.f : 0.f,
                               (surfaceIndex == 2) ? 1.f : 0.f, 1.f};
        d3dContext_->ClearRenderTargetView(rtvs_[surfaceIndex].get(),
                                           clearColor);
    }
}

void Renderer::createFence() {
    // Create a fence for signalling when rendering work finishes
    d3dFence_.capture(d3dDevice_, &ID3D11Device5::CreateFence, 0,
                      D3D11_FENCE_FLAG_SHARED);

    auto deviceInterop = params_->device.as<IDisplayDeviceInterop>();

    winrt::handle fenceHandle;
    // Share the ID3D11Fence across devices using a handle
    winrt::check_hresult(d3dFence_->CreateSharedHandle(
        nullptr, GENERIC_ALL, nullptr, fenceHandle.put()));

    // Call OpenSharedHandle on the DisplayDevice to get a DisplayFence
    winrt::com_ptr<::IInspectable> displayFenceInspectable;
    displayFenceInspectable.capture(deviceInterop,
                                    &IDisplayDeviceInterop::OpenSharedHandle,
                                    fenceHandle.get());

    displayFence_ = displayFenceInspectable.as<winrt::DisplayFence>();
}

Renderer::~Renderer() {
    params_->device.WaitForVBlank(source_);
    params_.reset();
}

int Renderer::waitFrame() {
    incrementModuloSize(waitedIndex_);
    params_->device.WaitForVBlank(source_);
    d3dContext_->SetMarkerInt(L"waitFrame completed", 0);
    d3dContext_->BeginEventInt(L"Render frame #d", (INT)fenceValue_);

    return waitedIndex_;
}

void Renderer::endFrame() {
    //! @todo do we care about wrapping? Will this 64 bit value ever wrap?
    ++fenceValue_;
    d3dContext_->EndEvent();

    d3dContext_->BeginEventInt(L"endFrame #d", (INT)fenceValue_);
    d3dContext_->Signal(d3dFence_.get(), fenceValue_);
    incrementModuloSize(endedIndex_);

    winrt::DisplayTask task = taskPool_.CreateTask();
    task.SetScanout(scanouts_[endedIndex_]);
    task.SetWait(displayFence_, fenceValue_);

    taskPool_.ExecuteTask(task);
    d3dContext_->EndEvent();
}

void Renderer::blankScreen() {
    auto index = waitFrame();
    float clearColor[4] = {0, 0, 0, 0};
    d3dContext_->ClearRenderTargetView(rtvs_[index].get(), clearColor);
    endFrame();
}

void Renderer::incrementModuloSize(int32_t& i) const noexcept {
    int32_t newVal = i + 1;
    if (newVal >= static_cast<uint64_t>(numSurfaces_)) {
        newVal = 0;
    }
    i = newVal;
}
}  // namespace metaview
