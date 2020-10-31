// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on
// directxtk-samples/SimpleSampleWin32
// Copyright (c) Microsoft Corporation
// SPDX-License-Identifier: MIT
//
// Portions based on
// Windows-classic-samples/Samples/DisplayCoreCustomCompositor/cpp/DisplayCoreCustomCompositor.cpp:
// Copyright (c) Microsoft Corporation
// SPDX-License-Identifier: MIT
#pragma once

#include <winsdkver.h>
#define _WIN32_WINNT 0x0601
#include <sdkddkver.h>

// Use the C++ standard templated min/max
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <winrt/base.h>
#include <wrl/client.h>

#include "StepTimer.h"

#include <CommonStates.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <Effects.h>
#include <GeometricPrimitive.h>
#include <Model.h>
#include <PrimitiveBatch.h>
#include <SimpleMath.h>
#include <VertexTypes.h>

#include <memory>

namespace metaview {
class RenderParams;
class GameRenderer {
  public:
    GameRenderer(winrt::com_ptr<ID3D11DeviceContext> const& context,
                 winrt::com_ptr<ID3D11Device1> const& device, uint32_t height,
                 uint32_t width);

    GameRenderer(GameRenderer&&) = default;
    GameRenderer& operator=(GameRenderer&&) = default;

    GameRenderer(GameRenderer const&) = delete;
    GameRenderer& operator=(GameRenderer const&) = delete;

    void RenderFrame(
        winrt::com_ptr<ID3D11RenderTargetView> const& renderTarget) {
        Update(m_timer);
        Render(renderTarget);
    }

  private:
    void Update(DX::StepTimer const& timer);
    void Render(winrt::com_ptr<ID3D11RenderTargetView> const& renderTarget);
    void Clear(winrt::com_ptr<ID3D11RenderTargetView> const& renderTarget);
    uint32_t height_;
    uint32_t width_;
    DXGI_FORMAT depthBufferFormat_ = DXGI_FORMAT_D32_FLOAT;
    // Direct3D objects.
    winrt::com_ptr<ID3D11Device1> m_d3dDevice;
    winrt::com_ptr<ID3D11DeviceContext1> m_d3dContext;
    winrt::com_ptr<ID3DUserDefinedAnnotation> m_d3dAnnotation;

    // Direct3D rendering objects. Required for 3D.
    winrt::com_ptr<ID3D11Texture2D> m_depthStencil;
    winrt::com_ptr<ID3D11DepthStencilView> m_d3dDepthStencilView;
    winrt::com_ptr<ID3D11InputLayout> batchInputLayout_;
    D3D11_VIEWPORT m_screenViewport;

    // DirectXTK objects.
    std::unique_ptr<DirectX::CommonStates> states_;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>
        batch_;
    std::unique_ptr<DirectX::BasicEffect> batchEffect_;
    std::unique_ptr<DirectX::GeometricPrimitive> m_shape;
    // std::unique_ptr<DirectX::Model> m_model;

    DirectX::SimpleMath::Matrix m_world;
    DirectX::SimpleMath::Matrix m_view;
    DirectX::SimpleMath::Matrix m_projection;

    // Rendering loop timer.
    DX::StepTimer m_timer;
    // Performance events
    void PIXBeginEvent(_In_z_ const wchar_t* name) {
        m_d3dAnnotation->BeginEvent(name);
    }

    void PIXEndEvent() { m_d3dAnnotation->EndEvent(); }

    void PIXSetMarker(_In_z_ const wchar_t* name) {
        m_d3dAnnotation->SetMarker(name);
    }
};
}  // namespace metaview
