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
#include "researchmode\ResearchModeApi.h"
#include "ShaderStructures.h"
#include "ModelRenderer.h"

namespace BasicHologram
{
    class XAxisModel :
        public ModelRenderer
    {
    public:
        XAxisModel(std::shared_ptr<DX::DeviceResources> const& deviceResources, float length, float thick) :
            ModelRenderer(deviceResources)
        {
            m_pixelShaderFile = L"ms-appx:///SimplePixelShader.cso";
            m_length = length;
            m_thick = thick;

            m_endColor = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
            m_originColor = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);

        }
        virtual ~XAxisModel()
        {
        }

        DirectX::XMMATRIX GetModelRotation();

        void SetLenghtAndThickness(float length, float thick)
        {
            m_length = length;
            m_thick = thick;
        }

        void SetRotation(DirectX::XMFLOAT4X4 rotation)
        {
            m_rotation = rotation;
        }

        void SetColor(DirectX::XMFLOAT3 &color)
        {
            m_originColor = color;
        }

    protected:

        void GetModelVertices(std::vector<VertexPositionColor> &returnedModelVertices);
        void GetOriginCenterdCubeModelVertices(std::vector<VertexPositionColor> &returnedModelVertices);
        void GetModelXAxisVertices(std::vector<VertexPositionColor> &modelVertices);

        void GetModelTriangleIndices(std::vector<unsigned short> &triangleIndices);


        virtual void UpdateSlateTexture()
        {
        }

        void EnsureSlateTexture()
        {
        }

        float m_length;
        float m_thick;

        DirectX::XMFLOAT4X4 m_rotation;
        DirectX::XMFLOAT3 m_endColor;
        DirectX::XMFLOAT3 m_originColor;
    };
}
