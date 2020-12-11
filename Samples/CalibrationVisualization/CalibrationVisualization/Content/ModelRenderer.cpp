//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "ModelRenderer.h"
#include "Common\DirectXHelper.h"
#include <mutex>
#include <string>

using namespace BasicHologram;
using namespace DirectX;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI::Input::Spatial;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
ModelRenderer::ModelRenderer(std::shared_ptr<DX::DeviceResources> const& deviceResources) :
    m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();

    XMStoreFloat4x4(&m_modelTransform, DirectX::XMMatrixIdentity());
    XMStoreFloat4x4(&m_groupTransform, DirectX::XMMatrixIdentity());

    m_radiansY = static_cast<float>(XM_PI / 2);
    m_fGroupScale = 1.0f;
}

// This function uses a SpatialPointerPose to position the world-locked hologram
// two meters in front of the user's heading.
void ModelRenderer::PositionHologram(SpatialPointerPose const& pointerPose, const DX::StepTimer& timer)
{
    const float& deltaTime = static_cast<float>(timer.GetElapsedSeconds());

    if (pointerPose != nullptr)
    {
        // Get the gaze direction relative to the given coordinate system.
        const float3 headPosition = pointerPose.Head().Position();
        const float3 headDirection = pointerPose.Head().ForwardDirection();

        constexpr float distanceFromUser = 1.0f; // meters
        const float3 gazeAtTwoMeters = headPosition + (distanceFromUser * headDirection);

        // Use linear interpolation to smooth the position over time. This keeps the hologram 
        // comfortably stable.
        const float3 smoothedPosition = lerp(m_position, gazeAtTwoMeters, deltaTime * 4.0f);

        // This will be used as the translation component of the hologram's
        // model transform.
        SetPosition(smoothedPosition);
    }
}

// This function uses a SpatialPointerPose to position the world-locked hologram
// two meters in front of the user's heading.
void ModelRenderer::PositionHologramNoSmoothing(SpatialPointerPose const& pointerPose)
{
    if (pointerPose != nullptr)
    {
        // Get the gaze direction relative to the given coordinate system.
        const float3 headPosition = pointerPose.Head().Position();
        const float3 headDirection = pointerPose.Head().ForwardDirection();

        constexpr float distanceFromUser = 1.0f; // meters
        const float3 gazeAtTwoMeters = headPosition + (distanceFromUser * headDirection);

        // This will be used as the translation component of the hologram's
        // model transform.
        SetPosition(gazeAtTwoMeters);
    }
}

// Called once per frame. Rotates the cube, and calculates and sets the model matrix
// relative to the position transform indicated by hologramPositionTransform.
void ModelRenderer::Update(DX::StepTimer const& timer)
{
    // Seconds elapsed since previous frame.
    const float deltaTime = static_cast<float>(timer.GetElapsedSeconds());
    const float oneOverDeltaTime = 1.f / deltaTime;

    // Create a direction normal from the hologram's position to the origin of person space.
    // This is the z-axis rotation.
    XMVECTOR facingNormal = XMVector3Normalize(-XMLoadFloat3(&m_position));

    // Rotate the x-axis around the y-axis.
    // This is a 90-degree angle from the normal, in the xz-plane.
    // This is the x-axis rotation.
    XMVECTOR xAxisRotation = XMVector3Normalize(XMVectorSet(XMVectorGetZ(facingNormal), 0.f, -XMVectorGetX(facingNormal), 0.f));

    // Create a third normal to satisfy the conditions of a rotation matrix.
    // The cross product  of the other two normals is at a 90-degree angle to 
    // both normals. (Normalize the cross product to avoid floating-point math
    // errors.)
    // Note how the cross product will never be a zero-matrix because the two normals
    // are always at a 90-degree angle from one another.
    XMVECTOR yAxisRotation = XMVector3Normalize(XMVector3Cross(facingNormal, xAxisRotation));
    
    // Construct the 4x4 rotation matrix.

    // Rotate the quad to face the user.
    XMMATRIX headlockedRotationMatrix = XMMATRIX(
        xAxisRotation,
        yAxisRotation,
        facingNormal,
        XMVectorSet(0.f, 0.f, 0.f, 1.f)
        );

    XMMATRIX groupScalingMatrix = XMMatrixScaling(m_fGroupScale, m_fGroupScale, m_fGroupScale);
    XMMATRIX groupTransformMatrix = XMLoadFloat4x4(&m_groupTransform);

    XMMATRIX modelRotation = GetModelRotation();
    XMMATRIX modelTransform = XMLoadFloat4x4(&m_modelTransform);

    // Position the quad.
    const XMMATRIX headlockedTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_position));
    const XMMATRIX modelOffsetTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_offset));

    // The view and projection matrices are provided by the system; they are associated
    // with holographic cameras, and updated on a per-camera basis.
    // Here, we provide the model transform for the sample hologram. The model transform
    // matrix is transposed to prepare it for the shader.

    // headlockedRotationMatrix * headlockedTranslation transforms to a headlocked reference frame in the render coordinates system. These are then same
    // for all models.
    // (modelRotation * modelOffsetTranslation adujst the model position in its own reference frame. These are different per model.

    XMMATRIX combinedModelTransform = modelRotation * modelTransform * modelOffsetTranslation;

    XMStoreFloat4x4(&m_modelConstantBufferData.model, XMMatrixTranspose(combinedModelTransform * groupScalingMatrix * groupTransformMatrix * headlockedRotationMatrix * headlockedTranslation));

    // Loading is asynchronous. Resources must be created before they can be updated.
    if (!m_loadingComplete)
    {
        return;
    }

    // Use the D3D device context to update Direct3D device-based resources.
    const auto context = m_deviceResources->GetD3DDeviceContext();
     
    // Update the model transform buffer for the hologram.
    context->UpdateSubresource(
        m_modelConstantBuffer.Get(),
        0,
        nullptr,
        &m_modelConstantBufferData,
        0,
        0
    );
}

void ModelRenderer::SetSensorFrame(IResearchModeSensorFrame* pSensorFrame)
{

}

// Renders one frame using the vertex and pixel shaders.
// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
// a pass-through geometry shader is also used to set the render 
// target array index.
void ModelRenderer::Render()
{
    // Loading is asynchronous. Resources must be created before drawing can occur.
    if (!m_loadingComplete || !m_renderEnabled)
    {
        return;
    }

    const auto context = m_deviceResources->GetD3DDeviceContext();

    // Each vertex is one instance of the VertexPositionColor struct.
    const UINT stride = sizeof(VertexPositionColor);
    const UINT offset = 0;
    context->IASetVertexBuffers(
        0,
        1,
        m_vertexBuffer.GetAddressOf(),
        &stride,
        &offset
    );
    context->IASetIndexBuffer(
        m_indexBuffer.Get(),
        DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
        0
    );
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(m_inputLayout.Get());

    // Attach the vertex shader.
    context->VSSetShader(
        m_vertexShader.Get(),
        nullptr,
        0
    );

    // Set pixel shader resources
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        UpdateSlateTexture();

        ID3D11ShaderResourceView* shaderResourceViews[1] =
        {
            nullptr != m_texture2D ? m_texture2D->GetShaderResourceView().Get() : nullptr
        };

        context->PSSetShaderResources(
            0 /* StartSlot */,
            1 /* NumViews */,
            shaderResourceViews);
    }

    // Apply the model constant buffer to the vertex shader.
    context->VSSetConstantBuffers(
        0,
        1,
        m_modelConstantBuffer.GetAddressOf()
    );

    if (!m_usingVprtShaders)
    {
        // On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
        // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
        // a pass-through geometry shader is used to set the render target 
        // array index.
        context->GSSetShader(
            m_geometryShader.Get(),
            nullptr,
            0
        );
    }

    // Attach the pixel shader.
    context->PSSetShader(
        m_pixelShader.Get(),
        nullptr,
        0
    );

    // Draw the objects.
    context->DrawIndexedInstanced(
        m_indexCount,   // Index count per instance.
        2,              // Instance count.
        0,              // Start index location.
        0,              // Base vertex location.
        0               // Start instance location.
    );
}

std::future<void> ModelRenderer::CreateDeviceDependentResources()
{
    m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

    // On devices that do support the D3D11_FEATURE_D3D11_OPTIONS3::
    // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature
    // we can avoid using a pass-through geometry shader to set the render
    // target array index, thus avoiding any overhead that would be 
    // incurred by setting the geometry shader stage.
    std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///VprtVertexShader.cso" : L"ms-appx:///VertexShader.cso";

    // Shaders will be loaded asynchronously.

    // After the vertex shader file is loaded, create the shader and input layout.
    std::vector<byte> vertexShaderFileData = co_await DX::ReadDataAsync(vertexShaderFileName);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateVertexShader(
            vertexShaderFileData.data(),
            vertexShaderFileData.size(),
            nullptr,
            &m_vertexShader
        ));

    constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 3> vertexDesc =
        { {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        } };

    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateInputLayout(
            vertexDesc.data(),
            static_cast<UINT>(vertexDesc.size()),
            vertexShaderFileData.data(),
            static_cast<UINT>(vertexShaderFileData.size()),
            &m_inputLayout
        ));

    //std::wstring pixelShaderFile = L"ms-appx:///PixelShader.cso";
    // After the pixel shader file is loaded, create the shader and constant buffer.
    std::vector<byte> pixelShaderFileData = co_await DX::ReadDataAsync(m_pixelShaderFile.data());
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreatePixelShader(
            pixelShaderFileData.data(),
            pixelShaderFileData.size(),
            nullptr,
            &m_pixelShader
        ));

    const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &constantBufferDesc,
            nullptr,
            &m_modelConstantBuffer
        ));


    if (!m_usingVprtShaders)
    {
        // Load the pass-through geometry shader.
        std::vector<byte> geometryShaderFileData = co_await DX::ReadDataAsync(L"ms-appx:///GeometryShader.cso");

        // After the pass-through geometry shader file is loaded, create the shader.
        winrt::check_hresult(
            m_deviceResources->GetD3DDevice()->CreateGeometryShader(
                geometryShaderFileData.data(),
                geometryShaderFileData.size(),
                nullptr,
                &m_geometryShader
            ));
    }

    std::vector<VertexPositionColor> modelVertices;
    GetModelVertices(modelVertices);

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = modelVertices.data();
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionColor) * static_cast<UINT>(modelVertices.size()), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &vertexBufferDesc,
            &vertexBufferData,
            &m_vertexBuffer
        ));


    std::vector<unsigned short> modelIndices;
    GetModelTriangleIndices(modelIndices);

    m_indexCount = static_cast<unsigned int>(modelIndices.size());

    D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
    indexBufferData.pSysMem = modelIndices.data();
    indexBufferData.SysMemPitch = 0;
    indexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned short) * static_cast<UINT>(modelIndices.size()), D3D11_BIND_INDEX_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &indexBufferDesc,
            &indexBufferData,
            &m_indexBuffer
        ));

    // Once the cube is loaded, the object is ready to be rendered.
    m_loadingComplete = true;
}

void ModelRenderer::ReleaseDeviceDependentResources()
{
    m_loadingComplete = false;
    m_usingVprtShaders = false;
    m_vertexShader.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_geometryShader.Reset();
    m_modelConstantBuffer.Reset();
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
}
