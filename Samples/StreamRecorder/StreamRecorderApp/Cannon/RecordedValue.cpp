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


#include "RecordedValue.h"


RecordedValue::RecordedValue()
{
	Reset();
}

unsigned RecordedValue::GetUserFrameCount() const
{
    return m_RecordedFrameCount;
}

XMVECTOR RecordedValue::GetValue(int FramesAgo) const
{
    int iFrameIndex = m_CurRecordedFrame-FramesAgo;
    if(iFrameIndex<0)
        iFrameIndex += m_RecordedFrameCount;

    return m_RecordedValues[iFrameIndex];
}		  	

XMVECTOR RecordedValue::GetSmoothedValue(int FramesAgo) const
{
    int iFrameIndex = m_CurRecordedFrame-FramesAgo;
    if(iFrameIndex<0)
        iFrameIndex += m_RecordedFrameCount;

    return m_SmoothedRecordedValues[iFrameIndex];
}			

void RecordedValue::Reset()
{
	m_CurRecordedFrame = 0;
	m_RecordedFrameCount = 0;
	m_RecordedValueFilter.Reset();
}

void RecordedValue::RecordValue(XMVECTOR Value)
{
    m_CurRecordedFrame = (m_CurRecordedFrame+1) %s_MaxFrames;
    if(m_RecordedFrameCount < s_MaxFrames)
        ++(m_RecordedFrameCount);

    m_RecordedValues[m_CurRecordedFrame] = Value;

    m_RecordedValueFilter.Update(Value);
    m_SmoothedRecordedValues[m_CurRecordedFrame] = m_RecordedValueFilter.GetFilteredValue();
}

void RecordedValue::SetSmoothingParameters(float smoothing, float correction, float prediction, float jitterRadius, float maxDeviationRadius)
{
	m_RecordedValueFilter.SetParameters(smoothing, correction, prediction, jitterRadius, maxDeviationRadius);
}

