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


#include "FloatingSlate.h"

#include <memory>
using namespace std;

float FloatingSlateButton::m_defaultFontSize = 36.0f;
void FloatingSlateButton::SetDefaultFontSize(const float fontSize)
{
	m_defaultFontSize = fontSize;
}

FloatingSlateButton::FloatingSlateButton(const XMVECTOR& position, const XMVECTOR& size, const XMVECTOR& color, const unsigned id, IFloatingSlateButtonCallback* pCallback, shared_ptr<Texture2D> texture) :
	m_position(position),
	m_size(size),
	m_pushDistance(0),
	m_activePushingHandIndex(-1),
	m_buttonSpringSpeed(0.10f),
	m_color(color),
	m_highlightColor(m_color + XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f) * 0.25f),
	m_autoHighlightColor(true),
	m_id(id),
	m_buttonShape(Mesh::MT_ROUNDEDBOX),
	m_fontSize(m_defaultFontSize),
	m_drawCall("Lit_VS.cso", "LitTextureColorBlend_PS.cso", Mesh::MT_EMPTY),
	m_outlineDrawCall("Lit_VS.cso", "Lit_PS.cso", Mesh::MT_EMPTY),
	m_texture(texture),
	m_geometryNeedsUpdate(true),
	m_textureNeedsUpdate(true),
	m_hidden(false),
	m_on(false),
	m_disabled(false),
	m_stayHighlighted(false),
	m_pushedIn(false),
	m_outlineWhenHighlighted(false),
	m_spacer(false),
	m_highlightActive(false)
{
	// If a texture is specified at creation time, don't try to update it
	if (m_texture)
		m_textureNeedsUpdate = false;

	if (pCallback)
		RegisterCallback(pCallback);
}

FloatingSlateButton::FloatingSlateButton(const XMVECTOR& position, const XMVECTOR& size, const XMVECTOR& color, const unsigned id, IFloatingSlateButtonCallback* pCallback, const string& buttonText) :
	FloatingSlateButton(position, size, color, id, pCallback, shared_ptr<Texture2D>(nullptr))
{
	SetText(buttonText);
}

FloatingSlateButton::FloatingSlateButton(const XMVECTOR& position, const XMVECTOR& size, const XMVECTOR& color, const unsigned id, IFloatingSlateButtonCallback* pCallback) :
	FloatingSlateButton(position, size, color, id, pCallback, shared_ptr<Texture2D>(nullptr))
{
}

FloatingSlateButton::FloatingSlateButton(const unsigned id, IFloatingSlateButtonCallback * pCallback, std::shared_ptr<Texture2D> texture) :
	FloatingSlateButton(XMVectorZero(), XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), id, pCallback, texture)
{
}

FloatingSlateButton::FloatingSlateButton(const unsigned id, IFloatingSlateButtonCallback* pCallback, const std::string& buttonText) :
	FloatingSlateButton(XMVectorZero(), XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), id, pCallback, buttonText)
{
}


FloatingSlateButton::FloatingSlateButton(const unsigned id, IFloatingSlateButtonCallback * pCallback) :
	FloatingSlateButton(XMVectorZero(), XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), id, pCallback, std::shared_ptr<Texture2D>(nullptr))
{
}

FloatingSlateButton::FloatingSlateButton() :
	FloatingSlateButton(0, nullptr)
{
}

void FloatingSlateButton::SetShape(Mesh::MeshType meshType)
{
	m_buttonShape = meshType;

	m_geometryNeedsUpdate = true;
	m_textureNeedsUpdate = true;
}

XMVECTOR FloatingSlateButton::GetPosition()
{
	return m_position;
}

void FloatingSlateButton::SetPosition(const XMVECTOR& position)
{
	m_position = position;
}

XMVECTOR FloatingSlateButton::GetSize()
{
	return m_size;
}

void FloatingSlateButton::SetSize(const XMVECTOR& size)
{
	m_size = size;
	m_geometryNeedsUpdate = true;
}

void FloatingSlateButton::Update(float timeDeltaInSeconds, XMMATRIX& parentTransform, TrackedHands& hands, float offsetToSlateSurface, bool suspendInteractions)
{
	if (m_hidden)
		return;

	if (m_geometryNeedsUpdate)
		UpdateGeometry();

	// Build a world-space push volume that contains all possible movement of the button and a buffer area behind it
	XMVECTOR unPushedRelativePosition = m_position + XMVectorSet(0.0f, 0.0f, offsetToSlateSurface + XMVectorGetZ(m_size) / 2.0f, 0.0f);
	XMMATRIX unPushedRelativeTransform = XMMatrixTranslationFromVector(unPushedRelativePosition);
	
	float maxPushDistance = XMVectorGetZ(m_size) - XMVectorGetZ(m_size) * 0.1f;
	XMVECTOR maxPushedRelativePosition = unPushedRelativePosition - XMVectorSet(0.0f, 0.0f, maxPushDistance * 5.0f, 0.0f);
	XMMATRIX maxPushedRelativeTransform = XMMatrixTranslationFromVector(maxPushedRelativePosition);

	BoundingBox localBounds = m_drawCall.GetMesh()->GetBoundingBox();
	BoundingBox unPushedRelativeBounds;
	localBounds.Transform(unPushedRelativeBounds, unPushedRelativeTransform);
	BoundingBox maxPushedRelativeBounds;
	localBounds.Transform(maxPushedRelativeBounds, maxPushedRelativeTransform);
	BoundingBox relativePushVolume;
	BoundingBox::CreateMerged(relativePushVolume, unPushedRelativeBounds, maxPushedRelativeBounds);

	BoundingOrientedBox worldPushVolume;
	BoundingOrientedBox::CreateFromBoundingBox(worldPushVolume, relativePushVolume);
	worldPushVolume.Transform(worldPushVolume, parentTransform);

	// Calculate the world position that corresponds to the center of the front surface of the button
	XMVECTOR zeroPushPosition = XMVectorSetZ(unPushedRelativePosition, XMVectorGetZ(unPushedRelativePosition) + XMVectorGetZ(m_size) / 2.0f);
	zeroPushPosition = XMVector3TransformCoord(zeroPushPosition, parentTransform);
	XMVECTOR pushDirection = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), parentTransform));

	// Check if a finger broke the front plane of the button this frame
	if (m_activePushingHandIndex == -1)
	{
		for (size_t handIndex = 0; handIndex < HAND_COUNT; ++handIndex)
		{
			if (hands.IsHandTracked(handIndex))
			{
				XMVECTOR indexTip = hands.GetIndexTipSurfacePosition(handIndex);
				if (worldPushVolume.Contains(indexTip) == CONTAINS)
				{					
					XMVECTOR lastIndexTip = hands.GetIndexTipSurfacePosition(handIndex, 1);
					float lastFingerPushDistance = XMVectorGetX(XMVector3Dot(lastIndexTip - zeroPushPosition, pushDirection));

					if (lastFingerPushDistance <= 0.0f)
					{
						m_activePushingHandIndex = (int) handIndex;
						break;
					}
				}
			}
		}
	}
	
	// Calculate push distance if button is being actively pressed and index tip is still in push volume
	float fingerPushDistance = 0.0f;
	if(m_activePushingHandIndex != -1 && !suspendInteractions && !m_disabled && !m_pushedIn && !m_spacer)
	{
		XMVECTOR indexTip = hands.GetIndexTipSurfacePosition(m_activePushingHandIndex);
		if (hands.IsHandTracked(m_activePushingHandIndex) && worldPushVolume.Contains(indexTip) == CONTAINS)
		{
			fingerPushDistance = XMVectorGetX(XMVector3Dot(indexTip - zeroPushPosition, pushDirection));
		}
		else
		{
			m_activePushingHandIndex = -1;
		}
	}

	// Update button state based on finger push distance
	if (fingerPushDistance < m_pushDistance)
	{
		m_pushDistance -= m_buttonSpringSpeed * timeDeltaInSeconds;
		m_pushDistance = max(m_pushDistance, 0.0f);
		
		if (m_pushDistance == 0.0f)
		{
			m_on = false;
		}
	}
	else if (fingerPushDistance < maxPushDistance)
	{
		m_pushDistance = fingerPushDistance;
	}
	else if (fingerPushDistance >= maxPushDistance)
	{
		m_pushDistance = maxPushDistance;

		if (!m_on)
		{
			for (auto callback : m_registeredCallbacks)
				callback->OnButtonPressed(this);

			m_on = true;
		}
	}

	if(m_pushedIn)
		m_pushDistance = maxPushDistance;

	bool lastHighlightActiveState = m_highlightActive;
	m_highlightActive = m_on || m_stayHighlighted || m_pushedIn;

	// Set final color for rendering
	if (m_highlightActive)
		m_drawCall.SetColor(m_highlightColor);
	else
		m_drawCall.SetColor(m_color);

	// Set final transform for rendering
	XMMATRIX relativeTransform = XMMatrixTranslationFromVector(unPushedRelativePosition + XMVectorSet(0.0f, 0.0f, -m_pushDistance, 0.0f));
	XMMATRIX worldTransform = relativeTransform * parentTransform;
	m_drawCall.SetWorldTransform(worldTransform);

	XMMATRIX outlineRelativeTransform = XMMatrixTranslationFromVector(unPushedRelativePosition + XMVectorSet(0.0f, 0.0f, -m_pushDistance + XMVectorGetZ(m_size) / 2.0f + 0.0001f, 0.0f));
	m_outlineDrawCall.SetWorldTransform(outlineRelativeTransform * parentTransform);
	m_outlineDrawCall.SetColor(XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f));
}

bool FloatingSlateButton::TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, float& distance)
{
	return m_drawCall.TestRayIntersection(rayOriginInWorldSpace, rayDirectionInWorldSpace, distance);
}

void FloatingSlateButton::UpdateGeometry()
{
	Mesh::DiscMode discMode = Mesh::DiscMode::Circle;
	unsigned segmentCount = 32;

	if (m_buttonShape == Mesh::MT_BOX)
	{
		m_drawCall.GetMesh()->LoadBox(XMVectorGetX(m_size), XMVectorGetZ(m_size), XMVectorGetY(m_size));

		auto& vertices = m_drawCall.GetMesh()->GetVertices();
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			// Kill all texcoords on the box except the front face so that texture will only appear on the front of the button
			if (i < 6 * 4 || i >= 6 * 5)
			{
				vertices[i].texcoord.x = 0.0f;
				vertices[i].texcoord.y = 0.0f;
			}
		}

		discMode = Mesh::DiscMode::Square;
		segmentCount = 4;
	}
	else if (m_buttonShape == Mesh::MT_ROUNDEDBOX)
	{
		m_drawCall.GetMesh()->LoadRoundedBox(XMVectorGetX(m_size), XMVectorGetZ(m_size), XMVectorGetY(m_size));
		discMode = Mesh::DiscMode::RoundedSquare;
	}
	else if (m_buttonShape == Mesh::MT_CYLINDER)
	{
		m_drawCall.GetMesh()->LoadCylinder(XMVectorGetX(m_size) / 2.0f, XMVectorGetZ(m_size));
		discMode = Mesh::DiscMode::Circle;
	}

	// Create the outline mesh for this button
	auto outlineMesh = m_outlineDrawCall.GetMesh();
	outlineMesh->GetVertices().clear();
	outlineMesh->GetIndices().clear();
	XMVECTOR upDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR rightDir = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	float rightRadius = XMVectorGetX(m_size) / 2.0f;
	float backRadius = XMVectorGetY(m_size) / 2.0f;
	vector<Mesh::Disc> discs;
	discs.push_back({ XMVectorSet(0.0f, 0.00f, 0.0f, 1.0f), upDir, rightDir, rightRadius + 0.001f, backRadius + 0.001f });
	discs.push_back({ XMVectorSet(0.0f, 0.00f, 0.0f, 1.0f), upDir, rightDir, rightRadius + 0.0f, backRadius + 0.0f });
	outlineMesh->AppendGeometryForDiscs(discs, segmentCount, discMode);
	outlineMesh->GenerateSmoothNormals();

	// Rotate the button such that +Y is facing forward
	XMMATRIX rotationToYFront = XMMatrixRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XM_PIDIV2);
	m_drawCall.GetMesh()->BakeTransform(rotationToYFront);
	m_outlineDrawCall.GetMesh()->BakeTransform(rotationToYFront);

	m_geometryNeedsUpdate = false;
}

void FloatingSlateButton::UpdateTexture()
{
	if (!m_texture)
	{
		const float dpi = 96.0f * 2.0f;
		float dpm = dpi * 0.393f * 100.0f;
		unsigned requiredWidth = (unsigned)(dpm * XMVectorGetX(m_size));
		unsigned requiredHeight = (unsigned)(dpm * XMVectorGetY(m_size));

		// Snap to nearest multiple of 2
		const unsigned multipleOf = 2;
		requiredWidth = (requiredWidth / multipleOf) * multipleOf;
		requiredHeight = (requiredHeight / multipleOf) * multipleOf;

		m_texture = make_shared<Texture2D>(requiredWidth, requiredHeight);
	}

	if (m_texture->IsRenderTarget())
	{
		DrawCall::PushRenderPass(0, m_texture);
		m_texture->Clear(0.0f, 0.0f, 0.0f, 0.0f);

		D2D_RECT_F layoutRect{ 0.0f, 0.0f, (float)m_texture->GetWidth(), (float)m_texture->GetHeight() };

		TextColor textColor = !m_disabled ? TextColor::White : TextColor::Gray;
		DrawCall::DrawText(m_buttonText, layoutRect, m_fontSize, HorizontalAlignment::Center, VerticalAlignment::Middle, textColor);
		
		DrawCall::PopRenderPass();
	}

	m_textureNeedsUpdate = false;
}

void FloatingSlateButton::Draw()
{
	if (!m_hidden)
	{
		if (!m_texture || m_textureNeedsUpdate)
			UpdateTexture();

		m_texture->BindAsPixelShaderResource(0);
		m_drawCall.Draw();

		if (m_outlineWhenHighlighted && m_highlightActive)
			m_outlineDrawCall.Draw();
	}
}

bool FloatingSlateButton::IsOn()
{
	return m_on;
}

XMVECTOR FloatingSlateButton::GetColor()
{
	return m_color;
}

void FloatingSlateButton::SetColor(const XMVECTOR & color)
{
	m_color = color;

	if (m_autoHighlightColor)
		m_highlightColor = m_color + XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f) * 0.25f;
}

void FloatingSlateButton::SetHighlightColor(const XMVECTOR& color)
{
	m_highlightColor = color;
	m_autoHighlightColor = false;
}

void FloatingSlateButton::SetText(const std::string& buttonText)
{
	if (buttonText != m_buttonText)
	{
		m_buttonText = buttonText;
		m_textureNeedsUpdate = true;
	}
}

void FloatingSlateButton::SetFontSize(const float fontSize)
{
	m_fontSize = fontSize;
	m_textureNeedsUpdate = true;
}

bool FloatingSlateButton::IsDisabled()
{
	return m_disabled;
}

void FloatingSlateButton::SetDisabled(bool disabled)
{
	if (disabled != m_disabled)
	{
		m_disabled = disabled;

		if (!m_buttonText.empty())
			m_textureNeedsUpdate = true;
	}
}

bool FloatingSlateButton::IsButtonHighlighted()
{
	return m_stayHighlighted;
}

void FloatingSlateButton::SetButtonStayHighlightedState(bool stayHighlighted)
{
	m_stayHighlighted = stayHighlighted;

	if (m_stayHighlighted)
	{
		for (auto button : m_mutuallyExclusiveButtons)
			button->SetButtonStayHighlightedState(false);
	}
}

bool FloatingSlateButton::IsButtonPushedIn()
{
	return m_pushedIn;
}

void FloatingSlateButton::SetButtonPushedInState(bool pushedIn)
{
	m_pushedIn = pushedIn;

	if (m_pushedIn)
	{
		for (auto button : m_mutuallyExclusiveButtons)
			button->SetButtonPushedInState(false);
	}
}

void FloatingSlateButton::SetOutlineWhenHighlighted(bool outlineWhenHighlighted)
{
	m_outlineWhenHighlighted = outlineWhenHighlighted;
}

void FloatingSlateButton::SetSpacer(bool spacer)
{
	m_spacer = spacer;
}

unsigned FloatingSlateButton::GetID()
{
	return m_id;
}

const std::string& FloatingSlateButton::GetText()
{
	return m_buttonText;
}

bool FloatingSlateButton::IsHidden()
{
	return m_hidden;
}

void FloatingSlateButton::SetHidden(bool hidden)
{
	m_hidden = hidden;
}

void FloatingSlateButton::RegisterCallback(IFloatingSlateButtonCallback* pCallback)
{
	if (pCallback)
		m_registeredCallbacks.push_back(pCallback);
}

void FloatingSlateButton::RegisterMutuallyExclusiveButton(std::shared_ptr<FloatingSlateButton> button)
{
	if (button && button.get() != this)
		m_mutuallyExclusiveButtons.push_back(button);
}

void FloatingSlate::SetTitleBarHeight(float titleBarHeight)
{
	m_titleBarHeight = titleBarHeight;
}

void FloatingSlate::SetDefaultColor(const DirectX::XMVECTOR& color)
{
	m_defaultColor = color;
}

void FloatingSlate::SetDefaultTitleBarColor(const DirectX::XMVECTOR& color)
{
	m_defaultTitleBarColor = color;
}

float FloatingSlate::m_titleBarHeight = 0.03f;
XMVECTOR FloatingSlate::m_defaultColor = XMVectorSet(0.50f, 0.50f, 0.50f, 1.0f);
XMVECTOR FloatingSlate::m_defaultTitleBarColor = XMVectorSet(0.30f, 0.30f, 0.30f, 1.0f);

FloatingSlate::FloatingSlate() :
	FloatingSlate(XMVectorSet(0.15f, 0.20f, 0.02f, 0.0f))
{


}

FloatingSlate::FloatingSlate(const XMVECTOR& size) :
	m_size(size),
	m_minScale(XMVectorSet(0.02f, 0.02f, 0.02f, 0.0f)),
	m_maxScale(XMVectorSet(1.00f, 1.00f, 1.00f, 0.0f)),
	m_horizontalPivot(SlateHorizontalPivot::Center),
	m_verticalPivot(SlateVerticalPivot::Center),
	m_lerpAmount(1.0f),
	m_hideOnAnimationComplete(false),
	m_color(m_defaultColor),
	m_titleBarColor(m_defaultTitleBarColor),
	m_titleBarFlowDirection(1.0f),
	m_baseSlateDrawCall("Lit_VS.cso", "Lit_PS.cso", make_shared<Mesh>(Mesh::MT_BOX)),
	m_titleBarDrawCall("Lit_VS.cso", "Lit_PS.cso", make_shared<Mesh>(Mesh::MT_BOX)),
	m_titleBarSpacerButton(make_shared<FloatingSlateButton>())
{
	m_titleBarSpacerButton->SetSpacer(true);

	SetRotation(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}

void FloatingSlate::CalculateFollowHandPositionAndRotation(TrackedHands& hands, XMVECTOR& targetPosition, XMVECTOR& targetRotation)
{
	XMVECTOR headPosition = hands.GetHeadPosition();
	XMVECTOR headForward = hands.GetHeadForward();
	XMVECTOR headUp = hands.GetHeadUp();
	XMVECTOR headRight = XMVector3Cross(headForward, headUp);

	XMVECTOR targetForwardDirection = XMVectorZero();
	XMVECTOR targetUpDirection = XMVectorZero();

	XMVECTOR scale = m_interpolatedTransform.CalculateScale(m_lerpAmount);

	switch (m_followHand)
	{
	case FollowHandMode::LeftIndex: // get the slate flying on the left of the left hand index 
	{
		targetPosition = hands.GetJoint(0, HandJointIndex::IndexTip) + headRight * scale * 0.045f;
		targetForwardDirection = XMVector3Normalize(headPosition - targetPosition);
		targetUpDirection = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		break;
	}
	case FollowHandMode::LeftPalm:// get the slate flying on the slighlty left of the mid point between the left hand middle  and ring finger's intermediate joins(using two fingers to reduc jitter)
	{
		targetPosition = 0.5 * (hands.GetJoint(0, HandJointIndex::MiddleIntermediate) + hands.GetJoint(0, HandJointIndex::RingIntermediate)) - headForward * 0.025f;
		targetForwardDirection = XMVector3Normalize(headPosition - targetPosition);
		targetUpDirection = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		break;
	}
	case FollowHandMode::RightIndex: // get the slate flying on the right of the mid point between the right hand index and thumb tips(using two fingers to reduce jitter)
	{
		targetPosition = hands.GetJoint(1, HandJointIndex::IndexTip) - headRight * scale * 0.045f;
		targetForwardDirection = XMVector3Normalize(headPosition - targetPosition);
		targetUpDirection = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		break;
	}
	case FollowHandMode::RightPalm:// get the slate flying on the slighlty right of the mid point between the right hand middle  and ring finger's intermediate joins(using two fingers to reduc jitter)
	{
		targetPosition = 0.5 * (hands.GetJoint(1, HandJointIndex::MiddleIntermediate) + hands.GetJoint(1, HandJointIndex::RingIntermediate)) - headForward * 0.025f;
		targetForwardDirection = XMVector3Normalize(headPosition - targetPosition);
		targetUpDirection = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		break;
	}
	case FollowHandMode::LeftInnerEdge:
	case FollowHandMode::RightInnerEdge:
	{
		size_t handIndex = (m_followHand == FollowHandMode::LeftInnerEdge) ? 0 : 1;

		XMVECTOR ringProximal = hands.GetSmoothedJoint(handIndex, HandJointIndex::RingProximal);
		XMVECTOR pinkyProximal = hands.GetSmoothedJoint(handIndex, HandJointIndex::PinkyProximal);
		targetPosition = pinkyProximal + 2.5f * (pinkyProximal - ringProximal);

		XMVECTOR middleProximal = hands.GetSmoothedJoint(handIndex, HandJointIndex::MiddleProximal);
		float middleProximalRadius = hands.GetJointRadius(handIndex, HandJointIndex::MiddleMetacarpal);
		XMVECTOR middleProximalSurface = middleProximal + hands.GetSmoothedPalmDirection(handIndex) * middleProximalRadius;
		XMVECTOR wrist = hands.GetSmoothedJoint(handIndex, HandJointIndex::Wrist);

		float menuWidth = XMVectorGetX(GetFullHierarchySize());
		XMVECTOR centerOfMenu = targetPosition + menuWidth / 2.0f * XMVector3Normalize(pinkyProximal - ringProximal);
		targetForwardDirection = XMVector3Normalize(headPosition - centerOfMenu);
		targetUpDirection = hands.CalculatePointingDirection(wrist, middleProximalSurface, middleProximalSurface);
		XMVECTOR targetRightDirection = XMVector3Normalize(XMVector3Cross(targetUpDirection, targetForwardDirection));
		targetForwardDirection = XMVector3Normalize(XMVector3Cross(targetRightDirection, targetUpDirection));

		break;
	}
	default:
	{
		break;
	}
	}

	targetRotation = CalculateQuaternionFromBasisVectors(targetForwardDirection, targetUpDirection);
}

void FloatingSlate::Update(float timeDeltaInSeconds, TrackedHands& hands)
{
	Update(timeDeltaInSeconds, XMMatrixIdentity(), hands);
}

void FloatingSlate::Update(float timeDeltaInSeconds, const XMMATRIX& parentTransform, TrackedHands& hands, bool suspendInteractions)
{
	if (m_hidden)
		return;

	if (m_followHand != FollowHandMode::None)
	{
		XMVECTOR targetPosition = XMVectorZero();
		XMVECTOR targetRotation = XMVectorZero();
		CalculateFollowHandPositionAndRotation(hands, targetPosition, targetRotation);

		m_interpolatedTransform.SetTargetPosition(targetPosition);
		m_interpolatedTransform.SetTargetRotation(targetRotation);
	}

	if (IsAnimating())
	{
		const float animationDuration = 0.25f;
		if (m_animationState == AnimationState::PreTimer)
		{
			if (m_animationTimer.GetTime() >= m_animationBuffer)
			{
				m_animationState = AnimationState::Lerping;
			}
		}
		else if (m_animationState == AnimationState::Lerping)
		{
			float lerpSpeed = 1.0f / animationDuration;
			m_lerpAmount += lerpSpeed * timeDeltaInSeconds;
			m_lerpAmount = min(1.0f, m_lerpAmount);

			if (m_lerpAmount == 1.0f)
			{
				m_animationState = AnimationState::PostTimer;
				m_animationTimer.Reset();
			}
		}
		else if (m_animationState == AnimationState::PostTimer)
		{
			if (m_animationTimer.GetTime() >= m_animationBuffer)
			{
				m_animationState = AnimationState::Complete;
				if (m_hideOnAnimationComplete)
				{
					Hide();
				}
			}
		}
	}

	float pivotX = 0.0f;
	if (m_horizontalPivot == SlateHorizontalPivot::Left)
		pivotX = 0.5f * XMVectorGetX(m_size);
	else if (m_horizontalPivot == SlateHorizontalPivot::Right)
		pivotX = -0.5f * XMVectorGetX(m_size);

	float pivotY = 0.0f;
	if (m_verticalPivot == SlateVerticalPivot::Bottom)
		pivotY = 0.5f * XMVectorGetY(m_size);
	else if (m_verticalPivot == SlateVerticalPivot::Top)
		pivotY = -0.5f * XMVectorGetY(m_size);

	XMMATRIX pivotTransform = XMMatrixTranslation(pivotX, pivotY, 0.0f);
	XMMATRIX worldTransform = pivotTransform * m_interpolatedTransform.CalculateMatrix(m_lerpAmount) * parentTransform;
	XMMATRIX localTransform = XMMatrixScaling(XMVectorGetX(m_size), XMVectorGetY(m_size), XMVectorGetZ(m_size));

	m_baseSlateDrawCall.SetColor(m_color);
	m_baseSlateDrawCall.SetWorldTransform(localTransform * worldTransform);

	if (m_buttonLayoutNeedsUpdate)
	{
		ReflowButtons();
		ReflowTitleBarButtons();
		m_buttonLayoutNeedsUpdate = false;
	};

	// Clear the active modal child slate pointer if the child window has been closed
	if (m_activeModalChildSlate && m_activeModalChildSlate->IsHidden())
		m_activeModalChildSlate.reset();

	suspendInteractions = suspendInteractions || IsAnimating() || m_activeModalChildSlate;

	if (!m_titleBarHidden)
	{
		m_titleBarDrawCall.SetColor(m_titleBarColor);

		XMMATRIX titleBarTranslation = XMMatrixTranslation(0.0f, XMVectorGetY(m_size) / 2.0f + m_titleBarHeight / 2.0f, -XMVectorGetZ(m_size) / 4.0f);
		XMMATRIX titleBarScale = XMMatrixScaling(XMVectorGetX(m_size), m_titleBarHeight, XMVectorGetZ(m_size) / 2.0f);
		m_titleBarDrawCall.SetWorldTransform(titleBarScale * titleBarTranslation * worldTransform);

		for (auto& button : m_titleBarButtons)
		{
			button->SetColor(m_titleBarColor);
			button->Update(timeDeltaInSeconds, titleBarTranslation * worldTransform, hands, XMVectorGetZ(m_size) / 4.0f, suspendInteractions);
		}

		m_titleBarSpacerButton->SetColor(m_titleBarColor);
		m_titleBarSpacerButton->Update(timeDeltaInSeconds, titleBarTranslation * worldTransform, hands, XMVectorGetZ(m_size) / 4.0f, suspendInteractions);
	}

	for (auto& button : m_buttons)
	{
		button->Update(timeDeltaInSeconds, worldTransform, hands, XMVectorGetZ(m_size) / 2.0f, suspendInteractions);
	}

	// If there is an active modal child slate, suspend interactions on all other child slates,
	//  otherwise just pass through the suspendInteractions value
	if (m_activeModalChildSlate)
	{
		for (auto& slate : m_childSlates)
			slate->Update(timeDeltaInSeconds, worldTransform, hands, slate != m_activeModalChildSlate);
	}
	else
	{
		for (auto& slate : m_childSlates)
			slate->Update(timeDeltaInSeconds, worldTransform, hands, suspendInteractions);
	}
}

XMVECTOR FloatingSlate::GetPosition()
{
	return m_interpolatedTransform.CalculateTranslation(m_lerpAmount);
}

void FloatingSlate::SetPosition(const XMVECTOR& position)
{
	m_interpolatedTransform.SetTargetPosition(position);
}

XMVECTOR FloatingSlate::GetRotation()
{
	return m_interpolatedTransform.CalculateRotation(m_lerpAmount);
}

void FloatingSlate::SetRotation(const XMVECTOR& rotation)
{
	m_interpolatedTransform.SetTargetRotation(rotation);
}

void FloatingSlate::SetRotation(const XMVECTOR& forwardDirection, const XMVECTOR& upDirection)
{
	XMVECTOR rotation = CalculateQuaternionFromBasisVectors(forwardDirection, upDirection);
	m_interpolatedTransform.SetTargetRotation(rotation);
}

XMVECTOR FloatingSlate::GetSize()
{
	return m_size;
}

void FloatingSlate::SetSize(const XMVECTOR& size)
{
	m_size = size;
}

void FloatingSlate::SetMinScale(const XMVECTOR& minScale)
{
	m_minScale = minScale;
	m_interpolatedTransform.SetStartingScale(m_minScale);
}

void FloatingSlate::SetMaxScale(const XMVECTOR& maxScale)
{
	m_maxScale = maxScale;
	m_interpolatedTransform.SetTargetScale(m_maxScale);
}

void FloatingSlate::SetHorizontalPivot(SlateHorizontalPivot horizontalPivot)
{
	m_horizontalPivot = horizontalPivot;
}

void FloatingSlate::SetVerticalPivot(SlateVerticalPivot verticalPivot)
{
	m_verticalPivot = verticalPivot;
}

const XMVECTOR& FloatingSlate::GetColor()
{
	return m_color;
}

void FloatingSlate::SetColor(const XMVECTOR& color)
{
	m_color = color;
}

void FloatingSlate::SetTitleBarColor(const XMVECTOR& color)
{
	m_titleBarColor = color;
}

void FloatingSlate::ShowTitleBar()
{
	m_titleBarHidden = false;
}

void FloatingSlate::HideTitleBar()
{
	m_titleBarHidden = true;
}

bool FloatingSlate::TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, float& distance)
{
	if (m_hidden)
		return false;

	bool hit = false;
	float closestDistance = FLT_MAX;
	float lastDistance = 0;

	for (auto& button : m_buttons)
	{
		if (button->TestRayIntersection(rayOriginInWorldSpace, rayDirectionInWorldSpace, lastDistance))
		{
			hit = true;

			if (lastDistance < closestDistance)
				closestDistance = lastDistance;
		}
	}

	for (auto& button : m_titleBarButtons)
	{
		if (button->TestRayIntersection(rayOriginInWorldSpace, rayDirectionInWorldSpace, lastDistance))
		{
			hit = true;

			if (lastDistance < closestDistance)
				closestDistance = lastDistance;
		}
	}

	if (m_baseSlateDrawCall.TestRayIntersection(rayOriginInWorldSpace, rayDirectionInWorldSpace, lastDistance))
	{
		hit = true;

		if (lastDistance < closestDistance)
			closestDistance = lastDistance;
	}

	if (!m_titleBarHidden && m_titleBarDrawCall.TestRayIntersection(rayOriginInWorldSpace, rayDirectionInWorldSpace, lastDistance))
	{
		hit = true;

		if (lastDistance < closestDistance)
			closestDistance = lastDistance;
	}

	for (auto& slate : m_childSlates)
	{
		if (slate->TestRayIntersection(rayOriginInWorldSpace, rayDirectionInWorldSpace, lastDistance))
		{
			hit = true;

			if (lastDistance < closestDistance)
				closestDistance = lastDistance;
		}
	}

	if (hit)
	{
		distance = closestDistance;
	}

	return hit;
}

XMVECTOR FloatingSlate::GetFullHierarchySize()
{
	XMVECTOR size = XMVectorZero();
	CalculateFullHierarchySize(size);
	return size;
}

void FloatingSlate::CalculateFullHierarchySize(XMVECTOR& runningSize)
{
	if (!m_hidden)
	{
		if (XMVectorGetX(m_size) > XMVectorGetX(runningSize))
			runningSize = XMVectorSetX(runningSize, XMVectorGetX(m_size));
		if (XMVectorGetY(m_size) > XMVectorGetY(runningSize))
			runningSize = XMVectorSetY(runningSize, XMVectorGetY(m_size));
		if (XMVectorGetZ(m_size) > XMVectorGetZ(runningSize))
			runningSize = XMVectorSetZ(runningSize, XMVectorGetZ(m_size));
	}

	for (auto& slate : m_childSlates)
		slate->CalculateFullHierarchySize(runningSize);
}

void FloatingSlate::SetTitleBarText(const std::string& text)
{
	if (text != m_titleBarText)
	{
		m_titleBarText = text;
		m_buttonLayoutNeedsUpdate = true;
	}
}

const std::string& FloatingSlate::GetTitleBarText()
{
	return m_titleBarText;
}

void FloatingSlate::Draw()
{
	if (!m_hidden && !m_buttonLayoutNeedsUpdate)
	{
		m_baseSlateDrawCall.Draw();
		for (auto& button : m_buttons)
			button->Draw();

		if (!m_titleBarHidden)
		{
			m_titleBarDrawCall.Draw();
			for (auto& button : m_titleBarButtons)
				button->Draw();

			m_titleBarSpacerButton->Draw();
		}

		for (auto& slate : m_childSlates)
			slate->Draw();
	}
}

void FloatingSlate::Show()
{
	m_hidden = false;
}

void FloatingSlate::Hide()
{
	m_hidden = true;
}

bool FloatingSlate::IsHidden()
{
	return m_hidden;
}

bool FloatingSlate::IsAnimating()
{
	return m_animationState != AnimationState::Complete;
}

bool FloatingSlate::IsOpening()
{
	return IsAnimating() && !m_hideOnAnimationComplete;
}

bool FloatingSlate::IsClosing()
{
	return IsAnimating() && m_hideOnAnimationComplete;
}

void FloatingSlate::AnimateOpen(const XMVECTOR& targetPosition, const XMVECTOR& targetRotation)
{
	m_interpolatedTransform.SetStartingPosition(GetPosition());
	m_interpolatedTransform.SetTargetPosition(targetPosition);

	m_interpolatedTransform.SetStartingRotation(GetRotation());
	m_interpolatedTransform.SetTargetRotation(targetRotation);

	m_interpolatedTransform.SetStartingScale(m_minScale);
	m_interpolatedTransform.SetTargetScale(m_maxScale);

	m_lerpAmount = 0.0f;
	m_hideOnAnimationComplete = false;
	m_animationTimer.Reset();
	m_animationState = AnimationState::PreTimer;

	Show();
}

void FloatingSlate::AnimateOpen(const XMVECTOR& targetPosition, const XMVECTOR& forwardDirection, const XMVECTOR& upDirection)
{
	AnimateOpen(targetPosition, CalculateQuaternionFromBasisVectors(forwardDirection, upDirection));
}

void FloatingSlate::AnimateOpen(const XMVECTOR& targetPosition)
{
	AnimateOpen(targetPosition, GetRotation());
}

void FloatingSlate::AnimateOpen()
{
	AnimateOpen(GetPosition());
}

void FloatingSlate::AnimateClose(const XMVECTOR& targetPosition, const XMVECTOR& targetRotation)
{
	m_interpolatedTransform.SetStartingPosition(GetPosition());
	m_interpolatedTransform.SetTargetPosition(targetPosition);

	m_interpolatedTransform.SetStartingRotation(GetRotation());
	m_interpolatedTransform.SetTargetRotation(targetRotation);

	m_interpolatedTransform.SetStartingScale(m_maxScale);
	m_interpolatedTransform.SetTargetScale(m_minScale);

	m_lerpAmount = 0.0f;
	m_hideOnAnimationComplete = true;
	m_animationTimer.Reset();
	m_animationState = AnimationState::PreTimer;
}

void FloatingSlate::AnimateClose(const XMVECTOR& targetPosition, const XMVECTOR& forwardDirection, const XMVECTOR& upDirection)
{
	AnimateClose(targetPosition, CalculateQuaternionFromBasisVectors(forwardDirection, upDirection));
}

void FloatingSlate::AnimateClose(const XMVECTOR& targetPosition)
{
	XMVECTOR targetRotation = m_interpolatedTransform.GetStartingRotation();
	AnimateClose(targetPosition, targetRotation);
}

void FloatingSlate::AnimateClose()
{
	XMVECTOR targetPosition = m_interpolatedTransform.GetStartingPosition();
	AnimateClose(targetPosition);
}

void FloatingSlate::SetAnimationBuffer(float animationBuffer)
{
	m_animationBuffer = animationBuffer;
}

void FloatingSlate::OpenChildSlate(std::shared_ptr<FloatingSlate> childSlate, ChildSlatePositionMode positionMode, bool isModal, XMFLOAT2 startOffset, XMFLOAT2 targetOffset, XMFLOAT2 startScale)
{
	if (!childSlate)
		return;

	XMVECTOR minScale = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR startPosition = XMVectorZero();
	XMVECTOR targetPosition = XMVectorZero();

	switch (positionMode)
	{
	case ChildSlatePositionMode::Left:
	{
		float parentMenuWidth = XMVectorGetX(GetSize());
		float childMenuWidth = XMVectorGetX(childSlate->GetSize());
		minScale = XMVectorSet(0.01f, 1.0f, 1.0f, 0.0f);
		targetPosition = XMVectorSet(-parentMenuWidth / 2.0f + targetOffset.x, targetOffset.y, 0.0f, 1.0f);
		startPosition = XMVectorSet(-parentMenuWidth / 2.0f + startOffset.x, startOffset.y, 0.0f, 1.0f);
		break;
	}
	case ChildSlatePositionMode::Right:
	{
		float parentMenuWidth = XMVectorGetX(GetSize());
		float childMenuWidth = XMVectorGetX(childSlate->GetSize());
		minScale = XMVectorSet(0.01f, 1.0f, 1.0f, 0.0f);
		targetPosition = XMVectorSet(parentMenuWidth / 2.0f + targetOffset.x, targetOffset.y, 0.0f, 1.0f);
		startPosition = XMVectorSet(parentMenuWidth / 2.0f + startOffset.x, startOffset.y, 0.0f, 1.0f);
		break;
	}
	case ChildSlatePositionMode::Front:
	{
		float parentMenuDepth = XMVectorGetZ(GetSize());
		float childMenuDepth = XMVectorGetZ(childSlate->GetSize());
		minScale = XMVectorSet(startScale.x, startScale.y, 0.01f, 0.0f);
		targetPosition = XMVectorSet(targetOffset.x, targetOffset.y, parentMenuDepth / 2.0f + childMenuDepth / 2.0f, 1.0f);
		startPosition = XMVectorSet(startOffset.x, startOffset.y, 0.0f, 1.0f);
		childSlate->SetAnimationBuffer(0.25f);
		break;
	}
	}

	if (childSlate->IsHidden())
	{
		childSlate->SetMinScale(minScale);
		childSlate->SetPosition(startPosition);
		childSlate->AnimateOpen(targetPosition);
	}
	else
	{
		childSlate->SetPosition(targetPosition);
	}

	if (isModal)
	{
		m_activeModalChildSlate = childSlate;
	}
}

void FloatingSlate::AddButton(std::shared_ptr<FloatingSlateButton> button)
{
	if (!button)
		return;

	m_buttons.push_back(button);
	m_buttonLayoutNeedsUpdate = true;
}

void FloatingSlate::AddTitleBarButton(std::shared_ptr<FloatingSlateButton> button)
{
	if (!button)
		return;

	m_titleBarButtons.push_back(button);
	m_buttonLayoutNeedsUpdate = true;
}

std::shared_ptr<FloatingSlateButton> FloatingSlate::GetButton(const unsigned buttonID)
{
	for (auto& button : m_buttons)
		if (button->GetID() == buttonID)
			return button;

	for (auto& button : m_titleBarButtons)
		if (button->GetID() == buttonID)
			return button;

	return nullptr;
}

std::shared_ptr<FloatingSlateButton> FloatingSlate::GetButton(const std::string& buttonText)
{
	for (auto& button : m_buttons)
		if (button->GetText() == buttonText)
			return button;

	for (auto& button : m_titleBarButtons)
		if (button->GetText() == buttonText)
			return button;

	return nullptr;
}

std::vector<std::shared_ptr<FloatingSlateButton>>& FloatingSlate::GetButtons()
{
	return m_buttons;
}

std::vector<std::shared_ptr<FloatingSlateButton>>& FloatingSlate::GetTitleBarButtons()
{
	return m_titleBarButtons;
}

void FloatingSlate::ClearButtons()
{
	m_buttons.clear();
}

void FloatingSlate::AddChildSlate(std::shared_ptr<FloatingSlate> slate)
{
	if (!slate)
		return;

	m_childSlates.push_back(slate);
}

void FloatingSlate::CalculateButtonSpacing(float& columnSpacing, float& rowSpacing)
{
	float minButtonWidth = FLT_MAX;
	float minButtonHeight = FLT_MAX;

	for (auto& button : m_buttons)
	{
		auto size = button->GetSize();
		if (XMVectorGetX(size) < minButtonWidth)
			minButtonWidth = XMVectorGetX(size);
		if (XMVectorGetY(size) < minButtonHeight)
			minButtonHeight = XMVectorGetY(size);
	}

	const float defaultMinSpacing = 0.0025f;

	float slateWidth = XMVectorGetX(m_size);
	float slateHeight = XMVectorGetY(m_size);

	float columnMinSpacing = defaultMinSpacing;
	if (columnMinSpacing + minButtonWidth > slateWidth)
		columnMinSpacing = (slateWidth - minButtonWidth) / 2.0f;

	unsigned buttonsPerRow = (unsigned)(slateWidth / (minButtonWidth + columnMinSpacing));
	columnSpacing = (slateWidth - buttonsPerRow * minButtonWidth) / (buttonsPerRow + 1);

	float rowMinSpacing = defaultMinSpacing;
	if (rowMinSpacing + minButtonHeight > slateHeight)
		rowMinSpacing = (slateHeight - minButtonHeight) / 2.0f;

	unsigned buttonsPerCol = (unsigned)(slateHeight / (minButtonHeight + rowMinSpacing));
	rowSpacing = (slateHeight - buttonsPerCol * minButtonHeight) / (buttonsPerCol + 1);
}

void FloatingSlate::ReflowButtons()
{
	if (m_buttons.empty() || m_flowMode == FlowMode::Off)
		return;

	float slateWidth = XMVectorGetX(m_size);
	float slateHeight = XMVectorGetY(m_size);

	float columnSpacing = 0, rowSpacing = 0;
	CalculateButtonSpacing(columnSpacing, rowSpacing);

	float startingLeftX = -slateWidth / 2.0f + columnSpacing;
	float leftX = startingLeftX;
	float maxX = slateWidth / 2.0f - columnSpacing;

	float startingTopY = slateHeight / 2.0f - rowSpacing;
	float topY = startingTopY;
	float minY = -slateHeight / 2.0f + rowSpacing;

	float widestFromPreviousColumn = 0.0f;
	float tallestFromPreviousRow = 0.0f;

	const float epsilon = 0.0001f;

	for (unsigned buttonIndex = 0; buttonIndex < (unsigned) m_buttons.size(); ++buttonIndex)
	{
		auto& button = m_buttons[buttonIndex];
		auto size = button->GetSize();
		float width = XMVectorGetX(size);
		float height = XMVectorGetY(size);

		if (m_flowMode == FlowMode::RowFirst)
		{
			if (leftX != startingLeftX && (leftX + width - maxX) > epsilon)
			{
				leftX = startingLeftX;
				topY -= (tallestFromPreviousRow + rowSpacing);
				tallestFromPreviousRow = 0.0f;
			}
		}
		else if (m_flowMode == FlowMode::ColumnFirst)
		{
			if (topY != startingTopY && (topY - height - minY) < -epsilon)
			{
				topY = startingTopY;
				leftX += (widestFromPreviousColumn + columnSpacing);
				widestFromPreviousColumn = 0.0f;
			}
		}

		button->SetPosition(XMVectorSet(leftX + width / 2.0f, topY - height / 2.0f, 0.0f, 1.0f));

		if (m_flowMode == FlowMode::RowFirst)
		{
			leftX += (width + columnSpacing);

			if (height > tallestFromPreviousRow)
				tallestFromPreviousRow = height;
		}
		else if (m_flowMode == FlowMode::ColumnFirst)
		{
			topY -= (height + rowSpacing);

			if (width > widestFromPreviousColumn)
				widestFromPreviousColumn = width;
		}

	}
}

void FloatingSlate::ReflowTitleBarButtons()
{
	float slateWidth = XMVectorGetX(m_size);
	float buttonSpacing = 0.001f;

	for (size_t buttonIndex = 0; buttonIndex < m_titleBarButtons.size(); ++buttonIndex)
	{
		float buttonXOffset = slateWidth / 2.0f - m_titleBarHeight / 2.0f - buttonIndex * (m_titleBarHeight + buttonSpacing);
		buttonXOffset *= m_titleBarFlowDirection;

		m_titleBarButtons[buttonIndex]->SetPosition(XMVectorSet(buttonXOffset, 0.0f, 0.0f, 1.0f));
		m_titleBarButtons[buttonIndex]->SetSize(XMVectorSet(m_titleBarHeight, m_titleBarHeight, XMVectorGetZ(m_size) / 2.0f, 0.0f));
	}

	float spacerButtonLeftEdge = -slateWidth / 2.0f;
	float spacerButtonRightEdge = slateWidth / 2.0f - m_titleBarButtons.size() * (m_titleBarHeight + buttonSpacing);
	m_titleBarSpacerButton->SetPosition(XMVectorSet(m_titleBarFlowDirection * (spacerButtonLeftEdge + spacerButtonRightEdge) / 2.0f, 0.0f, 0.0f, 1.0f));
	m_titleBarSpacerButton->SetSize(XMVectorSet(spacerButtonRightEdge - spacerButtonLeftEdge, m_titleBarHeight, XMVectorGetZ(m_size) / 2.0f, 0.0f));
	m_titleBarSpacerButton->SetShape(Mesh::MT_BOX);
	
	if(!m_titleBarText.empty())
		m_titleBarSpacerButton->SetText(m_titleBarText);
}

XMVECTOR FloatingSlate::CalculateQuaternionFromBasisVectors(const XMVECTOR& forwardDirection, const XMVECTOR& upDirection)
{
	XMMATRIX rotationMatrix = XMMatrixTranspose(XMMatrixLookToLH(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), forwardDirection, upDirection));
	return XMQuaternionRotationMatrix(rotationMatrix);
}
