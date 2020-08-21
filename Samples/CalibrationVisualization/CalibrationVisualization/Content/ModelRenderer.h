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

#pragma once

#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"
#include "ShaderStructures.h"
#include "researchmode\ResearchModeApi.h"
#include <Texture2D.h>
#include <string>

namespace BasicHologram
{
    // This sample renderer instantiates a basic rendering pipeline.
    class ModelRenderer
    {
    public:
        ModelRenderer(std::shared_ptr<DX::DeviceResources> const& deviceResources);
        virtual ~ModelRenderer()
        {
        }
        std::future<void> CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        void Update(DX::StepTimer const& timer);
        void Render();

        // Repositions the sample hologram.
        void PositionHologram(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose, const DX::StepTimer& timer);
        void PositionHologramNoSmoothing(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose);

        // Property accessors.
        void SetPosition(winrt::Windows::Foundation::Numerics::float3 const& pos)   { m_position = pos;  }
        winrt::Windows::Foundation::Numerics::float3 const& GetPosition()           { return m_position; }

        void SetOffset(winrt::Windows::Foundation::Numerics::float3 const& pos) { m_offset = pos; }

        void SetGroupScaleFactor(float scale)
        {
            m_fGroupScale = scale;
        }

        void SetGroupTransform(DirectX::XMMATRIX &groupTransform)
        {
            XMStoreFloat4x4(&m_groupTransform, groupTransform);
        }

		void SetSensorFrame(IResearchModeSensorFrame* pSensorFrame);

        virtual void UpdateSlateTexture() = 0;

        virtual DirectX::XMMATRIX GetModelRotation()
        {
            return DirectX::XMMatrixIdentity();
        }

        void SetModelTransform(DirectX::XMMATRIX &modelTransform)
        {
            XMStoreFloat4x4(&m_modelTransform, modelTransform);
        }

        void SetModelTransform(DirectX::XMFLOAT4X4 &modelTransform)
        {
            m_modelTransform = modelTransform;
        }

        void GetModelTransform(DirectX::XMFLOAT4X4 &modelTransform)
        {
            modelTransform = m_modelTransform;
        }

        void EnableRendering()
        {
            m_renderEnabled = true;
        }

        void DisableRendering()
        {
            m_renderEnabled = false;
        }

        virtual bool IsAxisModel()
        {
            return false;
        }

    protected:

        virtual void GetModelVertices(std::vector<VertexPositionColor> &modelVertices) = 0;
        virtual void GetModelTriangleIndices(std::vector<unsigned short> &triangleIndices) = 0;

        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources>            m_deviceResources;

        std::wstring m_pixelShaderFile;

        // Direct3D resources for cube geometry.
        Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_indexBuffer;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11GeometryShader>    m_geometryShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pixelShader;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_modelConstantBuffer;

        // System resources for cube geometry.
        ModelConstantBuffer                             m_modelConstantBufferData;
        uint32_t                                        m_indexCount = 0;

        // Variables used with the rendering loop.
        bool                                            m_renderEnabled = true;
        bool                                            m_loadingComplete = false;
        float                                           m_degreesPerSecond = 0.0f;
        winrt::Windows::Foundation::Numerics::float3    m_position = { 0.f, 0.f, -2.f };
        winrt::Windows::Foundation::Numerics::float3    m_offset = { 0.0f, 0.0f, 0.0f };
        float                                           m_radiansY;
        DirectX::XMFLOAT4X4                             m_modelTransform;
        DirectX::XMFLOAT4X4                             m_groupTransform;
        float                                           m_fGroupScale;

        // If the current D3D Device supports VPRT, we can avoid using a geometry
        // shader just to set the render target array index.
        bool                                            m_usingVprtShaders = false;

        std::shared_ptr<Texture2D> m_texture2D;
        std::mutex m_mutex;
    };
}
