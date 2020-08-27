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
    class SensorVisualizationScenario : public Scenario
    {
    public:
        SensorVisualizationScenario(std::shared_ptr<DX::DeviceResources> const& deviceResources);
        virtual ~SensorVisualizationScenario();

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
        static void ImuAccessOnComplete(ResearchModeSensorConsent consent);

    protected:

        IResearchModeSensorDevice *m_pSensorDevice;
        IResearchModeSensorDeviceConsent* m_pSensorDeviceConsent;
        std::vector<ResearchModeSensorDescriptor> m_sensorDescriptors;
        IResearchModeSensor *m_pLFCameraSensor = nullptr;
        IResearchModeSensor *m_pRFCameraSensor = nullptr;
        IResearchModeSensor *m_pLTSensor = nullptr;
        IResearchModeSensor *m_pAHATSensor = nullptr;
        IResearchModeSensor *m_pAccelSensor = nullptr;

        std::shared_ptr<XAxisModel>                                 m_xaxisOriginRenderer;
        std::shared_ptr<YAxisModel>                                 m_yaxisOriginRenderer;
        std::shared_ptr<ZAxisModel>                                 m_zaxisOriginRenderer;
        std::vector<std::shared_ptr<ModelRenderer>>                 m_modelRenderers;
        std::shared_ptr<AccelRenderer>                              m_AccelRenderer;
        std::shared_ptr<SlateCameraRenderer>                        m_LFCameraRenderer;
        std::shared_ptr<SlateCameraRenderer>                        m_LTCameraRenderer;
    };
}
