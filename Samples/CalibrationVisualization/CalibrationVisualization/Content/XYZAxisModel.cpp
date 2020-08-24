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
#include "XYZAxisModel.h"

using namespace BasicHologram;
using namespace DirectX;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI::Input::Spatial;

DirectX::XMMATRIX XYZAxisModel::GetModelRotation()
{
    return DirectX::XMMatrixIdentity();
    //return DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 0.f, 1.f, 0.f), -DirectX::XM_PIDIV2);
}

void XYZAxisModel::GetModelVertices(std::vector<VertexPositionColor> &modelVertices)
{
    GetModelXAxisVertices(modelVertices);
}

void XYZAxisModel::GetModelXAxisVertices(std::vector<VertexPositionColor> &modelVertices)
{
    modelVertices.push_back({ XMFLOAT3(0.0f,     -(m_thick/2), -(m_thick/2)), m_xoriginColor, XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(0.0f,     -(m_thick/2),  (m_thick/2)), m_xoriginColor, XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(0.0f,      (m_thick/2), -(m_thick/2)), m_xoriginColor, XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(0.0f,      (m_thick/2),  (m_thick/2)), m_xoriginColor, XMFLOAT2(1.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(m_length, -(m_thick/2), -(m_thick/2)), m_xoriginColor, XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(m_length, -(m_thick/2),  (m_thick/2)), m_xoriginColor, XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(m_length,  (m_thick/2), -(m_thick/2)), m_xoriginColor, XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(m_length,  (m_thick/2),  (m_thick/2)), m_xoriginColor, XMFLOAT2(1.0f, 1.0f)});

    modelVertices.push_back({ XMFLOAT3(-(m_thick/2), 0.0f, -(m_thick/2)), m_yoriginColor, XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(-(m_thick/2), 0.0f,  (m_thick/2)), m_yoriginColor, XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(-(m_thick/2), m_length, -(m_thick/2)), m_yoriginColor, XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(-(m_thick/2), m_length,  (m_thick/2)), m_yoriginColor, XMFLOAT2(1.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3((m_thick/2), 0.0f, -(m_thick/2)), m_yoriginColor, XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3((m_thick/2), 0.0f,  (m_thick/2)), m_yoriginColor, XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3((m_thick/2), m_length, -(m_thick/2)), m_yoriginColor, XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3((m_thick/2), m_length,  (m_thick/2)), m_yoriginColor, XMFLOAT2(1.0f, 1.0f)});

    modelVertices.push_back({ XMFLOAT3(-(m_thick/2), -(m_thick/2), 0.0f), m_zoriginColor, XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(-(m_thick/2), -(m_thick/2), m_length), m_zoriginColor, XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(-(m_thick/2),  (m_thick/2), 0.0f), m_zoriginColor, XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(-(m_thick/2),  (m_thick/2), m_length), m_zoriginColor, XMFLOAT2(1.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3((m_thick/2), -(m_thick/2), 0.0f), m_zoriginColor, XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3((m_thick/2), -(m_thick/2), m_length), m_zoriginColor, XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3((m_thick/2),  (m_thick/2), 0.0f), m_zoriginColor, XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3((m_thick/2),  (m_thick/2), m_length), m_zoriginColor, XMFLOAT2(1.0f, 1.0f)});
}

void XYZAxisModel::GetOriginCenterdCubeModelVertices(std::vector<VertexPositionColor> &modelVertices)
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

void XYZAxisModel::GetModelTriangleIndices(std::vector<unsigned short> &triangleIndices)
{
    ///////// X axis
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

    /////////// Y axis
    /*
    2,1,0, // -x
    2,3,1,
    */
    triangleIndices.push_back(2+8);
    triangleIndices.push_back(1+8);
    triangleIndices.push_back(0+8);

    triangleIndices.push_back(2+8);
    triangleIndices.push_back(3+8);
    triangleIndices.push_back(1+8);

    /*
    6,4,5, // +x
    6,5,7,
    */
    triangleIndices.push_back(6+8);
    triangleIndices.push_back(4+8);
    triangleIndices.push_back(5+8);

    triangleIndices.push_back(6+8);
    triangleIndices.push_back(5+8);
    triangleIndices.push_back(7+8);

    /*
    0,1,5, // -y
    0,5,4,
    */
    triangleIndices.push_back(0+8);
    triangleIndices.push_back(1+8);
    triangleIndices.push_back(5+8);

    triangleIndices.push_back(0+8);
    triangleIndices.push_back(5+8);
    triangleIndices.push_back(4+8);

    /*
    2,6,7, // +y
    2,7,3,    
    */
    triangleIndices.push_back(2+8);
    triangleIndices.push_back(6+8);
    triangleIndices.push_back(7+8);

    triangleIndices.push_back(2+8);
    triangleIndices.push_back(7+8);
    triangleIndices.push_back(3+8);

    /*
    0,4,6, // -z
    0,6,2,
    */

    triangleIndices.push_back(0+8);
    triangleIndices.push_back(4+8);
    triangleIndices.push_back(6+8);

    triangleIndices.push_back(0+8);
    triangleIndices.push_back(6+8);
    triangleIndices.push_back(2+8);

    /*
    1,3,7, // +z
    1,7,5,    
    */
    triangleIndices.push_back(1+8);
    triangleIndices.push_back(3+8);
    triangleIndices.push_back(7+8);

    triangleIndices.push_back(1+8);
    triangleIndices.push_back(7+8);
    triangleIndices.push_back(5+8);

    /////////// Z axis
    /*
    2,1,0, // -x
    2,3,1,
    */
    triangleIndices.push_back(2+16);
    triangleIndices.push_back(1+16);
    triangleIndices.push_back(0+16);

    triangleIndices.push_back(2+16);
    triangleIndices.push_back(3+16);
    triangleIndices.push_back(1+16);

    /*
    6,4,5, // +x
    6,5,7,
    */
    triangleIndices.push_back(6+16);
    triangleIndices.push_back(4+16);
    triangleIndices.push_back(5+16);

    triangleIndices.push_back(6+16);
    triangleIndices.push_back(5+16);
    triangleIndices.push_back(7+16);

    /*
    0,1,5, // -y
    0,5,4,
    */
    triangleIndices.push_back(0+16);
    triangleIndices.push_back(1+16);
    triangleIndices.push_back(5+16);

    triangleIndices.push_back(0+16);
    triangleIndices.push_back(5+16);
    triangleIndices.push_back(4+16);

    /*
    2,6,7, // +y
    2,7,3,    
    */
    triangleIndices.push_back(2+16);
    triangleIndices.push_back(6+16);
    triangleIndices.push_back(7+16);

    triangleIndices.push_back(2+16);
    triangleIndices.push_back(7+16);
    triangleIndices.push_back(3+16);

    /*
    0,4,6, // -z
    0,6,2,
    */

    triangleIndices.push_back(0+16);
    triangleIndices.push_back(4+16);
    triangleIndices.push_back(6+16);

    triangleIndices.push_back(0+16);
    triangleIndices.push_back(6+16);
    triangleIndices.push_back(2+16);

    /*
    1,3,7, // +z
    1,7,5,    
    */
    triangleIndices.push_back(1+16);
    triangleIndices.push_back(3+16);
    triangleIndices.push_back(7+16);

    triangleIndices.push_back(1+16);
    triangleIndices.push_back(7+16);
    triangleIndices.push_back(5+16);

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



