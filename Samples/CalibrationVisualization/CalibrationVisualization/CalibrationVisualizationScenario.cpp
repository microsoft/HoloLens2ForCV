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
#include "CalibrationVisualizationScenario.h"
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

CalibrationVisualizationScenario::CalibrationVisualizationScenario(std::shared_ptr<DX::DeviceResources> const& deviceResources) :
    Scenario(deviceResources)
{
}

CalibrationVisualizationScenario::~CalibrationVisualizationScenario()
{
    if (m_pLFCameraSensor)
    {
        m_pLFCameraSensor->Release();
    }

    if (m_pSensorDevice)
    {
        m_pSensorDevice->EnableEyeSelection();
        m_pSensorDevice->Release();
    }
}

void CalibrationVisualizationScenario::IntializeSensors()
{
    HRESULT hr = S_OK;
    size_t sensorCount = 0;

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

    m_pSensorDevice->DisableEyeSelection();

    winrt::check_hresult(m_pSensorDevice->GetSensorCount(&sensorCount));
    m_sensorDescriptors.resize(sensorCount);

    winrt::check_hresult(m_pSensorDevice->GetSensorDescriptors(m_sensorDescriptors.data(), m_sensorDescriptors.size(), &sensorCount));

    for (auto sensorDescriptor : m_sensorDescriptors)
    {
        IResearchModeSensor *pSensor = nullptr;
        IResearchModeCameraSensor *pCameraSensor = nullptr;

        if (sensorDescriptor.sensorType == LEFT_FRONT)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pLFCameraSensor));
            winrt::check_hresult(m_pLFCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor)));
            winrt::check_hresult(pCameraSensor->GetCameraExtrinsicsMatrix(&m_LFCameraPose));
        }

        if (sensorDescriptor.sensorType == LEFT_LEFT)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pLLCameraSensor));
            winrt::check_hresult(m_pLLCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor)));
            winrt::check_hresult(pCameraSensor->GetCameraExtrinsicsMatrix(&m_LLCameraPose));
        }

        if (sensorDescriptor.sensorType == RIGHT_FRONT)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pRFCameraSensor));
            winrt::check_hresult(m_pRFCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor)));
            winrt::check_hresult(pCameraSensor->GetCameraExtrinsicsMatrix(&m_RFCameraPose));
        }

        if (sensorDescriptor.sensorType == RIGHT_RIGHT)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pRRCameraSensor));
            winrt::check_hresult(m_pRRCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor)));
            winrt::check_hresult(pCameraSensor->GetCameraExtrinsicsMatrix(&m_RRCameraPose));
        }

        if (sensorDescriptor.sensorType == DEPTH_LONG_THROW)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pLTCameraSensor));
            winrt::check_hresult(m_pLTCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor)));
            winrt::check_hresult(pCameraSensor->GetCameraExtrinsicsMatrix(&m_LTCameraPose));
        }

    }
}

void CalibrationVisualizationScenario::UpdateState()
{
    m_state++;

    DirectX::XMMATRIX groupRotation = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(1.f, 0.f, 0.f, 0.f), -DirectX::XM_PIDIV2/2);
    groupRotation = groupRotation * DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f), (-DirectX::XM_PIDIV2 / 2 + (DirectX::XM_PIDIV2 / 8 * m_state)));
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->SetGroupTransform(groupRotation);
    }
}


void CalibrationVisualizationScenario::IntializeModelRendering()
{
    HRESULT hr = S_OK;

    DirectX::XMMATRIX cameraNodeToRigPoseInverted;
    DirectX::XMMATRIX cameraNodeToRigPose;
    DirectX::XMVECTOR det;
    float xy[2] = {0};
    float uv[2];

    DirectX::XMMATRIX groupRotation = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(1.f, 0.f, 0.f, 0.f), -DirectX::XM_PIDIV2/2);
    groupRotation = groupRotation * DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f), (-DirectX::XM_PIDIV2 / 2 + (DirectX::XM_PIDIV2 / 4 * m_state)));

    auto axisOriginRenderer = std::make_shared<XYZAxisModel>(m_deviceResources, 0.6f, 0.001f);

    axisOriginRenderer->SetGroupScaleFactor(3.0);
    axisOriginRenderer->SetGroupTransform(groupRotation);
    axisOriginRenderer->SetColors(DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
        DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
        DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
    m_modelRenderers.push_back(axisOriginRenderer);

    if (m_pRFCameraSensor)
    {
        IResearchModeCameraSensor *pCameraSensor = nullptr;

        // Initialize the sample hologram.
        auto axisRFRenderer = std::make_shared<XYZAxisModel>(m_deviceResources, 0.05f, 0.001f);

        cameraNodeToRigPose = DirectX::XMLoadFloat4x4(&m_RFCameraPose);
        det = XMMatrixDeterminant(cameraNodeToRigPose);
        cameraNodeToRigPoseInverted = DirectX::XMMatrixInverse(&det, cameraNodeToRigPose);

        axisRFRenderer->SetGroupScaleFactor(3.0);
        axisRFRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        axisRFRenderer->SetGroupTransform(groupRotation);

        axisRFRenderer->SetColors(DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
        m_modelRenderers.push_back(axisRFRenderer);

        hr = m_pRFCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor));

        uv[0] = 0.0f;
        uv[1] = 0.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        auto vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f;
        uv[1] = 0.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f;
        uv[1] = 480.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 0.0f;
        uv[1] = 480.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f / 2;
        uv[1] = 480.0f / 2;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        m_rayRight = vectorOriginRenderer;

        pCameraSensor->Release();
    }

    if (m_pLFCameraSensor)
    {
        IResearchModeCameraSensor *pCameraSensor = nullptr;

        // Initialize the sample hologram.
        auto axisLFRenderer = std::make_shared<XYZAxisModel>(m_deviceResources, 0.1f, 0.001f);

        cameraNodeToRigPose = DirectX::XMLoadFloat4x4(&m_LFCameraPose);
        det = XMMatrixDeterminant(cameraNodeToRigPose);
        cameraNodeToRigPoseInverted = DirectX::XMMatrixInverse(&det, cameraNodeToRigPose);

        //axisLFRenderer->SetOffset(offset);
        axisLFRenderer->SetGroupScaleFactor(3.0);
        axisLFRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        axisLFRenderer->SetGroupTransform(groupRotation);

        axisLFRenderer->SetColors(DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
        m_modelRenderers.push_back(axisLFRenderer);


        hr = m_pLFCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor));

        uv[0] = 0.0f;
        uv[1] = 0.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        auto vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f;
        uv[1] = 0.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f;
        uv[1] = 480.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 0.0f;
        uv[1] = 480.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f / 2;
        uv[1] = 480.0f / 2;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        m_rayLeft = vectorOriginRenderer;

        pCameraSensor->Release();
    }

    if (m_pLLCameraSensor)
    {
        auto axisLLRenderer = std::make_shared<XYZAxisModel>(m_deviceResources, 0.05f, 0.001f);

        cameraNodeToRigPose = DirectX::XMLoadFloat4x4(&m_LLCameraPose);
        det = XMMatrixDeterminant(cameraNodeToRigPose);
        cameraNodeToRigPoseInverted = DirectX::XMMatrixInverse(&det, cameraNodeToRigPose);

        axisLLRenderer->SetGroupScaleFactor(3.0);
        axisLLRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        axisLLRenderer->SetGroupTransform(groupRotation);

        axisLLRenderer->SetColors(DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
        m_modelRenderers.push_back(axisLLRenderer);
    }

    if (m_pRRCameraSensor)
    {
        auto axisRRRenderer = std::make_shared<XYZAxisModel>(m_deviceResources, 0.05f, 0.001f);

        cameraNodeToRigPose = DirectX::XMLoadFloat4x4(&m_RRCameraPose);
        det = XMMatrixDeterminant(cameraNodeToRigPose);
        cameraNodeToRigPoseInverted = DirectX::XMMatrixInverse(&det, cameraNodeToRigPose);

        axisRRRenderer->SetGroupScaleFactor(3.0);
        axisRRRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        axisRRRenderer->SetGroupTransform(groupRotation);

        axisRRRenderer->SetColors(DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
        m_modelRenderers.push_back(axisRRRenderer); 
    }

    if (m_pLTCameraSensor)
    {
        IResearchModeCameraSensor *pCameraSensor = nullptr;

        auto axisLTRenderer = std::make_shared<XYZAxisModel>(m_deviceResources, 0.05f, 0.001f);

        cameraNodeToRigPose = DirectX::XMLoadFloat4x4(&m_LTCameraPose);
        det = XMMatrixDeterminant(cameraNodeToRigPose);
        cameraNodeToRigPoseInverted = DirectX::XMMatrixInverse(&det, cameraNodeToRigPose);

        axisLTRenderer->SetGroupScaleFactor(3.0);
        axisLTRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        axisLTRenderer->SetGroupTransform(groupRotation);

        axisLTRenderer->SetColors(DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
        m_modelRenderers.push_back(axisLTRenderer);

        hr = m_pLTCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor));

        uv[0] = 0.0f;
        uv[1] = 0.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        auto vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 320.0f;
        uv[1] = 0.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 320.0f;
        uv[1] = 288.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 0.0f;
        uv[1] = 288.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(3.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

    }

    /*
    auto yaxisRenderer = std::make_shared<YAxisModel>(m_deviceResources, 0.6f, 0.01f);

    yaxisRenderer->SetOffset(offset);
    yaxisRenderer->SetColor(DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
    m_modelRenderers.push_back(yaxisRenderer);

    auto zaxisRenderer = std::make_shared<ZAxisModel>(m_deviceResources, 0.6f, 0.01f);

    zaxisRenderer->SetOffset(offset);
    zaxisRenderer->SetColor(DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
    m_modelRenderers.push_back(zaxisRenderer);
    */
}

void CalibrationVisualizationScenario::PositionHologram(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose, const DX::StepTimer& timer)
{
    // When a Pressed gesture is detected, the sample hologram will be repositioned
    // two meters in front of the user.
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->PositionHologram(pointerPose, timer);
    }
}

void CalibrationVisualizationScenario::PositionHologramNoSmoothing(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose)
{
    // When a Pressed gesture is detected, the sample hologram will be repositioned
    // two meters in front of the user.
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->PositionHologramNoSmoothing(pointerPose);
    }
}

void CalibrationVisualizationScenario::UpdateModels(DX::StepTimer &timer)
{
    float xy[2] = {0};
    float uv[2];

    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->Update(timer);
    }

    if (m_lastUpdateTime == 0.0)
    {
        m_lastUpdateTime = timer.GetTotalSeconds();
    }
    else if (timer.GetTotalSeconds() - m_lastUpdateTime > 0.005)
    {
        HRESULT hr = S_OK;

        IResearchModeCameraSensor *pCameraSensor = nullptr;

        m_lastUpdateTime = timer.GetTotalSeconds();

        /*
        char printString[1000];     
         
        sprintf(printString, "####Setp: %d %d\n", row, col);

        OutputDebugStringA(printString);
        */

        m_rayCol += 20;
        if (m_rayCol >= 640)
        {
            m_rayCol = 0;
            m_rayRow += 20;
            if (m_rayRow >= 480)
            {
                m_rayRow = 0;
            }
        }
        
        if (m_pLFCameraSensor)
        {
            hr = m_pLFCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor));

            uv[0] = (float)m_rayCol;
            uv[1] = (float)m_rayRow;

            pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

            m_rayLeft->SetDirection(DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

            pCameraSensor->Release();
        }

        if (m_pRFCameraSensor)
        {
            hr = m_pRFCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor));

            uv[0] = (float)m_rayCol;
            uv[1] = (float)m_rayRow;

            pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

            m_rayRight->SetDirection(DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

            pCameraSensor->Release();
        }
    }
    
}

void CalibrationVisualizationScenario::RenderModels()
{
    // Draw the sample hologram.
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->Render();
    }
}

void CalibrationVisualizationScenario::OnDeviceLost()
{
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->ReleaseDeviceDependentResources();
    }
}

void CalibrationVisualizationScenario::OnDeviceRestored()
{
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->CreateDeviceDependentResources();
    }
}
