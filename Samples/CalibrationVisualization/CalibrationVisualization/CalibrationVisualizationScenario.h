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
    class CalibrationVisualizationScenario : public Scenario
    {
    public:
        CalibrationVisualizationScenario(std::shared_ptr<DX::DeviceResources> const& deviceResources);
        virtual ~CalibrationVisualizationScenario();

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

        virtual void UpdateState();

    protected:

        IResearchModeSensorDevice *m_pSensorDevice;
        std::vector<ResearchModeSensorDescriptor> m_sensorDescriptors;
        IResearchModeSensor *m_pLFCameraSensor = nullptr;
        DirectX::XMFLOAT4X4 m_LFCameraPose;

        IResearchModeSensor *m_pRFCameraSensor = nullptr;
        DirectX::XMFLOAT4X4 m_RFCameraPose;

        IResearchModeSensor *m_pLLCameraSensor = nullptr;
        DirectX::XMFLOAT4X4 m_LLCameraPose;

        IResearchModeSensor *m_pRRCameraSensor = nullptr;
        DirectX::XMFLOAT4X4 m_RRCameraPose;

        IResearchModeSensor *m_pLTCameraSensor = nullptr;
        DirectX::XMFLOAT4X4 m_LTCameraPose;

        std::vector<std::shared_ptr<ModelRenderer>> m_modelRenderers;
        int m_state = 0;
        double m_lastUpdateTime = 0.0;

        std::shared_ptr<VectorModel> m_rayLeft;
        std::shared_ptr<VectorModel> m_rayRight;
        int m_rayCol = 0;
        int m_rayRow = 0;
    };
}
