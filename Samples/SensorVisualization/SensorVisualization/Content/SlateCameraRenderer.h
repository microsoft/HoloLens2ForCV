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

#pragma once

#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"
#include "ShaderStructures.h"
#include "researchmode\ResearchModeApi.h"
#include "ModelRenderer.h"

namespace BasicHologram
{
    class SlateCameraRenderer :
        public ModelRenderer
    {
    public:
        SlateCameraRenderer(std::shared_ptr<DX::DeviceResources> const& deviceResources, float fWidth, float fHeight, IResearchModeSensor *pLLSensor, HANDLE hasData, ResearchModeSensorConsent *pCamAccessConsent) :
            ModelRenderer(deviceResources)
        {
            m_pRMCameraSensor = pLLSensor;
            m_pRMCameraSensor->AddRef();
            m_pSensorFrame = nullptr;
            m_frameCallback = nullptr;
            m_slateWidth = fWidth;
            m_slateHeight = fHeight;

            m_pixelShaderFile = L"ms-appx:///PixelShader.cso";

            m_pCameraUpdateThread = new std::thread(CameraUpdateThread, this, hasData, pCamAccessConsent);
        }
        virtual ~SlateCameraRenderer()
        {
            m_fExit = true;
            m_pCameraUpdateThread->join();

            if (m_pRMCameraSensor)
            {
				m_pRMCameraSensor->CloseStream();
                m_pRMCameraSensor->Release();
            }
        }

        DirectX::XMMATRIX GetModelRotation()
        {
            return DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f), -DirectX::XM_PIDIV2);
        }

        void SetFrameCallBack(std::function<void(IResearchModeSensorFrame*, PVOID frameCtx)> frameCallback, PVOID frameCtx)
        {
            m_frameCallback = frameCallback;
            m_frameCtx = frameCtx;
        }

    protected:

        void GetModelVertices(std::vector<VertexPositionColor> &returnedModelVertices);

        void GetModelTriangleIndices(std::vector<unsigned short> &triangleIndices);

        virtual void UpdateSlateTexture();

        virtual void EnsureTextureForCameraFrame(IResearchModeSensorFrame* pSensorFrame);
        virtual void UpdateTextureFromCameraFrame(IResearchModeSensorFrame* pSensorFrame, std::shared_ptr<Texture2D> texture2D);

        static void CameraUpdateThread(SlateCameraRenderer* pSlateCameraRenderer, HANDLE hasData, ResearchModeSensorConsent *pCamAccessConsent);

		IResearchModeSensor *m_pRMCameraSensor = nullptr;
		IResearchModeSensorFrame* m_pSensorFrame;
        uint64_t m_refreshTimeInMilliseconds = 0;
        float m_slateWidth;
        float m_slateHeight;

        std::function<void(IResearchModeSensorFrame*, PVOID frameCtx)> m_frameCallback;
        PVOID m_frameCtx;

        std::thread *m_pCameraUpdateThread;
        bool m_fExit = { false };
    };
}
