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

#include "GameRenderer.h"
#include "Model/RenderParam.h"

using DirectX::BasicEffect;
using DirectX::CommonStates;
using DirectX::GeometricPrimitive;
using DirectX::PrimitiveBatch;
using DirectX::VertexPositionColor;
using DirectX::XM_PI;
using DirectX::XM_PIDIV4;
using DirectX::XMMATRIX;
using DirectX::XMVECTORF32;
using DirectX::SimpleMath::Matrix;
using DirectX::SimpleMath::Vector3;
namespace Colors = DirectX::Colors;

namespace metaview {
GameRenderer::GameRenderer(winrt::com_ptr<ID3D11DeviceContext> const& context,
                           winrt::com_ptr<ID3D11Device1> const& device,
                           uint32_t height, uint32_t width)
    : height_(height),
      width_(width),
      m_d3dDevice(device),
      m_d3dContext(context.as<ID3D11DeviceContext1>()),
      states_(std::make_unique<CommonStates>(m_d3dDevice.get())),
      batch_(std::make_unique<PrimitiveBatch<VertexPositionColor>>(
          m_d3dContext.get())),
      batchEffect_(std::make_unique<BasicEffect>(m_d3dDevice.get())) {
    // Create a depth stencil view for use with 3D rendering if needed.
    CD3D11_TEXTURE2D_DESC depthStencilDesc(
        depthBufferFormat_, width, height,
        1,  // This depth stencil view has only one texture.
        1,  // Use a single mipmap level.
        D3D11_BIND_DEPTH_STENCIL);

    winrt::check_hresult(m_d3dDevice->CreateTexture2D(
        &depthStencilDesc, nullptr, m_depthStencil.put()));

    CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(
        D3D11_DSV_DIMENSION_TEXTURE2D);
    winrt::check_hresult(m_d3dDevice->CreateDepthStencilView(
        m_depthStencil.get(), &depthStencilViewDesc,
        m_d3dDepthStencilView.put()));

    // Set the 3D rendering viewport to target the entire window.
    m_screenViewport = CD3D11_VIEWPORT(0.0f, 0.0f, static_cast<float>(width),
                                       static_cast<float>(height));

    batchEffect_->SetVertexColorEnabled(true);

    {
        void const* shaderByteCode;
        size_t byteCodeLength;

        batchEffect_->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

        winrt::check_hresult(device->CreateInputLayout(
            VertexPositionColor::InputElements,
            VertexPositionColor::InputElementCount, shaderByteCode,
            byteCodeLength, batchInputLayout_.put()));
    }

    m_shape = GeometricPrimitive::CreateTeapot(context.get(), 4.f, 8);

    float aspectRatio = float(width) / float(height);
    float fovAngleY = 70.0f * XM_PI / 180.0f;

    // This sample makes use of a right-handed coordinate system using row-major
    // matrices.
    m_projection = Matrix::CreatePerspectiveFieldOfView(fovAngleY, aspectRatio,
                                                        0.01f, 100.0f);

    batchEffect_->SetProjection(m_projection);
    m_d3dAnnotation = m_d3dContext.as<ID3DUserDefinedAnnotation>();
}

void GameRenderer::Update(DX::StepTimer const& timer) {
    Vector3 eye(0.0f, 0.7f, 1.5f);
    Vector3 at(0.0f, -0.1f, 0.0f);

    m_view = Matrix::CreateLookAt(eye, at, Vector3::UnitY);

    m_world =
        Matrix::CreateRotationY(float(timer.GetTotalSeconds() * XM_PIDIV4));

    batchEffect_->SetView(m_view);
    batchEffect_->SetWorld(Matrix::Identity);
}
// Draws the scene.
void GameRenderer::Render(
    winrt::com_ptr<ID3D11RenderTargetView> const& renderTarget) {
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0) {
        return;
    }

    Clear(renderTarget);

    PIXBeginEvent(L"Render");
#if 0
    // Draw procedurally generated dynamic grid
    const XMVECTORF32 xaxis = {20.f, 0.f, 0.f};
    const XMVECTORF32 yaxis = {0.f, 0.f, 20.f};
    DrawGrid(xaxis, yaxis, g_XMZero, 20, 20, Colors::Gray);
#endif

    // Draw 3D object
    PIXBeginEvent(L"Draw teapot");
    XMMATRIX local = m_world * Matrix::CreateTranslation(-2.f, -2.f, -4.f);
    m_shape->Draw(local, m_view, m_projection,
                  Colors::White);  // , m_texture1.Get());
    PIXEndEvent();

#if 0
    PIXBeginEvent(L"Draw model");
    const XMVECTORF32 scale = {0.01f, 0.01f, 0.01f};
    const XMVECTORF32 translate = {3.f, -2.f, -4.f};
    XMVECTOR rotate =
        Quaternion::CreateFromYawPitchRoll(XM_PI / 2.f, 0.f, -XM_PI / 2.f);
    local =
        m_world * XMMatrixTransformation(g_XMZero, Quaternion::Identity, scale,
                                         g_XMZero, rotate, translate);
    m_model->Draw(context, *m_states, local, m_view, m_projection);
    PIXEndEvent();
#endif

    PIXEndEvent();

    // Show the new frame.
    // m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void GameRenderer::Clear(
    winrt::com_ptr<ID3D11RenderTargetView> const& renderTarget) {
    PIXBeginEvent(L"Clear");

    // Clear the views.

    m_d3dContext->ClearRenderTargetView(renderTarget.get(),
                                        Colors::CornflowerBlue);
    m_d3dContext->ClearDepthStencilView(m_d3dDepthStencilView.get(),
                                        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                        1.0f, 0);
    ID3D11RenderTargetView* renderTargets[] = {renderTarget.get()};
    m_d3dContext->OMSetRenderTargets(1, renderTargets,
                                     m_d3dDepthStencilView.get());

    // Set the viewport.
    m_d3dContext->RSSetViewports(1, &m_screenViewport);

    PIXEndEvent();
}
}  // namespace metaview
