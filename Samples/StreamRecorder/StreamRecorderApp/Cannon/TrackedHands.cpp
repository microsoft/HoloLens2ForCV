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


#include "TrackedHands.h"


TrackedHands::TrackedHands() :
	m_timestamps{ 0 },
	m_isNewFrameAvailable(false)
{
	m_headPosition = XMVectorZero();
	m_headForward = XMVectorZero();
	m_headUp = XMVectorZero();
	m_headTransform = XMMatrixIdentity();
	
	for (size_t handIndex = 0; handIndex < HAND_COUNT; ++handIndex)
	{
		ResetHand(handIndex);
		m_handTrackedStates[handIndex] = false;
	}
}

void TrackedHands::ResetHand(size_t handIndex)
{
	if (handIndex >= HAND_COUNT)
		return;

	for (size_t jointIndex = 0; jointIndex < kHandJointCount; ++jointIndex)
	{
		m_handJoints[handIndex * kHandJointCount + jointIndex].Reset();
		m_handOrientations[handIndex * kHandJointCount + jointIndex].Reset();
		m_handRadii[handIndex * kHandJointCount + jointIndex] = 0.0f;
	}	
}

bool TrackedHands::IsHandTracked(size_t handIndex)
{
	if (handIndex < HAND_COUNT)
		return m_handTrackedStates[handIndex];
	else
		return false;
}

XMVECTOR TrackedHands::GetJoint(size_t handIndex, HandJointIndex jointIndex)
{
	if (handIndex < HAND_COUNT && (size_t)jointIndex < kHandJointCount)
	{
		return m_handJoints[handIndex * kHandJointCount + (size_t)jointIndex].GetValue(0);
	}
	else
	{
		return XMVectorZero();
	}
}

XMVECTOR TrackedHands::GetJointOrientation(size_t handIndex, HandJointIndex jointIndex)
{
	if (handIndex < HAND_COUNT && (size_t)jointIndex < kHandJointCount)
	{
		return m_handOrientations[handIndex * kHandJointCount + (size_t)jointIndex].GetValue(0);
	}
	else
	{
		return XMVectorZero();
	}
}

XMMATRIX TrackedHands::GetOrientedJoint(size_t handIndex, HandJointIndex jointIndex)
{
	if (handIndex < HAND_COUNT && (size_t)jointIndex < kHandJointCount)
	{
		const size_t idx = handIndex * kHandJointCount + (size_t)jointIndex;

		const XMVECTOR pWS = GetJoint(handIndex, jointIndex);
		const XMVECTOR qWS = GetJointOrientation(handIndex, jointIndex);
		return XMMatrixAffineTransformation(XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), XMVectorZero(), qWS, pWS);
	}
	else
	{
		return XMMatrixIdentity();
	}
}

XMVECTOR TrackedHands::GetIndexTipSurfacePosition(size_t handIndex, int framesAgo)
{
	XMVECTOR indexTip = GetJointHistory(handIndex, HandJointIndex::IndexTip).GetValue(framesAgo);
	XMVECTOR indexDistal = GetJointHistory(handIndex, HandJointIndex::IndexDistal).GetValue(framesAgo);
	float indexRadius = GetJointRadius(handIndex, HandJointIndex::IndexTip);

	return XMVectorSetW(indexTip + XMVector3Normalize(indexTip - indexDistal) * indexRadius * 1.5f, 1.0f);
}

float TrackedHands::GetJointRadius(size_t handIndex, HandJointIndex jointIndex)
{
	if (handIndex < HAND_COUNT && (size_t)jointIndex < kHandJointCount)
	{
		return m_handRadii[handIndex * kHandJointCount + (size_t)jointIndex];
	}
	else
	{
		return 0.0f;
	}
}

RecordedValue& TrackedHands::GetJointHistory(size_t handIndex, HandJointIndex jointIndex)
{
	if (handIndex < HAND_COUNT && (size_t)jointIndex < kHandJointCount)
	{
		return m_handJoints[handIndex * kHandJointCount + (size_t)jointIndex];
	}
	else
	{
		static RecordedValue blankRecordedValue;
		return blankRecordedValue;
	}
}

XMVECTOR TrackedHands::GetSmoothedJoint(size_t handIndex, HandJointIndex jointIndex)
{
	if (handIndex < HAND_COUNT && (size_t)jointIndex < kHandJointCount)
	{
		return m_handJoints[handIndex * kHandJointCount + (size_t)jointIndex].GetSmoothedValue(0);
	}
	else
	{
		return XMVectorZero();
	}
}

XMVECTOR TrackedHands::GetSmoothedPalmDirection(size_t handIndex)
{
	if (IsHandTracked(handIndex) && handIndex < HAND_COUNT)
		return CalculatePalmDirection(handIndex, GetSmoothedJoint(handIndex, HandJointIndex::Wrist), GetSmoothedJoint(handIndex, HandJointIndex::IndexProximal), GetSmoothedJoint(handIndex, HandJointIndex::PinkyProximal));
	else
		return XMVectorZero();
}

XMVECTOR TrackedHands::CalculatePointingDirection(XMVECTOR wrist, XMVECTOR indexBase, XMVECTOR pinkyBase)
{
	XMVECTOR centerKnuckle = (indexBase + pinkyBase) / 2.0f;
	return XMVector3Normalize(centerKnuckle - wrist);
}

XMVECTOR TrackedHands::CalculatePalmDirection(size_t handIndex, XMVECTOR wrist, XMVECTOR indexBase, XMVECTOR pinkyBase)
{
	XMVECTOR wristToPinkyDir = XMVector3Normalize(pinkyBase - wrist);
	XMVECTOR wristToIndexDir = XMVector3Normalize(indexBase - wrist);

	if (handIndex == 0)	// Left hand
		return XMVector3Normalize(XMVector3Cross(wristToPinkyDir, wristToIndexDir));
	else				// Right hand
		return XMVector3Normalize(XMVector3Cross(wristToIndexDir, wristToPinkyDir));
}

void TrackedHands::UpdateFromMixedReality(MixedReality& mixedReality)
{
	m_isNewFrameAvailable = false;

	for (size_t handIndex = 0; handIndex < HAND_COUNT; ++handIndex)
	{
		const auto pHandData = mixedReality.GetHand(handIndex);

		if (pHandData)
		{
			m_handTrackedStates[handIndex] = true;

			if (pHandData->lastTimestamp > m_timestamps[handIndex])
			{
				m_timestamps[handIndex] = pHandData->lastTimestamp;
				m_isNewFrameAvailable = true;
			}
			else
			{
				continue;
			}
		}
		else
		{
			m_handTrackedStates[handIndex] = false;
			//ResetHand(handIndex);
			continue;
		}

		for (size_t jointIndex = 0; jointIndex < kHandJointCount; ++jointIndex)
		{
			size_t finalIndex = handIndex * kHandJointCount + jointIndex;
			XMVECTOR worldPosition = pHandData->handJoints[jointIndex].position;

			m_handJoints[finalIndex].RecordValue(worldPosition);
			XMVECTOR worldOrientation = pHandData->handJoints[jointIndex].orientation;
			m_handOrientations[finalIndex].RecordValue(worldOrientation);
			m_handRadii[finalIndex] = pHandData->handJoints[jointIndex].radius;			
		}		
	}

	m_headPosition = mixedReality.GetHeadPosition();
	m_headForward = mixedReality.GetHeadForwardDirection();
	m_headUp = mixedReality.GetHeadUpDirection();

	XMMATRIX worldTransform = XMMatrixLookToRH(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), m_headForward, m_headUp);
	m_headTransform = XMMatrixMultiply(XMMatrixTranspose(worldTransform), XMMatrixTranslationFromVector(m_headPosition));
}