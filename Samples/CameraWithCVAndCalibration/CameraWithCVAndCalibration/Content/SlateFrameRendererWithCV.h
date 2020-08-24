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
#include "researchmode\ResearchModeApi.h"
#include "ShaderStructures.h"
#include "ModelRenderer.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>  // cv::Canny()
#include <opencv2/aruco.hpp>
#include <opencv2/core/mat.hpp>

namespace BasicHologram
{
    class SlateFrameRendererWithCV :
        public ModelRenderer
    {
    public:
        SlateFrameRendererWithCV(std::shared_ptr<DX::DeviceResources> const& deviceResources,
                                 std::function<void(IResearchModeSensorFrame* pSensorFrame, cv::Mat& cvResultMat, std::vector<int> &ids, std::vector<std::vector<cv::Point2f>> &corners)> cvFrameProcessor) :
            ModelRenderer(deviceResources)
        {
            m_Width = 0;
            m_Height = 0;
            m_pSensorFrame = nullptr;
            m_pSensorFrameIn = nullptr;
            m_hFrameEvent = NULL;

            m_pixelShaderFile = L"ms-appx:///PixelShader.cso";

            m_fExit = false;
            m_hFrameEvent = CreateEvent(NULL, true, false, NULL);

            m_cvFrameProcessor = cvFrameProcessor;
        }
        virtual ~SlateFrameRendererWithCV()
        {
            m_fExit = true;
            m_pFrameUpdateThread->join();
        }

        DirectX::XMMATRIX GetModelRotation();

        void UpdateSlateTextureWithBitmap(const BYTE *pImage, UINT uWidth, UINT uHeight);
        void StartCVProcessing(BYTE bright);

        static void FrameReadyCallback(IResearchModeSensorFrame* pSensorFrame, PVOID frameCtx)
        {
            SlateFrameRendererWithCV *pSlateTexture = (SlateFrameRendererWithCV*)frameCtx;
            std::lock_guard<std::mutex> guard(pSlateTexture->m_frameMutex);

            if (pSlateTexture->m_pSensorFrameIn)
            {
                pSlateTexture->m_pSensorFrameIn->Release();
            }
            pSlateTexture->m_pSensorFrameIn = pSensorFrame;
            if (pSensorFrame)
            {
                pSensorFrame->AddRef();
            }

            SetEvent(pSlateTexture->m_hFrameEvent);
        }

        bool GetFirstCenter(float *px, float *py, ResearchModeSensorTimestamp *pTimeStamp);

    protected:

        void GetModelVertices(std::vector<VertexPositionColor> &returnedModelVertices);

        void GetModelTriangleIndices(std::vector<unsigned short> &triangleIndices);

        virtual void UpdateSlateTexture();

        void EnsureSlateTexture();

        void FrameProcessing();

        static void FrameProcessingThread(SlateFrameRendererWithCV* pSlateCameraRenderer);
        bool m_fExit = { false };
        std::thread *m_pFrameUpdateThread;
        HANDLE m_hFrameEvent;

        std::mutex m_frameMutex;
        IResearchModeSensorFrame* m_pSensorFrame;
        IResearchModeSensorFrame* m_pSensorFrameIn;
        UINT m_Width;
        UINT m_Height;

        std::vector<std::vector<cv::Point2f>> m_corners;
        std::vector<cv::Point2f> m_centers;
        std::vector<int> m_ids;
        ResearchModeSensorTimestamp m_timeStamp;

        std::mutex m_cornerMutex;

        std::function<void(IResearchModeSensorFrame* pSensorFrame, cv::Mat& cvResultMat, std::vector<int> &ids, std::vector<std::vector<cv::Point2f>> &corners)> m_cvFrameProcessor;

        cv::Mat m_cvResultMat;
    };
}
