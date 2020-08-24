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
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>  // cv::Canny()
#include <opencv2/aruco.hpp>
#include <opencv2/core/mat.hpp>


void ProcessRmFrameWithAruco(IResearchModeSensorFrame* pSensorFrame, cv::Mat& cvResultMat, std::vector<int> &ids, std::vector<std::vector<cv::Point2f>> &corners)
{
    HRESULT hr = S_OK;
    ResearchModeSensorResolution resolution;
    size_t outBufferCount = 0;
    const BYTE *pImage = nullptr;
    IResearchModeSensorVLCFrame *pVLCFrame = nullptr;
    pSensorFrame->GetResolution(&resolution);
    static cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);

    hr = pSensorFrame->QueryInterface(IID_PPV_ARGS(&pVLCFrame));

    if (SUCCEEDED(hr))
    {
        pVLCFrame->GetBuffer(&pImage, &outBufferCount);

        cv::Mat processed(resolution.Height, resolution.Width, CV_8U, (void*)pImage);
        cv::aruco::detectMarkers(processed, dictionary, corners, ids);

        cvResultMat = processed;

        // if at least one marker detected
        if (ids.size() > 0)
            cv::aruco::drawDetectedMarkers(cvResultMat, corners, ids);
    }

    if (pVLCFrame)
    {
        pVLCFrame->Release();
    }
}


void ProcessRmFrameWithCanny(IResearchModeSensorFrame* pSensorFrame, cv::Mat& cvResultMat)
{
    HRESULT hr = S_OK;
    ResearchModeSensorResolution resolution;
    size_t outBufferCount = 0;
    const BYTE *pImage = nullptr;
    IResearchModeSensorVLCFrame *pVLCFrame = nullptr;
    pSensorFrame->GetResolution(&resolution);

    hr = pSensorFrame->QueryInterface(IID_PPV_ARGS(&pVLCFrame));

    if (SUCCEEDED(hr))
    {
        pVLCFrame->GetBuffer(&pImage, &outBufferCount);

        cv::Mat processed(resolution.Height, resolution.Width, CV_8U, (void*)pImage);

        cv::Canny(processed, cvResultMat, 400, 1000, 5);
    }

    if (pVLCFrame)
    {
        pVLCFrame->Release();
    }
}


