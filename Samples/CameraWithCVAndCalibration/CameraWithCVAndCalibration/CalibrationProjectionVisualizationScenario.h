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

#include "BasicHologramMain.h"

namespace BasicHologram
{
    class CalibrationProjectionVisualizationScenario : public Scenario
    {
    public:
        CalibrationProjectionVisualizationScenario(std::shared_ptr<DX::DeviceResources> const& deviceResources);
        virtual ~CalibrationProjectionVisualizationScenario();

        void IntializeSensors();
        void IntializeModelRendering();
        void UpdateModels(DX::StepTimer &timer);
        void PositionHologram(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose, const DX::StepTimer& timer);
        void PositionHologramNoSmoothing(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose);
        winrt::Windows::Foundation::Numerics::float3 const& GetPosition()
        {
            return m_modelRenderers[0]->GetPosition();
        }
        void RenderModels();
        void OnDeviceLost();
        void OnDeviceRestored();
        static void CamAccessOnComplete(ResearchModeSensorConsent consent);

        virtual void UpdateState();

    protected:

        void IntializeSensorFrameModelRendering();
        void InitializeArucoRendering();

        IResearchModeSensorDevice *m_pSensorDevice;
        IResearchModeSensorDeviceConsent* m_pSensorDeviceConsent;
        std::vector<ResearchModeSensorDescriptor> m_sensorDescriptors;
        IResearchModeSensor *m_pLFCameraSensor = nullptr;
        DirectX::XMFLOAT4X4 m_LFCameraPose;
        DirectX::XMFLOAT4X4 m_LFCameraRotation;
        DirectX::XMFLOAT4 m_LFRotDeterminant;

        IResearchModeSensor *m_pRFCameraSensor = nullptr;
        DirectX::XMFLOAT4X4 m_RFCameraPose;
        DirectX::XMFLOAT4X4 m_RFCameraRotation;
        DirectX::XMFLOAT4 m_RFRotDeterminant;

        DirectX::XMFLOAT4X4 m_groupRotation;

        std::vector<std::shared_ptr<ModelRenderer>> m_modelRenderers;
        std::shared_ptr<VectorModel> m_rayLeft;
        std::shared_ptr<VectorModel> m_rayRight;

        std::shared_ptr<SlateFrameRendererWithCV> m_arucoDetectorLeft;
        std::shared_ptr<SlateFrameRendererWithCV> m_arucoDetectorRight;

        std::vector<std::shared_ptr<SlateFrameRendererWithCV>> m_arucoDetectors;
        int m_state = 0;
    };
}
