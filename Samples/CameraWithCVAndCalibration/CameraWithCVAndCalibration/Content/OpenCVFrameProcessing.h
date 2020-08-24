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

#include "researchmode\ResearchModeApi.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>  // cv::Canny()
#include <opencv2/aruco.hpp>
#include <opencv2/core/mat.hpp>

void ProcessRmFrameWithAruco(IResearchModeSensorFrame* pSensorFrame, cv::Mat& cvResultMat, std::vector<int> &ids, std::vector<std::vector<cv::Point2f>> &corners);
void ProcessRmFrameWithCanny(IResearchModeSensorFrame* pSensorFrame, cv::Mat& cvResultMat);




