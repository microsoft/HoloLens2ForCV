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
#include "CalibrationProjectionVisualizationScenario.h"
#include "Common\DirectXHelper.h"

#include "content\OpenCVFrameProcessing.h"

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

CalibrationProjectionVisualizationScenario::CalibrationProjectionVisualizationScenario(std::shared_ptr<DX::DeviceResources> const& deviceResources) :
    Scenario(deviceResources)
{
}

CalibrationProjectionVisualizationScenario::~CalibrationProjectionVisualizationScenario()
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

void CalibrationProjectionVisualizationScenario::CamAccessOnComplete(ResearchModeSensorConsent consent)
{
    camAccessCheck = consent;
    SetEvent(camConsentGiven);
}

void CalibrationProjectionVisualizationScenario::IntializeSensors()
{
    HRESULT hr = S_OK;
    size_t sensorCount = 0;
    camConsentGiven = CreateEvent(nullptr, true, false, nullptr);

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
    winrt::check_hresult(m_pSensorDeviceConsent->RequestCamAccessAsync(CalibrationProjectionVisualizationScenario::CamAccessOnComplete));

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

            DirectX::XMFLOAT4 zeros = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

            DirectX::XMMATRIX cameraPose = XMLoadFloat4x4(&m_LFCameraPose);
            DirectX::XMMATRIX cameraRotation = cameraPose;
            cameraRotation.r[3] = DirectX::XMLoadFloat4(&zeros);
            XMStoreFloat4x4(&m_LFCameraRotation, cameraRotation);

            DirectX::XMVECTOR det = XMMatrixDeterminant(cameraRotation);
            XMStoreFloat4(&m_LFRotDeterminant, det);
        }

        if (sensorDescriptor.sensorType == RIGHT_FRONT)
        {
            winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pRFCameraSensor));

            winrt::check_hresult(m_pRFCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor)));

            winrt::check_hresult(pCameraSensor->GetCameraExtrinsicsMatrix(&m_RFCameraPose));

            DirectX::XMFLOAT4 zeros = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

            DirectX::XMMATRIX cameraPose = XMLoadFloat4x4(&m_RFCameraPose);
            DirectX::XMMATRIX cameraRotation = cameraPose;
            cameraRotation.r[3] = DirectX::XMLoadFloat4(&zeros);
            XMStoreFloat4x4(&m_RFCameraRotation, cameraRotation);

            DirectX::XMVECTOR det = XMMatrixDeterminant(cameraRotation);
            XMStoreFloat4(&m_RFRotDeterminant, det);
        }
    }
}

void CalibrationProjectionVisualizationScenario::UpdateState()
{
    m_state++;

    DirectX::XMMATRIX groupRotation = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(1.f, 0.f, 0.f, 0.f), -DirectX::XM_PIDIV2/2);
    groupRotation = groupRotation * DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f), (-DirectX::XM_PIDIV2 / 2 + (DirectX::XM_PIDIV2 / 8 * m_state)));

    XMStoreFloat4x4(&m_groupRotation, groupRotation);

    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        if (m_modelRenderers[i]->IsAxisModel())
        {
            m_modelRenderers[i]->SetGroupTransform(groupRotation);
        }
    }
}

void CalibrationProjectionVisualizationScenario::IntializeSensorFrameModelRendering()
{
    HRESULT hr = S_OK;

    DirectX::XMMATRIX cameraNodeToRigPoseInverted;
    DirectX::XMMATRIX cameraNodeToRigPose;
    DirectX::XMVECTOR det;
    float xy[2] = {0};
    float uv[2];

    DirectX::XMMATRIX groupRotation = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(1.f, 0.f, 0.f, 0.f), -DirectX::XM_PIDIV2/2);
    groupRotation = groupRotation * DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f), (-DirectX::XM_PIDIV2 / 2 + (DirectX::XM_PIDIV2 / 4 * m_state)));

    XMStoreFloat4x4(&m_groupRotation, groupRotation);

    {
        IResearchModeCameraSensor *pCameraSensor = nullptr;

        // Initialize the sample hologram.
        auto axisRFRenderer = std::make_shared<XYZAxisModel>(m_deviceResources, 0.05f, 0.001f);

        cameraNodeToRigPose = DirectX::XMLoadFloat4x4(&m_RFCameraPose);
        det = XMMatrixDeterminant(cameraNodeToRigPose);
        cameraNodeToRigPoseInverted = DirectX::XMMatrixInverse(&det, cameraNodeToRigPose);

        axisRFRenderer->SetGroupScaleFactor(1.0);
        axisRFRenderer->SetModelTransform(cameraNodeToRigPoseInverted);

        axisRFRenderer->SetColors(DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
        m_modelRenderers.push_back(axisRFRenderer);

        winrt::check_hresult(m_pRFCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor)));

        uv[0] = 0.0f;
        uv[1] = 0.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        auto vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(1.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f;
        uv[1] = 0.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(1.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f;
        uv[1] = 480.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(1.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 0.0f;
        uv[1] = 480.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(1.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f / 2;
        uv[1] = 480.0f / 2;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.6f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(1.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        m_rayRight = vectorOriginRenderer;

        pCameraSensor->Release();
    }

    {
        IResearchModeCameraSensor *pCameraSensor = nullptr;

        // Initialize the sample hologram.
        auto axisLFRenderer = std::make_shared<XYZAxisModel>(m_deviceResources, 0.1f, 0.001f);

        cameraNodeToRigPose = DirectX::XMLoadFloat4x4(&m_LFCameraPose);
        det = XMMatrixDeterminant(cameraNodeToRigPose);
        cameraNodeToRigPoseInverted = DirectX::XMMatrixInverse(&det, cameraNodeToRigPose);

        //axisLFRenderer->SetOffset(offset);
        axisLFRenderer->SetGroupScaleFactor(1.0);
        axisLFRenderer->SetModelTransform(cameraNodeToRigPoseInverted);

        axisLFRenderer->SetColors(DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
            DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f));
        m_modelRenderers.push_back(axisLFRenderer);


        winrt::check_hresult(m_pLFCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor)));

        uv[0] = 0.0f;
        uv[1] = 0.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        auto vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(1.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f;
        uv[1] = 0.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(1.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f;
        uv[1] = 480.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(1.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 0.0f;
        uv[1] = 480.0f;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.1f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(1.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        uv[0] = 640.0f / 2;
        uv[1] = 480.0f / 2;

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

        vectorOriginRenderer = std::make_shared<VectorModel>(m_deviceResources, 0.6f, 0.0005f, DirectX::XMFLOAT3(xy[0], xy[1], 1.0f));

        vectorOriginRenderer->SetGroupScaleFactor(1.0);
        vectorOriginRenderer->SetModelTransform(cameraNodeToRigPoseInverted);
        vectorOriginRenderer->SetGroupTransform(groupRotation);
        vectorOriginRenderer->SetColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
        m_modelRenderers.push_back(vectorOriginRenderer);

        m_rayLeft = vectorOriginRenderer;

        pCameraSensor->Release();
    }
}

void CalibrationProjectionVisualizationScenario::InitializeArucoRendering()
{
    {
        SlateCameraRenderer* pLFSlateCameraRenderer = nullptr;

        if (m_pLFCameraSensor)
        {
            // Initialize the sample hologram.
            auto slateCameraRenderer = std::make_shared<SlateCameraRenderer>(m_deviceResources, m_pLFCameraSensor, camConsentGiven, &camAccessCheck);

            float3 offset;
            offset.x = -0.2f;
            offset.y = 0.2f;
            offset.z = 0.0f;

            slateCameraRenderer->SetOffset(offset);
            slateCameraRenderer->DisableRendering();
            m_modelRenderers.push_back(slateCameraRenderer);

            pLFSlateCameraRenderer = slateCameraRenderer.get();
        }

        auto slateTextureRenderer = std::make_shared<SlateFrameRendererWithCV>(m_deviceResources, ProcessRmFrameWithAruco);
        slateTextureRenderer->StartCVProcessing(0xff);

        float3 offset;
        offset.x = -0.2f;
        offset.y = -0.2f;
        offset.z = -0.4f;

        slateTextureRenderer->SetOffset(offset);
        m_modelRenderers.push_back(slateTextureRenderer);
        m_arucoDetectorLeft = slateTextureRenderer;

        pLFSlateCameraRenderer->SetFrameCallBack(SlateFrameRendererWithCV::FrameReadyCallback, slateTextureRenderer.get());
    }

    {
        SlateCameraRenderer* pRFSlateCameraRenderer = nullptr;

        if (m_pLFCameraSensor)
        {
            // Initialize the sample hologram.
            auto slateCameraRenderer = std::make_shared<SlateCameraRenderer>(m_deviceResources, m_pRFCameraSensor, camConsentGiven, &camAccessCheck);

            float3 offset;
            offset.x = -0.2f;
            offset.y = 0.2f;
            offset.z = 0.0f;

            slateCameraRenderer->SetOffset(offset);
            slateCameraRenderer->DisableRendering();
            m_modelRenderers.push_back(slateCameraRenderer);

            pRFSlateCameraRenderer = slateCameraRenderer.get();
        }

        auto slateTextureRenderer = std::make_shared<SlateFrameRendererWithCV>(m_deviceResources, ProcessRmFrameWithAruco);
        slateTextureRenderer->StartCVProcessing(0xff);

        float3 offset;
        offset.x = 0.2f;
        offset.y = -0.2f;
        offset.z = -0.4f;

        slateTextureRenderer->SetOffset(offset);
        slateTextureRenderer->SetModelTransform(DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(1.f, 0.f, 0.f, 0.f), DirectX::XM_PI) * 
                                                DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f), DirectX::XM_PI));
        m_modelRenderers.push_back(slateTextureRenderer);
        m_arucoDetectorRight = slateTextureRenderer;

        pRFSlateCameraRenderer->SetFrameCallBack(SlateFrameRendererWithCV::FrameReadyCallback, slateTextureRenderer.get());
    }

}

void CalibrationProjectionVisualizationScenario::IntializeModelRendering()
{
    IntializeSensorFrameModelRendering();
    InitializeArucoRendering();
}

void CalibrationProjectionVisualizationScenario::PositionHologram(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose, const DX::StepTimer& timer)
{
    // When a Pressed gesture is detected, the sample hologram will be repositioned
    // two meters in front of the user.
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->PositionHologram(pointerPose, timer);
    }
}

void CalibrationProjectionVisualizationScenario::PositionHologramNoSmoothing(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose)
{
    // When a Pressed gesture is detected, the sample hologram will be repositioned
    // two meters in front of the user.
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->PositionHologramNoSmoothing(pointerPose);
    }
}

void CalibrationProjectionVisualizationScenario::UpdateModels(DX::StepTimer &timer)
{
    DirectX::XMMATRIX groupRotation = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(1.f, 0.f, 0.f, 0.f), -DirectX::XM_PIDIV2/2);

    groupRotation = XMLoadFloat4x4(&m_groupRotation);

    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->Update(timer);
    }

    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        if (m_modelRenderers[i]->IsAxisModel())
        {
            m_modelRenderers[i]->SetGroupTransform(groupRotation);
        }
    }

    float x[2];
    float uv[2];
    ResearchModeSensorTimestamp timeStamp;

    if (m_arucoDetectorLeft->GetFirstCenter(uv, uv + 1, &timeStamp))
    {
        HRESULT hr = S_OK;        
        IResearchModeCameraSensor *pCameraSensor = nullptr;
        
        winrt::check_hresult(m_pLFCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor)));

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, x);

        m_rayLeft->SetDirection(DirectX::XMFLOAT3(x[0], x[1], 1.0f));
        m_rayLeft->EnableRendering();

        pCameraSensor->Release();
    }
    else
    {
        m_rayLeft->DisableRendering();
    }

    if (m_arucoDetectorRight->GetFirstCenter(uv, uv + 1, &timeStamp))
    {
        HRESULT hr = S_OK;

        if (m_stationaryReferenceFrame)
        {
            SpatialCoordinateSystem currentCoordinateSystem =
                m_stationaryReferenceFrame.CoordinateSystem();
        }

        IResearchModeCameraSensor *pCameraSensor = nullptr;
        
        winrt::check_hresult(m_pRFCameraSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor)));

        pCameraSensor->MapImagePointToCameraUnitPlane(uv, x);

        m_rayRight->SetDirection(DirectX::XMFLOAT3(x[0], x[1], 1.0f));
        m_rayRight->EnableRendering();

        pCameraSensor->Release();
    }
    else
    {
        m_rayRight->DisableRendering();
    }
}

void CalibrationProjectionVisualizationScenario::RenderModels()
{
    // Draw the sample hologram.
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->Render();
    }
}

void CalibrationProjectionVisualizationScenario::OnDeviceLost()
{
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->ReleaseDeviceDependentResources();
    }
}

void CalibrationProjectionVisualizationScenario::OnDeviceRestored()
{
    for (int i = 0; i < m_modelRenderers.size(); i++)
    {
        m_modelRenderers[i]->CreateDeviceDependentResources();
    }
}
