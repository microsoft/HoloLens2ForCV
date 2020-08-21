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
using namespace DirectX;

class AnimatedVector
{
public:

	AnimatedVector();

	void StartPointToPoint(const XMVECTOR& startingVector, const XMVECTOR& targetVector, const float animationLengthInSeconds);
	void StartPingPong(const XMVECTOR& vectorA, const XMVECTOR& vectorB, const float animationLengthInSeconds);
	void Update(float timeDeltaInSeconds);

	void SetTargetVector(const XMVECTOR& targetVector);
	
	const XMVECTOR& GetCurrentVector();
	bool IsDone();

private:

	enum class AnimationMode
	{
		PointToPoint,
		PingPong
	};

	AnimationMode m_animationMode;

	XMVECTOR m_startingVector;
	XMVECTOR m_currentVector;
	XMVECTOR m_targetVector;

	float m_speed;
	bool m_animating;
};

class InterpolatedTransform
{
public:

	InterpolatedTransform();

	XMVECTOR GetStartingPosition();
	XMVECTOR GetStartingRotation();
	XMVECTOR GetStartingScale();

	void SetStartingPosition(const XMVECTOR& startingPosition);
	void SetTargetPosition(const XMVECTOR& targetPosition);

	void SetStartingRotation(const XMVECTOR& startingRotation);
	void SetTargetRotation(const XMVECTOR& targetRotation);

	void SetStartingScale(const XMVECTOR& startingScale);
	void SetTargetScale(const XMVECTOR& targetScale);

	XMVECTOR CalculateTranslation(float t);
	XMVECTOR CalculateRotation(float t);
	XMVECTOR CalculateScale(float t);
	XMMATRIX CalculateMatrix(float t);

private:

	XMVECTOR m_startingPosition;
	XMVECTOR m_targetPosition;

	XMVECTOR m_startingRotation;
	XMVECTOR m_targetRotation;

	XMVECTOR m_startingScale;
	XMVECTOR m_targetScale;
};