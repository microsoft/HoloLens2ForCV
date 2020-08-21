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
// Author: Casey Meekhof cmeekhof@microsoft.com

#pragma once

#include "Common/FilterDoubleExponential.h"

using namespace DirectX;


class RecordedValue
{
public:

	RecordedValue();
	void Reset();

	unsigned GetUserFrameCount() const;	
	XMVECTOR GetValue(int FramesAgo) const;	
	XMVECTOR GetSmoothedValue(int FramesAgo) const;
	void RecordValue(XMVECTOR Value);	
	void SetSmoothingParameters(float smoothing = 0.25f, float correction = 0.0f, float prediction = 0.0f, float jitterRadius = 0.05f, float maxDeviationRadius = 0.05f);

private:
	static const int s_MaxFrames = 60;

	XMVECTOR m_RecordedValues[s_MaxFrames];
	XMVECTOR m_SmoothedRecordedValues[s_MaxFrames];
	int m_CurRecordedFrame;
	int m_RecordedFrameCount;
	FilterDoubleExponential m_RecordedValueFilter;
};
