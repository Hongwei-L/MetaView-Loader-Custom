// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on
// Windows-classic-samples/Samples/DisplayCoreCustomCompositor/cpp/DisplayCoreCustomCompositor.cpp:
// Copyright (c) Microsoft Corporation
// SPDX-License-Identifier: MIT

#include <winrt/base.h>

#include "Model/DirectDisplayManager.h"
#include "Model/Renderer.h"

#include <windows.devices.display.core.interop.h>
#include <winrt/Windows.Devices.Display.Core.h>
#include <winrt/Windows.Foundation.h>  // fixes "function that returns 'auto' cannot be used..." err

#include <array>
#include <iostream>

// Import things into the winrt namespace, removing extra qualifications.
namespace winrt {
using winrt::Windows::Devices::Display::Core::DisplaySurface;
}  // namespace winrt

const int SurfaceCount = 2;
using namespace metaview;

void RenderThread(std::unique_ptr<RenderParam>&& params,
                  std::reference_wrapper<const std::atomic_bool> shouldStop) {
    // It's not necessary to call init_apartment on every thread, but it needs
    // to be called at least once before using WinRT
    winrt::init_apartment();

    std::cout << "In a render thread" << std::endl;

    Renderer renderer(std::move(params), SurfaceCount);

    int frameCount = 0;

    std::vector<RenderTargetViewPtr> renderTargets =
        renderer.getSwapchainRTVs();

    while (!shouldStop.get()) {
        int surfaceIndex = renderer.waitFrame();
        // For the sample, we simply render a color pattern using a frame
        // counter. This code is not interesting.
        {
            std::cout << "Frame " << frameCount << ", swapchain image "
                      << surfaceIndex << std::endl;
            frameCount++;
            float amount = (float)abs(sin((float)frameCount / 30 * 3.141592));
            float clearColor[4] = {amount * ((frameCount / 30) % 3 == 0),
                                   amount * ((frameCount / 30) % 3 == 1),
                                   amount * ((frameCount / 30) % 3 == 2), 1};
            renderer.getImmediateContext()->ClearRenderTargetView(
                renderTargets[surfaceIndex].get(), clearColor);
        }
        renderer.endFrame();
    }
}

void startDirect(DirectDisplayManager& manager, Display const& display) {
    std::unique_ptr<RenderParam> paramsPtr;
    try {
        paramsPtr = manager.setUpDirectDisplay(display.getTarget());
    } catch (std::exception const& e) {
        std::cerr << "Got exception: " << e.what() << std::endl;
        std::abort();
    }

    winrt::Windows::Graphics::SizeInt32 res =
        paramsPtr->path.SourceResolution().Value();
    winrt::Windows::Devices::Display::Core::DisplayPresentationRate rate =
        paramsPtr->path.PresentationRate().Value();
    std::cout << res.Width << " x " << res.Height << " @"
              << ((double)rate.VerticalSyncRate.Numerator /
                  (double)rate.VerticalSyncRate.Denominator)
              << std::endl;
    std::atomic_bool shouldStop = false;

    std::thread renderThread(
        [&]() { RenderThread(std::move(paramsPtr), std::cref(shouldStop)); });

    // Render for 10 seconds
    Sleep(10000);

    std::cout << "Shutting down." << std::endl;
    // Trigger render thread to terminate
    shouldStop = true;
    // Wait for thread to complete
    renderThread.join();
}
int main() {
    winrt::init_apartment();

    DirectDisplayManager manager;
    auto displays = manager.getAllDisplays();
    std::cout << "Found a total of " << displays.size()
              << " connected displays.\n";
    auto directDisplays = manager.getDirectMetaViewDisplays();
    std::cout << "Found that " << directDisplays.size()
              << " displays are direct-mode capable.\n";
    if (directDisplays.size() != 1) {
        std::cerr << "wanted 1 display, found " << directDisplays.size()
                  << std::endl;
        return -1;
    }
    std::wcout << L"Starting direct mode display on: "
               << directDisplays.front().getDisplayName() << std::endl;

    startDirect(manager, directDisplays.front());
    std::cout << "Waiting 5 seconds before doing one more set of rendering."
              << std::endl;
    // Wait for 5 seconds
    Sleep(5000);

    return 0;
}
