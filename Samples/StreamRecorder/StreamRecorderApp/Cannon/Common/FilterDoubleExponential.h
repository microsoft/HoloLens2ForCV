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

#include <DirectXMath.h>
#include <cstring>

using namespace DirectX;

// Holt Double Exponential Smoothing filter

struct FilterDoubleExponentialData
{
	XMVECTOR rawPosition;
	XMVECTOR filteredPosition;
	XMVECTOR trend;

	unsigned frameCount;
};

class FilterDoubleExponential
{
public:
	FilterDoubleExponential() { SetParameters(); }

	void SetParameters(float smoothing = 0.5f, float correction = 0.0f, float prediction = 0.0f, float jitterRadius = 0.05f, float maxDeviationRadius = 0.05f)
	{
		m_maxDeviationRadius = maxDeviationRadius; // Size of the max prediction radius Can snap back to noisy data when too high
		m_smoothing = smoothing;                   // How much smothing will occur.  Will lag when too high
		m_correction = correction;                 // How much to correct back from prediction.  Can make things springy
		m_prediction = prediction;                 // Amount of prediction into the future to use. Can over shoot when too high
		m_jitterRadius = jitterRadius;             // Size of the radius where jitter is removed. Can do too much smoothing when too high
		
		Reset();
	}

	void Reset()
	{
		m_filteredValue = DirectX::XMVectorZero();
		memset(&m_history, 0, sizeof(m_history));
	}

	void Update(const XMVECTOR& newRawPosition)
	{
		// If joint is invalid, reset the filter
		if (XMVector3Equal(newRawPosition, XMVectorZero()))
		{
			m_history.frameCount = 0;
		}

		// Check for divide by zero. Use an epsilon of a 10th of a millimeter
		m_jitterRadius = XMMax(0.0001f, m_jitterRadius);

		XMVECTOR newFilteredPosition;
		XMVECTOR newPredictedPosition;
		XMVECTOR deltaVector;
		float deltaDistance;
		XMVECTOR newTrend;

		// Initial start values
		if (m_history.frameCount == 0)
		{
			newFilteredPosition = newRawPosition;
			newTrend = XMVectorZero();
			m_history.frameCount++;
		}
		else if (m_history.frameCount == 1)
		{
			newFilteredPosition = (newRawPosition + m_history.rawPosition) * 0.5f;
			deltaVector = newFilteredPosition - m_history.filteredPosition;
			newTrend = deltaVector * m_correction + m_history.trend * (1.0f - m_correction);
			m_history.frameCount++;
		}
		else
		{
			// First apply jitter filter
			deltaVector = newRawPosition - m_history.filteredPosition;
			deltaDistance = XMVectorGetX(XMVector3Length(deltaVector));

			if (deltaDistance <= m_jitterRadius)
			{
				newFilteredPosition = newRawPosition * deltaDistance / m_jitterRadius + m_history.filteredPosition * (1.0f - deltaDistance / m_jitterRadius);
			}
			else
			{
				newFilteredPosition = newRawPosition;
			}

			// Now the double exponential smoothing filter
			newFilteredPosition = newFilteredPosition * (1.0f - m_smoothing) + (m_history.filteredPosition + m_history.trend) * m_smoothing;

			deltaVector = newFilteredPosition - m_history.filteredPosition;
			newTrend = deltaVector * m_correction + m_history.trend * (1.0f - m_correction);
		}

		// Predict into the future to reduce latency
		newPredictedPosition = newFilteredPosition + newTrend * m_prediction;

		// Check that we are not too far away from raw data
		deltaVector = newPredictedPosition - newRawPosition;
		deltaDistance = XMVectorGetX(XMVector3Length(deltaVector));

		if (deltaDistance > m_maxDeviationRadius)
		{
			newPredictedPosition = newPredictedPosition * m_maxDeviationRadius / deltaDistance + newRawPosition * (1.0f - m_maxDeviationRadius / deltaDistance);
		}

		// Save the data from this frame
		m_history.rawPosition = newRawPosition;
		m_history.filteredPosition = newFilteredPosition;
		m_history.trend = newTrend;

		// Output the data
		m_filteredValue = newPredictedPosition;
		m_filteredValue = XMVectorSetW(m_filteredValue, 1.0f);
	}

	XMVECTOR GetFilteredValue() { return m_filteredValue; }

private:
	XMVECTOR m_filteredValue;
	FilterDoubleExponentialData m_history;

	float m_smoothing;
	float m_correction;
	float m_prediction;
	float m_jitterRadius;
	float m_maxDeviationRadius;
};
