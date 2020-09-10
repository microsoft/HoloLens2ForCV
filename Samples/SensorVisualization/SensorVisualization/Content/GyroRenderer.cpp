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
#include "GyroRenderer.h"
#include "Common\DirectXHelper.h"
#include <mutex>

using namespace BasicHologram;
using namespace DirectX;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI::Input::Spatial;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
void GyroRenderer::GyroUpdateLoop()
{
    uint64_t lastSocTick = 0; 
    uint64_t lastHupTick = 0; 
    LARGE_INTEGER qpf;
    uint64_t lastQpcNow = 0;

    // Cache the QueryPerformanceFrequency
    QueryPerformanceFrequency(&qpf);

    winrt::check_hresult(m_pGyroSensor->OpenStream());

    while (!m_fExit)
    {
        char printString[1000];

        IResearchModeSensorFrame* pSensorFrame = nullptr;
        IResearchModeGyroFrame *pGyroFrame = nullptr;
        ResearchModeSensorTimestamp timeStamp;
        const GyroDataStruct *pGyroBuffer = nullptr;
        size_t BufferOutLength;

        winrt::check_hresult(m_pGyroSensor->GetNextBuffer(&pSensorFrame));

        winrt::check_hresult(pSensorFrame->QueryInterface(IID_PPV_ARGS(&pGyroFrame)));

        {
            std::lock_guard<std::mutex> guard(m_sampleMutex);

            winrt::check_hresult(pGyroFrame->GetCalibratedGyro(&m_gyroSample));
        }

        winrt::check_hresult(pGyroFrame->GetCalibratedGyroSamples(
            &pGyroBuffer,
            &BufferOutLength));

        lastHupTick = 0;
        std::string hupTimeDeltas = "";

        for (UINT i = 0; i < BufferOutLength; i++)
        {
            pSensorFrame->GetTimeStamp(&timeStamp);
            if (lastHupTick != 0)
            {
                if (pGyroBuffer[i].VinylHupTicks < lastHupTick)
                {
                    sprintf(printString, "####GYRO BAD HUP ORDERING\n");
                    OutputDebugStringA(printString);
                    DebugBreak();
                }
                sprintf(printString, " %I64d", (pGyroBuffer[i].VinylHupTicks - lastHupTick) / 1000); // Microseconds

                hupTimeDeltas = hupTimeDeltas + printString;

            }
            lastHupTick = pGyroBuffer[i].VinylHupTicks;
        }

        hupTimeDeltas = hupTimeDeltas + "\n";
        //OutputDebugStringA(hupTimeDeltas.c_str());

        pSensorFrame->GetTimeStamp(&timeStamp);
        LARGE_INTEGER qpcNow;
        uint64_t uqpcNow;
        QueryPerformanceCounter(&qpcNow);
        uqpcNow = qpcNow.QuadPart;

        if (lastSocTick != 0)
        {
            uint64_t timeInMilliseconds =
                (1000 *
                (uqpcNow - lastQpcNow)) /
                qpf.QuadPart;

            if (timeStamp.HostTicks < lastSocTick)
            {
                DebugBreak();
            }

            sprintf(printString, "####Gyro: % 3.4f % 3.4f % 3.4f %I64d %I64d\n",
                m_gyroSample.x,
                m_gyroSample.y,
                m_gyroSample.z,
                    (((timeStamp.HostTicks - lastSocTick) * 1000) / timeStamp.HostTicksPerSecond), // Milliseconds
                timeInMilliseconds);
            OutputDebugStringA(printString);
        }
        lastSocTick = timeStamp.HostTicks;
        lastQpcNow = uqpcNow;

        if (pSensorFrame)
        {
            pSensorFrame->Release();
        }

        if (pGyroFrame)
        {
            pGyroFrame->Release();
        }
    }

    winrt::check_hresult(m_pGyroSensor->CloseStream());
}

void GyroRenderer::GetGyroSample(DirectX::XMFLOAT3 *pGyroSample)
{
    std::lock_guard<std::mutex> guard(m_sampleMutex);

    *pGyroSample = m_gyroSample;
}

// This function uses a SpatialPointerPose to position the world-locked hologram
// two meters in front of the user's heading.
void GyroRenderer::PositionHologram(SpatialPointerPose const& pointerPose)
{
}

// Called once per frame. Rotates the cube, and calculates and sets the model matrix
// relative to the position transform indicated by hologramPositionTransform.
void GyroRenderer::Update(DX::StepTimer const& timer)
{
}

void GyroRenderer::SetSensorFrame(IResearchModeSensorFrame* pSensorFrame)
{

}

void GyroRenderer::GyroUpdateThread(GyroRenderer* pGyroRenderer, HANDLE hasData, ResearchModeSensorConsent *pCamAccessConsent)
{
    HRESULT hr = S_OK;

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

    pGyroRenderer->GyroUpdateLoop();
}

void GyroRenderer::UpdateSample()
{
    HRESULT hr = S_OK;
}
