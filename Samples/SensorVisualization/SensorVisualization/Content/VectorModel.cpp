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
#include "VectorModel.h"

using namespace BasicHologram;
using namespace DirectX;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI::Input::Spatial;

VectorModel::VectorModel(std::shared_ptr<DX::DeviceResources> const& deviceResources, float length, float thick, DirectX::XMFLOAT3 vectorFromOrigin) :
    ModelRenderer(deviceResources)
{
    m_pixelShaderFile = L"ms-appx:///SimplePixelShader.cso";
    m_length = length;
    m_thick = thick;

    m_endColor = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_originColor = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);

    SetDirection(vectorFromOrigin);
}

void VectorModel::SetDirection(DirectX::XMFLOAT3 &vectorFromOrigin)
{
    m_vectorFromOrigin = vectorFromOrigin;
    XMFLOAT3 xfunitVector;
    xfunitVector.x = 1.0f;
    xfunitVector.y = 0.0f;
    xfunitVector.z = 0.0f;

    XMVECTOR normalizedVector = XMVector3Normalize(XMLoadFloat3(&vectorFromOrigin));
    XMVECTOR xunitVector = XMLoadFloat3(&xfunitVector);

    XMVECTOR dotProdVector = XMVector3Dot(xunitVector, normalizedVector);
    XMFLOAT3 fdotProduct;
    XMStoreFloat3(&fdotProduct, dotProdVector);
    float angle = XMScalarACos(fdotProduct.x);

    XMVECTOR crossProdVector = XMVector3Cross(xunitVector, normalizedVector);

    XMMATRIX rotMatrix = XMMatrixRotationAxis(crossProdVector, angle);
    XMStoreFloat4x4(&m_rotation, rotMatrix);
}

DirectX::XMMATRIX VectorModel::GetModelRotation()
{
    return XMLoadFloat4x4(&m_rotation);
    //return DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 0.f, 1.f, 0.f), -DirectX::XM_PIDIV2);
}

void VectorModel::GetModelVertices(std::vector<VertexPositionColor> &modelVertices)
{
    GetModelXAxisVertices(modelVertices);
}

void VectorModel::GetModelXAxisVertices(std::vector<VertexPositionColor> &modelVertices)
{

    modelVertices.push_back({ XMFLOAT3(0.0f,     -(m_thick/2), -(m_thick/2)), m_originColor, XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(0.0f,     -(m_thick/2),  (m_thick/2)), m_originColor, XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(0.0f,      (m_thick/2), -(m_thick/2)), m_originColor, XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(0.0f,      (m_thick/2),  (m_thick/2)), m_originColor, XMFLOAT2(1.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(m_length, -(m_thick/2), -(m_thick/2)), m_originColor, XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(m_length, -(m_thick/2),  (m_thick/2)), m_originColor, XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(m_length,  (m_thick/2), -(m_thick/2)), m_originColor, XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(m_length,  (m_thick/2),  (m_thick/2)), m_originColor, XMFLOAT2(1.0f, 1.0f)});
}

void VectorModel::GetOriginCenterdCubeModelVertices(std::vector<VertexPositionColor> &modelVertices)
{

    modelVertices.push_back({ XMFLOAT3(-0.2f, -0.2f, -0.2f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(-0.2f, -0.2f,  0.2f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(-0.2f,  0.2f, -0.2f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(-0.2f,  0.2f,  0.2f), XMFLOAT3(0.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(0.2f, -0.2f, -0.2f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(0.2f, -0.2f,  0.2f), XMFLOAT3(1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(0.2f,  0.2f, -0.2f), XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(0.2f,  0.2f,  0.2f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f)});
}

void VectorModel::GetModelTriangleIndices(std::vector<unsigned short> &triangleIndices)
{
    /*
    2,1,0, // -x
    2,3,1,
    */
    triangleIndices.push_back(2);
    triangleIndices.push_back(1);
    triangleIndices.push_back(0);

    triangleIndices.push_back(2);
    triangleIndices.push_back(3);
    triangleIndices.push_back(1);

    /*
    6,4,5, // +x
    6,5,7,
    */
    triangleIndices.push_back(6);
    triangleIndices.push_back(4);
    triangleIndices.push_back(5);

    triangleIndices.push_back(6);
    triangleIndices.push_back(5);
    triangleIndices.push_back(7);

    /*
    0,1,5, // -y
    0,5,4,
    */
    triangleIndices.push_back(0);
    triangleIndices.push_back(1);
    triangleIndices.push_back(5);

    triangleIndices.push_back(0);
    triangleIndices.push_back(5);
    triangleIndices.push_back(4);

    /*
    2,6,7, // +y
    2,7,3,    
    */
    triangleIndices.push_back(2);
    triangleIndices.push_back(6);
    triangleIndices.push_back(7);

    triangleIndices.push_back(2);
    triangleIndices.push_back(7);
    triangleIndices.push_back(3);

    /*
    0,4,6, // -z
    0,6,2,
    */

    triangleIndices.push_back(0);
    triangleIndices.push_back(4);
    triangleIndices.push_back(6);

    triangleIndices.push_back(0);
    triangleIndices.push_back(6);
    triangleIndices.push_back(2);

    /*
    1,3,7, // +z
    1,7,5,    
    */
    triangleIndices.push_back(1);
    triangleIndices.push_back(3);
    triangleIndices.push_back(7);

    triangleIndices.push_back(1);
    triangleIndices.push_back(7);
    triangleIndices.push_back(5);

    /*
        { {
            2,1,0, // -x
            2,3,1,

            6,4,5, // +x
            6,5,7,

            0,1,5, // -y
            0,5,4,

            2,6,7, // +y
            2,7,3,

            0,4,6, // -z
            0,6,2,

            1,3,7, // +z
            1,7,5,
        } };
        */

}





