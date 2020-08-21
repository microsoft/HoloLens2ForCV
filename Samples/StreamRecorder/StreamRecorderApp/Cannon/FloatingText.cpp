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


#include "FloatingText.h"

#include <memory>
using namespace std;

FloatingText::FloatingText() :
	m_drawCall("Unlit_VS.cso", "UnlitTexture_PS.cso", make_shared<Mesh>(Mesh::MT_UIPLANE))
{
}

void FloatingText::SetText(const std::string& text)
{
	m_text = text;
	m_textureNeedsUpdate = true;
}

void FloatingText::SetFontSize(const float fontSize)
{
	m_fontSize = fontSize;
	m_textureNeedsUpdate = true;
}

void FloatingText::SetDPI(const float dpi)
{
	m_dpi = dpi;
	m_textureNeedsUpdate = true;
}

void FloatingText::SetHorizontalAlignment(const HorizontalAlignment horizontalAlignment)
{
	m_horizontalAlignment = horizontalAlignment;
}

void FloatingText::SetVerticalAlignment(const VerticalAlignment verticalAlignment)
{
	m_verticalAlignment = verticalAlignment;
}

void FloatingText::SetPosition(const XMVECTOR& position)
{
	m_position = position;
}

void FloatingText::SetForwardDirection(const DirectX::XMVECTOR& forwardDirection)
{
	m_forwardDirection = forwardDirection;
}

void FloatingText::SetUpDirection(const DirectX::XMVECTOR& upDirection)
{
	m_upDirection = upDirection;
}

void FloatingText::SetSize(const XMVECTOR& size)
{
	if (!XMVector3Equal(m_size, size))
	{
		m_size = size;
		m_textureNeedsUpdate = true;
	}
}

void FloatingText::Render()
{
	if (m_textureNeedsUpdate)
		UpdateTexture();

	float pivotX = 0.0f;
	if (m_horizontalAlignment == HorizontalAlignment::Left)
		pivotX = 0.5f;
	else if (m_horizontalAlignment == HorizontalAlignment::Right)
		pivotX = -0.5f;

	float pivotY = 0.0f;
	if (m_verticalAlignment == VerticalAlignment::Bottom)
		pivotY = 0.5f;
	else if (m_verticalAlignment == VerticalAlignment::Top)
		pivotY = -0.5f;

	XMMATRIX pivot = XMMatrixTranslation(pivotX, pivotY, 0.0f);

	XMMATRIX scale = XMMatrixScalingFromVector(m_size);

	XMMATRIX rotation = XMMatrixLookToRH(
		XMVectorSet(0.0f, 0.0f, 0.f, 1.0f),
		-m_forwardDirection,
		m_upDirection);
	rotation = XMMatrixTranspose(rotation);

	XMMATRIX translation = XMMatrixTranslationFromVector(m_position);

	DrawCall::PushDepthTestState(false);
	DrawCall::PushAlphaBlendState(DrawCall::BLEND_ALPHA);
	m_texture->BindAsPixelShaderResource(0);
	m_drawCall.SetWorldTransform(pivot * scale * rotation * translation);
	m_drawCall.Draw();
	DrawCall::PopAlphaBlendState();
	DrawCall::PopDepthTestState();
}

void FloatingText::UpdateTexture()
{
	float dpm = m_dpi * 0.393f * 100.0f;
	unsigned requiredWidth = (unsigned)(dpm * XMVectorGetX(m_size));
	unsigned requiredHeight = (unsigned)(dpm * XMVectorGetY(m_size));

	// Snap to nearest multiple of 64
	const unsigned multipleOf = 64;
	requiredWidth = (requiredWidth / multipleOf) * multipleOf;
	requiredHeight = (requiredHeight / multipleOf) * multipleOf;

	if (!m_texture || m_texture->GetWidth() != requiredWidth || m_texture->GetHeight() != requiredHeight)
	{
		m_texture = make_shared<Texture2D>(requiredWidth, requiredHeight);
	}

	DrawCall::PushRenderPass(0, m_texture);
	m_texture->Clear(0.0f, 0.0f, 0.0f, 0.0f);

	D2D_RECT_F layoutRect{ 0.0f, 0.0f, (float)m_texture->GetWidth(), (float)m_texture->GetHeight() };
	DrawCall::DrawText(m_text, layoutRect, m_fontSize, m_horizontalAlignment, m_verticalAlignment);

	DrawCall::PopRenderPass();

	m_textureNeedsUpdate = false;
}
