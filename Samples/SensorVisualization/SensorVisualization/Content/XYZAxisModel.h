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
    class XYZAxisModel :
        public ModelRenderer
    {
    public:
        XYZAxisModel(std::shared_ptr<DX::DeviceResources> const& deviceResources, float length, float thick) :
            ModelRenderer(deviceResources)
        {
            m_pixelShaderFile = L"ms-appx:///SimplePixelShader.cso";
            m_lengthX = length;
            m_lengthY = length;
            m_lengthZ = length;
            m_thick = thick;

            m_endColor = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
            m_xoriginColor = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
            m_yoriginColor = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
            m_zoriginColor = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);

        }
        virtual ~XYZAxisModel()
        {
        }

        DirectX::XMMATRIX GetModelRotation();

        void SetLenghtAndThickness(float length, float thick)
        {
            m_lengthX = length;
            m_lengthY = length;
            m_lengthZ = length;
            m_thick = thick;
        }

        void SetRotation(DirectX::XMFLOAT4X4 rotation)
        {
            m_rotation = rotation;
        }

        void SetColors(DirectX::XMFLOAT3 &xcolor, DirectX::XMFLOAT3 &ycolor, DirectX::XMFLOAT3 &zcolor)
        {
            m_xoriginColor = xcolor;
            m_yoriginColor = ycolor;
            m_zoriginColor = zcolor;
        }

        void SetAxisLengths(float lengthX, float lengthY, float lengthZ)
        {
            m_lengthX = lengthX;
            m_lengthY = lengthY;
            m_lengthZ = lengthZ;
        }

        virtual bool IsAxisModel()
        {
            return true;
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

        float m_lengthX;
        float m_lengthY;
        float m_lengthZ;
        float m_thick;

        DirectX::XMFLOAT4X4 m_rotation;
        DirectX::XMFLOAT3 m_endColor;
        DirectX::XMFLOAT3 m_xoriginColor;
        DirectX::XMFLOAT3 m_yoriginColor;
        DirectX::XMFLOAT3 m_zoriginColor;
    };
}
