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
#include "SensorVisualizationScenario.h"
#include "Common\DirectXHelper.h"

extern "C"
HMODULE LoadLibraryA(
    LPCSTR lpLibFileName
);

using namespace BasicHologram;
using namespace concurrency;
using namespace Microsoft::WRL;
using namespace std::placeholders;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::Gaming::Input;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::UI::Input::Spatial;

static ResearchModeSensorConsent camAccessCheck;
static HANDLE camConsentGiven;
static ResearchModeSensorConsent imuAccessCheck;
static HANDLE imuConsentGiven;

SensorVisualizationScenario::SensorVisualizationScenario(std::shared_ptr<DX::DeviceResources> const& deviceResources) :
    Scenario(deviceResources)
{
}

SensorVisualizationScenario::~SensorVisualizationScenario()
{
    if (m_pLFCameraSensor)
    {
        m_pLFCameraSensor->Release();
    }
    if (m_pRFCameraSensor)
    {
        m_pRFCameraSensor->Release();
    }
    if (m_pLTSensor)
    {
        m_pLTSensor->Release();
    }
    if (m_pLTSensor)
    {
        m_pLTSensor->Release();
    }

    if (m_pSensorDevice)
    {
        m_pSensorDevice->EnableEyeSelection();
        m_pSensorDevice->Release();
    }
}

void SensorVisualizationScenario::CamAccessOnComplete(ResearchModeSensorConsent consent)
{
    camAccessCheck = consent;
    SetEvent(camConsentGiven);
}

void SensorVisualizationScenario::ImuAccessOnComplete(ResearchModeSensorConsent consent)
{
    imuAccessCheck = consent;
    SetEvent(imuConsentGiven);
}

void SensorVisualizationScenario::IntializeSensors()
{
    HRESULT hr = S_OK;
    size_t sensorCount = 0;
    camConsentGiven = CreateEvent(nullptr, true, false, nullptr);
    imuConsentGiven = CreateEvent(nullptr, true, false, nullptr);

    HMODULE hrResearchMode = LoadLibraryA("ResearchModeAPI");
    if (hrResearchMode)
    {
        typedef HRESULT(__cdecl* PFN_CREATEPROVIDER) (IResearchModeSensorDevice** ppSensorDevice);
        PFN_CREATEPROVIDER pfnCreate = reinterpret_cast<PFN_CREATEPROVIDER>(GetProcAddress(hrResearchMode, "CreateResearchModeSensorDevice"));
        if (pfnCreate)
        {
            winrt::check_hresult(pfnCreate(&m_pSensorDevice));
        }
        else
        {
            winrt::check_hresult(E_INVALIDARG);
        }
    }

    winrt::check_hresult(m_pSensorDevice->QueryInterface(IID_PPV_ARGS(&m_pSensorDeviceConsent)));
    winrt::check_hresult(m_pSensorDeviceConsent->RequestCamAccessAsync(SensorVisualizationScenario::CamAccessOnComplete));
    winrt::check_hresult(m_pSensorDeviceConsent->RequestIMUAccessAsync(SensorVisualizationScenario::ImuAccessOnComplete));

    m_pSensorDevice->DisableEyeSelection();

    winrt::check_hresult(m_pSensorDevice->GetSensorCount(&sensorCount));
    m_sensorDescriptors.resize(sensorCount);

    winrt::check_hresult(m_pSensorDevice->GetSensorDescriptors(m_sensorDescriptors.data(), m_sensorDescriptors.size(), &sensorCount));

    for (auto sensorDescriptor : m_sensorDescriptors)
    {
        IResearchModeSensor *pSensor = nullptr;

        if (sensorDescriptor.sensorType == LEFT_FRONT)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pLFCameraSensor));
        }

        if (sensorDescriptor.sensorType == RIGHT_FRONT)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pRFCameraSensor));
        }

// Long throw and AHAT modes can not be used at the same time.
#define DEPTH_USE_LONG_THROW

#ifdef DEPTH_USE_LONG_THROW
        if (sensorDescriptor.sensorType == DEPTH_LONG_THROW)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pLTSensor));
        }
#else
        if (sensorDescriptor.sensorType == DEPTH_AHAT)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pAHATSensor));
        }
#endif
        if (sensorDescriptor.sensorType == IMU_ACCEL)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pAccelSensor));
        }
        if (sensorDescriptor.sensorType == IMU_GYRO)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pGyroSensor));
        }
        if (sensorDescriptor.sensorType == IMU_MAG)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pMagSensor));
        }
    }
}

void SensorVisualizationScenario::IntializeModelRendering()
{
    SlateCameraRenderer* pLLSlateCameraRenderer = nullptr;

    {
        static constexpr DirectX::XMFLOAT4X4 XMFloat4x4YZSwap{
            1, 0, 0, 0,
            0, 0, 1, 0,
            0, 1, 0, 0,
            0, 0, 0, 1 };

        DirectX::XMMATRIX groupRotation = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 0.f, 1.f, 0.f), -DirectX::XM_PIDIV2);
        groupRotation = DirectX::XMLoadFloat4x4(&XMFloat4x4YZSwap) * groupRotation;

        auto xaxisOriginRenderer = std::make_shared<XAxisModel>(m_deviceResources, 0.15f, 0.005f);

        xaxisOriginRenderer->SetGroupScaleFactor(3.0);
        xaxisOriginRenderer->SetGroupTransform(groupRotation);
        xaxisOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f));
        m_modelRenderers.push_back(xaxisOriginRenderer);

        float3 offset;
        offset.x = 0.05f;
        offset.y = 0.05f;
        offset.z = 0.1f;

        xaxisOriginRenderer->SetOffset(offset);
        m_xaxisOriginRenderer = xaxisOriginRenderer;

        auto yaxisOriginRenderer = std::make_shared<YAxisModel>(m_deviceResources, 0.15f, 0.005f);

        yaxisOriginRenderer->SetGroupScaleFactor(3.0);
        yaxisOriginRenderer->SetGroupTransform(groupRotation);
        yaxisOriginRenderer->SetColor(DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
        m_modelRenderers.push_back(yaxisOriginRenderer);

        yaxisOriginRenderer->SetOffset(offset);
        m_yaxisOriginRenderer = yaxisOriginRenderer;

        auto zaxisOriginRenderer = std::make_shared<ZAxisModel>(m_deviceResources, 0.15f, 0.005f);

        zaxisOriginRenderer->SetGroupScaleFactor(3.0);
        zaxisOriginRenderer->SetGroupTransform(groupRotation);
        zaxisOriginRenderer->SetColor(DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
        m_modelRenderers.push_back(zaxisOriginRenderer);

        zaxisOriginRenderer->SetOffset(offset);
        m_zaxisOriginRenderer = zaxisOriginRenderer;
    }

    if (m_pLFCameraSensor)
    {
        // Initialize the sample hologram.
        auto slateCameraRenderer = std::make_shared<SlateCameraRenderer>(m_deviceResources, 0.4f, 0.4f, m_pLFCameraSensor, camConsentGiven, &camAccessCheck);

        float3 offset;
        offset.x = -0.2f;
        offset.y = 0.2f;
        offset.z = 0.0f;

        slateCameraRenderer->SetOffset(offset);
        m_modelRenderers.push_back(slateCameraRenderer);

        pLLSlateCameraRenderer = slateCameraRenderer.get();

        m_LFCameraRenderer = slateCameraRenderer;
    }

    if (m_pRFCameraSensor)
    {
        // Initialize the sample hologram.
        auto slateCameraRenderer = std::make_shared<SlateCameraRenderer>(m_deviceResources, 0.4f, 0.4f, m_pRFCameraSensor, camConsentGiven, &camAccessCheck);

        float3 offset;
        offset.x = 0.2f;
        offset.y = 0.2f;
        offset.z = 0.0f;

        slateCameraRenderer->SetOffset(offset);
        m_modelRenderers.push_back(slateCameraRenderer);
    }

    if (m_pLTSensor)
    {
        DirectX::XMMATRIX modelRotation = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 0.f, 1.f, 0.f), DirectX::XM_PIDIV2);

        // Initialize the sample hologram.
        auto slateCameraRenderer = std::make_shared<SlateCameraRenderer>(m_deviceResources, 0.8f, 0.4f, m_pLTSensor, camConsentGiven, &camAccessCheck);

        float3 offset;
        offset.x = -0.2f;
        offset.y = -0.2f;
        offset.z = 0.0f;

        slateCameraRenderer->SetOffset(offset);
        slateCameraRenderer->SetModelTransform(modelRotation);
        m_modelRenderers.push_back(slateCameraRenderer);

        m_LTCameraRenderer = slateCameraRenderer;
    }

    if (m_pAHATSensor)
    {
        // Initialize the sample hologram.
        auto slateCameraRenderer = std::make_shared<SlateCameraRenderer>(m_deviceResources, 0.4f, 0.4f, m_pAHATSensor, camConsentGiven, &camAccessCheck);

        float3 offset;
        offset.x = 0.2f;
        offset.y = -0.2f;
        offset.z = 0.0f;

        slateCameraRenderer->SetOffset(offset);
        m_modelRenderers.push_back(slateCameraRenderer);
    }

    if (m_pAccelSensor)
    {
        m_AccelRenderer = std::make_shared<AccelRenderer>(m_deviceResources, m_pAccelSensor, imuConsentGiven, &imuAccessCheck);
    }

    if (m_pGyroSensor)
    {
        m_GyroRenderer = std::make_shared<GyroRenderer>(m_deviceResources, m_pGyroSensor, imuConsentGiven, &imuAccessCheck);
    }

    if (m_pMagSensor)
    {
        m_MagRenderer = std::make_shared<MagRenderer>(m_deviceResources, m_pMagSensor, imuConsentGiven, &imuAccessCheck);
    }
}

void SensorVisualizationScenario::PositionHologram(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose, const DX::StepTimer& timer)
{
    // When a Pressed gesture is detected, the sample hologram will be repositioned
    // two meters in front of the user.
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->PositionHologram(pointerPose, timer);
    }
}

void SensorVisualizationScenario::PositionHologramNoSmoothing(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose)
{
    // When a Pressed gesture is detected, the sample hologram will be repositioned
    // two meters in front of the user.
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->PositionHologramNoSmoothing(pointerPose);
    }
}

void SensorVisualizationScenario::UpdateModels(DX::StepTimer &timer)
{
    HRESULT hr = S_OK;
    DirectX::XMFLOAT3 imuSample;
//  char printString[1000];
    float vectorLength = 0.0f;
    float scalex = 0;
    float scaley = 0;
    float scalez = 0;
    DirectX::XMMATRIX xscaleTransform;
    DirectX::XMMATRIX yscaleTransform;
    DirectX::XMMATRIX zscaleTransform;
    uint64_t lastLFTimeStamp = 0;
    uint64_t lastLTTimeStamp = 0;

    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->Update(timer);
    }

#define VISUALIZE_ACCEL

#ifdef VISUALIZE_ACCEL
    m_AccelRenderer->GetAccelSample(&imuSample);
#else
    m_GyroRenderer->GetGyroSample(&imuSample);
#endif

    vectorLength = sqrt(imuSample.x * imuSample.x + imuSample.y * imuSample.y + imuSample.z * imuSample.z);

    scalex = imuSample.x / vectorLength;
    scaley = imuSample.y / vectorLength;
    scalez = imuSample.z / vectorLength;

    xscaleTransform = DirectX::XMMatrixScaling(scalex, 1.0f, 1.0f);
    m_xaxisOriginRenderer->SetModelTransform(xscaleTransform);

    yscaleTransform = DirectX::XMMatrixScaling(1.0f, scaley, 1.0f);
    m_yaxisOriginRenderer->SetModelTransform(yscaleTransform);

    zscaleTransform = DirectX::XMMatrixScaling(1.0f, 1.0f, scalez);
    m_zaxisOriginRenderer->SetModelTransform(zscaleTransform);

    lastLFTimeStamp = m_LFCameraRenderer->GetLastTimeStamp();
    if (m_LTCameraRenderer)
    {
        lastLTTimeStamp = m_LTCameraRenderer->GetLastTimeStamp();
    }
}

void SensorVisualizationScenario::RenderModels()
{
    // Draw the sample hologram.
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->Render();
    }
}

void SensorVisualizationScenario::OnDeviceLost()
{
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->ReleaseDeviceDependentResources();
    }
}

void SensorVisualizationScenario::OnDeviceRestored()
{
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->CreateDeviceDependentResources();
    }
}
