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


#include "AnimatedVector.h"

AnimatedVector::AnimatedVector() :
	m_animationMode(AnimationMode::PointToPoint),
	m_startingVector(XMVectorZero()),
	m_currentVector(XMVectorZero()),
	m_targetVector(XMVectorZero()),
	m_speed(0),
	m_animating(false)
{

}

void AnimatedVector::StartPointToPoint(const XMVECTOR& startingVector, const XMVECTOR& targetVector, const float animationLengthInSeconds)
{
	m_animationMode = AnimationMode::PointToPoint;

	m_startingVector = m_currentVector = startingVector;
	m_targetVector = targetVector;
	m_speed = XMVectorGetX(XMVector3Length(m_targetVector - m_startingVector)) / animationLengthInSeconds;
	m_animating = true;
}

void AnimatedVector::StartPingPong(const XMVECTOR& vectorA, const XMVECTOR& vectorB, const float animationLengthInSeconds)
{
	m_animationMode = AnimationMode::PingPong;

	m_startingVector = m_currentVector = vectorA;
	m_targetVector = vectorB;
	m_speed = XMVectorGetX(XMVector3Length(m_targetVector - m_startingVector)) / animationLengthInSeconds;
	m_animating = true;
}

void AnimatedVector::Update(float timeDeltaInSeconds)
{
	XMVECTOR vectorToTarget = m_targetVector - m_currentVector;
	float remainingDistance = XMVectorGetX(XMVector3Length(vectorToTarget));
	XMVECTOR directionToTarget = XMVector3Normalize(vectorToTarget);
	float currentStepSize = m_speed * timeDeltaInSeconds;

	if (m_animationMode == AnimationMode::PointToPoint)
	{
		if (remainingDistance > currentStepSize)
		{
			m_currentVector += directionToTarget * currentStepSize;
		}
		else
		{
			m_currentVector = m_targetVector;
			m_animating = false;
		}
	}
	else if (m_animationMode == AnimationMode::PingPong)
	{
		if (remainingDistance > currentStepSize)
		{
			m_currentVector += directionToTarget * currentStepSize;
		}
		else
		{
			m_currentVector = m_targetVector;
			m_targetVector = m_startingVector;
			m_startingVector = m_currentVector;
		}
	}
}

void AnimatedVector::SetTargetVector(const XMVECTOR& targetVector)
{
	m_targetVector = targetVector;
}

const XMVECTOR& AnimatedVector::GetCurrentVector()
{
	return m_currentVector;
}

bool AnimatedVector::IsDone()
{
	return !m_animating;
}

InterpolatedTransform::InterpolatedTransform() :
	m_startingPosition(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)),
	m_targetPosition(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)),
	m_startingRotation(XMQuaternionIdentity()),
	m_targetRotation(XMQuaternionIdentity()),
	m_startingScale(XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f)),
	m_targetScale(XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f))
{

}

XMVECTOR InterpolatedTransform::GetStartingPosition()
{
	return m_startingPosition;
}

XMVECTOR InterpolatedTransform::GetStartingRotation()
{
	return m_startingRotation;
}

XMVECTOR InterpolatedTransform::GetStartingScale()
{
	return m_startingScale;
}

void InterpolatedTransform::SetStartingPosition(const XMVECTOR& startingPosition)
{
	m_startingPosition = startingPosition;
}

void InterpolatedTransform::SetTargetPosition(const XMVECTOR& targetPosition)
{
	m_targetPosition = targetPosition;
}

void InterpolatedTransform::SetStartingRotation(const XMVECTOR& startingRotation)
{
	m_startingRotation = startingRotation;
}

void InterpolatedTransform::SetTargetRotation(const XMVECTOR& targetRotation)
{
	m_targetRotation = targetRotation;
}

void InterpolatedTransform::SetStartingScale(const XMVECTOR& startingScale)
{
	m_startingScale = startingScale;
}

void InterpolatedTransform::SetTargetScale(const XMVECTOR& targetScale)
{
	m_targetScale = targetScale;
}

XMVECTOR InterpolatedTransform::CalculateTranslation(float t)
{
	return m_startingPosition + t * (m_targetPosition - m_startingPosition);
}

XMVECTOR InterpolatedTransform::CalculateRotation(float t)
{
	return XMQuaternionSlerp(m_startingRotation, m_targetRotation, t);
}

XMVECTOR InterpolatedTransform::CalculateScale(float t)
{
	return m_startingScale + t * (m_targetScale - m_startingScale);
}

XMMATRIX InterpolatedTransform::CalculateMatrix(float t)
{
	XMVECTOR translation = CalculateTranslation(t);
	XMVECTOR rotation = CalculateRotation(t);
	XMVECTOR scale = CalculateScale(t);

	return XMMatrixScalingFromVector(scale) * XMMatrixRotationQuaternion(rotation) * XMMatrixTranslationFromVector(translation);
}
