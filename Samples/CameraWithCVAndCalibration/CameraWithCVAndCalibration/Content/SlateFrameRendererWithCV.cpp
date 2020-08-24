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
#include "SlateFrameRendererWithCV.h"

#include <opencv2/core.hpp>
#include "OpenCVFrameProcessing.h"

using namespace BasicHologram;
using namespace DirectX;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI::Input::Spatial;

void SlateFrameRendererWithCV::UpdateSlateTextureWithBitmap(const BYTE *pImage, UINT uWidth, UINT uHeight)
{
    // update m_texture2D
    if (uWidth != m_Width || uHeight != m_Height)
    {
        m_Width = uWidth;
        m_Height = uHeight;
        m_texture2D = nullptr;
    }

    EnsureSlateTexture();

    void* mappedTexture =
        m_texture2D->MapCPUTexture<void>(
            D3D11_MAP_WRITE /* mapType */);

    for (UINT i = 0; i < uWidth * uHeight; i++)
    {
        UINT32 pixel = 0;
        BYTE inputPixel = pImage[i];

        pixel = inputPixel | (inputPixel << 8) | (inputPixel << 16);

        *((UINT32*)(mappedTexture)+(i)) = pixel;
    }

    m_texture2D->UnmapCPUTexture();
    m_texture2D->CopyCPU2GPU();
}

DirectX::XMMATRIX SlateFrameRendererWithCV::GetModelRotation()
{
    XMMATRIX rotation = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f), -DirectX::XM_PIDIV2);
    return rotation * DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(1.f, 0.f, 0.f, 0.f), DirectX::XM_PI);
}

void SlateFrameRendererWithCV::GetModelVertices(std::vector<VertexPositionColor> &modelVertices)
{

    modelVertices.push_back({ XMFLOAT3(-0.01f, -0.2f, -0.2f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(-0.01f, -0.2f,  0.2f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(-0.01f,  0.2f, -0.2f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(-0.01f,  0.2f,  0.2f), XMFLOAT3(0.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(0.01f, -0.2f, -0.2f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(0.01f, -0.2f,  0.2f), XMFLOAT3(1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f)});
    modelVertices.push_back({ XMFLOAT3(0.01f,  0.2f, -0.2f), XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f)});
    modelVertices.push_back({ XMFLOAT3(0.01f,  0.2f,  0.2f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f)});
}

void SlateFrameRendererWithCV::GetModelTriangleIndices(std::vector<unsigned short> &triangleIndices)
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

void SlateFrameRendererWithCV::UpdateSlateTexture()
{
    HRESULT hr = S_OK;

    ResearchModeSensorResolution resolution;
    IResearchModeSensorVLCFrame *pVLCFrame = nullptr;
    size_t outBufferCount = 0;
    const BYTE *pImage = nullptr;

    if (m_pSensorFrame == nullptr)
    {
        return;
    }

    m_pSensorFrame->GetResolution(&resolution);

    hr = m_pSensorFrame->QueryInterface(IID_PPV_ARGS(&pVLCFrame));

    if (SUCCEEDED(hr))
    {
        pVLCFrame->GetBuffer(&pImage, &outBufferCount);
    }

    if (m_cvResultMat.data == nullptr)
    {
        UpdateSlateTextureWithBitmap(pImage, resolution.Width, resolution.Height);
    }
    else
    {
        UpdateSlateTextureWithBitmap(m_cvResultMat.data, resolution.Width, resolution.Height);
    }

    if (pVLCFrame)
    {
        pVLCFrame->Release();
    }

    m_pSensorFrame->Release();
    m_pSensorFrame = nullptr;
}

void SlateFrameRendererWithCV::EnsureSlateTexture()
{
    if (m_texture2D == nullptr)
    {
        m_texture2D = std::make_shared<BasicHologram::Texture2D>(
            m_deviceResources,
            m_Width,
            m_Height);
    }
}

void SlateFrameRendererWithCV::FrameProcessing()
{
    HRESULT hr = S_OK;

    while (!m_fExit)
    {
        WaitForSingleObject(m_hFrameEvent, INFINITE);

        //Sleep(500);
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            {
                std::lock_guard<std::mutex> frameguard(m_frameMutex);

                if (m_pSensorFrameIn == nullptr)
                {
                    return;
                }
                if (m_pSensorFrame)
                {
                    m_pSensorFrame->Release();
                }
                m_pSensorFrame = m_pSensorFrameIn;
                m_pSensorFrameIn = nullptr;
            }

            {
                std::lock_guard<std::mutex> guard(m_cornerMutex);

                m_corners.clear();
                m_ids.clear();

                m_cvFrameProcessor(m_pSensorFrame, m_cvResultMat, m_ids, m_corners);

                m_centers.clear();

                for (int i = 0; i < m_corners.size(); i++)
                {
                    float sumx = 0.0f;
                    float sumy = 0.0f;
                    cv::Point2f center;

                    for (int j = 0; j < m_corners[i].size(); j++)
                    {
                        sumx += m_corners[i][j].x;
                        sumy += m_corners[i][j].y;
                    }

                    center.x = sumx / 4.0f;
                    center.y = sumy / 4.0f;

                    m_centers.push_back(center);
                }

                m_pSensorFrame->GetTimeStamp(&m_timeStamp);
            }


            ResetEvent(m_hFrameEvent);
        }
    }
}

bool SlateFrameRendererWithCV::GetFirstCenter(float *px, float *py, ResearchModeSensorTimestamp *pTimeStamp)
{
    std::lock_guard<std::mutex> guard(m_cornerMutex);

    if (m_centers.size() >= 1)
    {
        *px = m_centers[0].x;
        *py = m_centers[0].y;

        return true;
    }

    return false;
}

void SlateFrameRendererWithCV::FrameProcessingThread(SlateFrameRendererWithCV* pSlateFrameRendererWithCV)
{
    pSlateFrameRendererWithCV->FrameProcessing();
}


void SlateFrameRendererWithCV::StartCVProcessing(BYTE bright)
{
    m_Width = 200;
    m_Height = 200;
    m_texture2D = nullptr;

    EnsureSlateTexture();

    m_pFrameUpdateThread = new std::thread(FrameProcessingThread, this);
}


