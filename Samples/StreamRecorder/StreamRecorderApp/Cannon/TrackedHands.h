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

#include "MixedReality.h"
#include "RecordedValue.h"

#define HAND_COUNT 2

constexpr size_t kHandJointCount = (size_t)HandJointIndex::Count;

class TrackedHands
{
public:	
	TrackedHands();
	void ResetHand(size_t handIndex);

	void UpdateFromMixedReality(MixedReality& mixedReality);
	const XMMATRIX& GetHeadTransform() { return m_headTransform; }
	bool IsHandTracked(size_t handIndex);
	
	XMVECTOR GetJoint(size_t handIndex, HandJointIndex jointIndex);
	XMVECTOR GetJointOrientation(size_t handIndex, HandJointIndex jointIndex);	// XMVECTOR is a quaternion in this case
	XMMATRIX GetOrientedJoint(size_t handIndex, HandJointIndex jointIndex);
	XMVECTOR GetIndexTipSurfacePosition(size_t handIndex, int framesAgo = 0);
	float GetJointRadius(size_t handIndex, HandJointIndex jointIndex);
	RecordedValue& GetJointHistory(size_t handIndex, HandJointIndex jointIndex);

	XMVECTOR GetSmoothedJoint(size_t handIndex, HandJointIndex jointIndex);
	XMVECTOR GetSmoothedPalmDirection(size_t handIndex);
	static XMVECTOR CalculatePointingDirection(XMVECTOR wrist, XMVECTOR indexBase, XMVECTOR pinkyBase); //Direction from wrist through middle knuckle area
	static XMVECTOR CalculatePalmDirection(size_t handIndex, XMVECTOR wrist, XMVECTOR indexBase, XMVECTOR pinkyBase); // Normal to palm of hand

	XMVECTOR GetHeadPosition() { return m_headPosition; }
	XMVECTOR GetHeadForward() { return m_headForward; }
	XMVECTOR GetHeadUp() { return m_headUp; }
	
private:
	long long m_timestamps[HAND_COUNT];
	bool m_isNewFrameAvailable;

	// Head pose at last hand tracking frame
	XMVECTOR m_headPosition;
	XMVECTOR m_headForward;
	XMVECTOR m_headUp;
	XMMATRIX m_headTransform;

	bool m_handTrackedStates[HAND_COUNT];	
	RecordedValue m_handJoints[HAND_COUNT * kHandJointCount];
	RecordedValue m_handOrientations[HAND_COUNT * kHandJointCount];
	float m_handRadii[HAND_COUNT * kHandJointCount];
};
