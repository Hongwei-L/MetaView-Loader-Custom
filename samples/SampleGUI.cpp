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
#include "resource.h"
#include "targetver.h"

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

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                      // current instance
WCHAR szTitle[MAX_LOADSTRING];        // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];  // the main window class name

std::atomic_bool shouldStop = false;

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    winrt::init_apartment();

    DirectDisplayManager manager;
    auto directDisplays = manager.getDirectMetaViewDisplays();
    if (directDisplays.size() != 1) {
        std::cerr << "wanted 1 display, found " << directDisplays.size()
                  << std::endl;
        return -1;
    }
    auto display = directDisplays.front();
    std::unique_ptr<RenderParam> paramsPtr;
    try {
        paramsPtr = manager.setUpDirectDisplay(display.getTarget());
    } catch (std::exception const& e) {
        std::cerr << "Got exception: " << e.what() << std::endl;
        std::abort();
    }

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SAMPLE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow)) {
        return FALSE;
    }

    HACCEL hAccelTable =
        LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SAMPLEGUI));

    std::thread renderThread(
        [&]() { RenderThread(std::move(paramsPtr), std::cref(shouldStop)); });

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    // Wait for thread to complete
    renderThread.join();

    return (int)msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SAMPLEGUI));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SAMPLEGUI);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;  // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr,
                              nullptr, hInstance, nullptr);

    if (!hWnd) {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
    switch (message) {
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId) {
                case IDM_ABOUT:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd,
                              About);
                    break;
                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
        } break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        } break;
        case WM_DESTROY:
            shouldStop = true;
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}
