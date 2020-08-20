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
#include "SlateCameraRenderer.h"

using namespace BasicHologram;
using namespace DirectX;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI::Input::Spatial;

void SlateCameraRenderer::CameraUpdateThread(SlateCameraRenderer* pSlateCameraRenderer, HANDLE hasData, ResearchModeSensorConsent *pCamAccessConsent)
{
    HRESULT hr = S_OK;
    LARGE_INTEGER qpf;
    uint64_t lastQpcNow = 0;

    QueryPerformanceFrequency(&qpf);

    if (hasData != nullptr)
    {
        DWORD waitResult = WaitForSingleObject(hasData, INFINITE);

        if (waitResult == WAIT_OBJECT_0)
        {
            switch (*pCamAccessConsent)
            {
            case ResearchModeSensorConsent::Allowed:
                OutputDebugString(L"Access is granted");
                break;
            case ResearchModeSensorConsent::DeniedBySystem:
                OutputDebugString(L"Access is denied by the system");
                hr = E_ACCESSDENIED;
                break;
            case ResearchModeSensorConsent::DeniedByUser:
                OutputDebugString(L"Access is denied by the user");
                hr = E_ACCESSDENIED;
                break;
            case ResearchModeSensorConsent::NotDeclaredByApp:
                OutputDebugString(L"Capability is not declared in the app manifest");
                hr = E_ACCESSDENIED;
                break;
            case ResearchModeSensorConsent::UserPromptRequired:
                OutputDebugString(L"Capability user prompt required");
                hr = E_ACCESSDENIED;
                break;
            default:
                OutputDebugString(L"Access is denied by the system");
                hr = E_ACCESSDENIED;
                break;
            }
        }
        else
        {
            hr = E_UNEXPECTED;
        }
    }

    if (FAILED(hr))
    {
        return;
    }

    winrt::check_hresult(pSlateCameraRenderer->m_pRMCameraSensor->OpenStream());

    while (!pSlateCameraRenderer->m_fExit && pSlateCameraRenderer->m_pRMCameraSensor)
    {
        static int gFrameCount = 0;
        HRESULT hr = S_OK;
        IResearchModeSensorFrame* pSensorFrame = nullptr;
        LARGE_INTEGER qpcNow;
        uint64_t uqpcNow;
        QueryPerformanceCounter(&qpcNow);
        uqpcNow = qpcNow.QuadPart;

        winrt::check_hresult(pSlateCameraRenderer->m_pRMCameraSensor->GetNextBuffer(&pSensorFrame));

        {
            if (lastQpcNow != 0)
            {
                pSlateCameraRenderer->m_refreshTimeInMilliseconds =
                    (1000 *
                    (uqpcNow - lastQpcNow)) /
                    qpf.QuadPart;
            }

            std::lock_guard<std::mutex> guard(pSlateCameraRenderer->m_mutex);

            if (pSlateCameraRenderer->m_frameCallback)
            {
                pSlateCameraRenderer->m_frameCallback(pSensorFrame, pSlateCameraRenderer->m_frameCtx);
            }

            if (pSlateCameraRenderer->m_pSensorFrame)
            {
                pSlateCameraRenderer->m_pSensorFrame->Release();
            }

            pSlateCameraRenderer->m_pSensorFrame = pSensorFrame;
            lastQpcNow = uqpcNow;
        }
    }

    if (pSlateCameraRenderer->m_pRMCameraSensor)
    {
        pSlateCameraRenderer->m_pRMCameraSensor->CloseStream();
    }
}

void SlateCameraRenderer::UpdateSlateTexture()
{
    char printString[1000];

    if (m_pSensorFrame)
    {
        EnsureTextureForCameraFrame(m_pSensorFrame);

        UpdateTextureFromCameraFrame(m_pSensorFrame, m_texture2D);

        sprintf(printString, "####CameraSlate %I64d\n", m_refreshTimeInMilliseconds);
        OutputDebugStringA(printString);
    }
}

void SlateCameraRenderer::EnsureTextureForCameraFrame(IResearchModeSensorFrame* pSensorFrame)
{
    ResearchModeSensorResolution resolution;
    BYTE *pImage = nullptr;

    if (m_texture2D == nullptr)
    {
        pSensorFrame->GetResolution(&resolution);

        m_texture2D = std::make_shared<BasicHologram::Texture2D>(
            m_deviceResources,
            resolution.Width,
            resolution.Height);
    }
}

BYTE ConvertDepthPixel(USHORT v, BYTE bSigma, USHORT mask, USHORT maxshort, const int vmin, const int vmax)
{
    if ((mask != 0) && (bSigma & mask) > 0)
    {
        v = 0;
    }

    if ((maxshort != 0) && (v > maxshort))
    {
        v = 0;
    }
    
    float colorValue = 0.0f;
    if (v <= vmin)
    {
        colorValue = 0.0f;
    }
    else if (v >= vmax)
    {
        colorValue = 1.0f;
    }
    else
    {
        colorValue = (float)(v - vmin) / (float)(vmax - vmin);
    }

    return (BYTE)(colorValue * 255);
}


void SlateCameraRenderer::UpdateTextureFromCameraFrame(IResearchModeSensorFrame* pSensorFrame, std::shared_ptr<Texture2D> texture2D)
{
    HRESULT hr = S_OK;
    char printString[1000];
    UINT32 gain = 0;
    UINT64 exposure = 0;

    ResearchModeSensorResolution resolution;
    IResearchModeSensorVLCFrame *pVLCFrame = nullptr;
    IResearchModeSensorDepthFrame *pDepthFrame = nullptr;
    size_t outBufferCount = 0;
    const BYTE *pImage = nullptr;

    pSensorFrame->GetResolution(&resolution);

    hr = pSensorFrame->QueryInterface(IID_PPV_ARGS(&pVLCFrame));

    if (FAILED(hr))
    {
        hr = pSensorFrame->QueryInterface(IID_PPV_ARGS(&pDepthFrame));
    }
    
    if (pVLCFrame)
    {
        pVLCFrame->GetBuffer(&pImage, &outBufferCount);

        pVLCFrame->GetGain(&gain);
        pVLCFrame->GetExposure(&exposure);

        sprintf(printString, "####CameraGain %d %I64d\n", gain, exposure);
        OutputDebugStringA(printString);

        void* mappedTexture =
            texture2D->MapCPUTexture<void>(
                D3D11_MAP_WRITE /* mapType */);

        for (UINT i = 0; i < resolution.Height; i++)
        {
            for (UINT j = 0; j < resolution.Width; j++)
            {
                UINT32 pixel = 0;
                BYTE inputPixel = pImage[resolution.Width * i + j];
                pixel = inputPixel | (inputPixel << 8) | (inputPixel << 16);

                if (m_pRMCameraSensor->GetSensorType() == LEFT_FRONT)
                {
                    *((UINT32*)(mappedTexture)+((texture2D->GetRowPitch() / 4) * i + (resolution.Width - j - 1))) = pixel;
                }
                else if (m_pRMCameraSensor->GetSensorType() == RIGHT_FRONT)
                {
                    *((UINT32*)(mappedTexture)+((texture2D->GetRowPitch() / 4) * (resolution.Height - i - 1) + j)) = pixel;
                }
                else
                {
                    *((UINT32*)(mappedTexture)+((m_texture2D->GetRowPitch() / 4) * i + j)) = pixel;
                }
            }
        }
    }

    if (pDepthFrame)
    {
        int maxClampDepth = 0;
        USHORT maxshort = 0;
        USHORT mask = 0;
        const BYTE *pSigma = nullptr;

        if (m_pRMCameraSensor->GetSensorType() == DEPTH_LONG_THROW)
        {
            mask = 0x80;
            maxClampDepth = 4000;
        }
        else if (m_pRMCameraSensor->GetSensorType() == DEPTH_AHAT)
        {
            mask = 0x0;
            maxClampDepth = 1000;
            maxshort = 4090;
        }

        if (m_pRMCameraSensor->GetSensorType() == DEPTH_LONG_THROW)
        {
            hr = pDepthFrame->GetSigmaBuffer(&pSigma, &outBufferCount);
        }

        const UINT16 *pDepth = nullptr;
        pDepthFrame->GetBuffer(&pDepth, &outBufferCount);

        void* mappedTexture =
            texture2D->MapCPUTexture<void>(
                D3D11_MAP_WRITE /* mapType */);

        for (UINT i = 0; i < resolution.Height; i++)
        {
            for (UINT j = 0; j < resolution.Width; j++)
            {
                UINT32 pixel = 0;
                //BYTE inputPixel = pImage[resolution.Width * i + j];
                BYTE inputPixel = ConvertDepthPixel(
                    pDepth[resolution.Width * i + j],
                    pSigma ? pSigma[resolution.Width * i + j] : 0,
                    mask,
                    maxshort,
                    0,
                    maxClampDepth);

                pixel = inputPixel | (inputPixel << 8) | (inputPixel << 16);

                *((UINT32*)(mappedTexture)+((m_texture2D->GetRowPitch() / 4) * i + j)) = pixel;
            }
        }
    }
    
	if (pVLCFrame)
	{
		pVLCFrame->Release();
	}
    
	if (pDepthFrame)
	{
		pDepthFrame->Release();
	}

    texture2D->UnmapCPUTexture();
    texture2D->CopyCPU2GPU();
}


void SlateCameraRenderer::GetModelVertices(std::vector<VertexPositionColor> &modelVertices)
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


void SlateCameraRenderer::GetModelTriangleIndices(std::vector<unsigned short> &triangleIndices)
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


