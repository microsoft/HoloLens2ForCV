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

#include "DrawCall.h"
#include "TrackedHands.h"
#include "AnimatedVector.h"
#include "Common/Timer.h"

#include <memory>
#include <vector>

enum class FollowHandMode
{
	None,
	LeftIndex,
	LeftPalm,
	LeftInnerEdge,
	RightIndex,
	RightPalm,
	RightInnerEdge,
	Mode_Count
};

class IFloatingSlateButtonCallback
{
public:

	virtual void OnButtonPressed(class FloatingSlateButton* pButton) = 0;
};

class FloatingSlateButton
{
public:

	static void SetDefaultFontSize(const float fontSize);

	FloatingSlateButton(const XMVECTOR& position, const XMVECTOR& size, const XMVECTOR& color, const unsigned id, IFloatingSlateButtonCallback* pCallback, std::shared_ptr<Texture2D> texture);
	FloatingSlateButton(const XMVECTOR& position, const XMVECTOR& size, const XMVECTOR& color, const unsigned id, IFloatingSlateButtonCallback* pCallback, const std::string& buttonText);
	FloatingSlateButton(const XMVECTOR& position, const XMVECTOR& size, const XMVECTOR& color, const unsigned id, IFloatingSlateButtonCallback* pCallback);
	FloatingSlateButton(const unsigned id, IFloatingSlateButtonCallback* pCallback, std::shared_ptr<Texture2D> texture);
	FloatingSlateButton(const unsigned id, IFloatingSlateButtonCallback* pCallback, const std::string& buttonText);
	FloatingSlateButton(const unsigned id, IFloatingSlateButtonCallback* pCallback);
	FloatingSlateButton();

	void SetShape(Mesh::MeshType meshType);

	XMVECTOR GetPosition();
	void SetPosition(const XMVECTOR& position);
	XMVECTOR GetSize();
	void SetSize(const XMVECTOR& size);

	void Update(float timeDeltaInSeconds, XMMATRIX& parentTransform, TrackedHands& hands, float offsetToSlateSurface, bool suspendInteractions = false);

	bool TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, float& distance);

	void Draw();

	void SetText(const std::string& buttonText);
	void SetFontSize(const float fontSize);			// Default size is 36. Buttons textures are rendered at 96*2DPI

	XMVECTOR GetColor();
	void SetColor(const XMVECTOR& color);
	void SetHighlightColor(const XMVECTOR& color);
	unsigned GetID();
	const std::string& GetText();

	bool IsHidden();
	void SetHidden(bool hidden);

	bool IsOn();
	bool IsDisabled();
	void SetDisabled(bool disabled);
	bool IsButtonHighlighted();
	void SetButtonStayHighlightedState(bool stayHighlighted);
	bool IsButtonPushedIn();
	void SetButtonPushedInState(bool pushedIn);
	void SetOutlineWhenHighlighted(bool outlineWhenHighlighted);
	void SetSpacer(bool spacer);

	void RegisterCallback(IFloatingSlateButtonCallback* pCallback);

	// Registers buttons that can't be enabled at the same time as this button. Whenever this button is set
	//  as pushed-in or highlighted, it will automatically un-push or un-highlight all registered buttons.
	void RegisterMutuallyExclusiveButton(std::shared_ptr<FloatingSlateButton> button);

private:

	static float m_defaultFontSize;

	DrawCall m_drawCall;
	DrawCall m_outlineDrawCall;
	std::shared_ptr<Texture2D> m_texture;
	bool m_geometryNeedsUpdate;
	bool m_textureNeedsUpdate;
	std::string m_buttonText;
	float m_fontSize;

	XMVECTOR m_position;			// Starting position of the button when in the un-pushed state
	XMVECTOR m_size;				// Size of the button (baked into the mesh)
	float m_pushDistance;			// Distance the button has moved from the un-pushed state
	float m_buttonSpringSpeed;		// How fast the button springs back to unpushed state in meters / second
	int m_activePushingHandIndex;	// Index of hand actively pushing this button, or -1 if no hand is currently pushing it

	XMVECTOR m_color;
	XMVECTOR m_highlightColor;
	bool m_autoHighlightColor;		// True if highlight color should be automatically generated from color (on by default, disabled if highlight color is set manually)
	unsigned m_id;

	Mesh::MeshType m_buttonShape;

	bool m_hidden;					// Button will not be rendered and cannot be pressed

	bool m_on;						// True when button is fully pushed and sends an event
	bool m_disabled;				// True if button is disabled and can't be interacted with (will render darker)
	bool m_stayHighlighted;			// True if button is set to remain highlighted but pushable
	bool m_pushedIn;				// True if button is set to remain in the pushed-in state (highlighted, stuck at max push amount, and disabled)
	bool m_outlineWhenHighlighted;	// True if button should draw a white outline around the button when highlighted
	bool m_spacer;					// True if button is just a spacer (can't be interacted with but will render at expected color)

	bool m_highlightActive;			// True if the button is currently being highlighted (due to combinations of the above states)

	std::vector<IFloatingSlateButtonCallback*> m_registeredCallbacks;
	std::vector<std::shared_ptr<FloatingSlateButton>> m_mutuallyExclusiveButtons;
	
	void UpdateGeometry();
	void UpdateTexture();
};

enum class FlowMode
{
	Off,
	RowFirst,
	ColumnFirst
};

enum class ChildSlatePositionMode
{
	Left,
	Right,
	Front
};

enum class SlateHorizontalPivot
{
	Left,
	Right,
	Center
};

enum class SlateVerticalPivot
{
	Top,
	Bottom,
	Center
};

class FloatingSlate
{
public:

	static void SetTitleBarHeight(float titleBarHeight);
	static void SetDefaultColor(const DirectX::XMVECTOR& color);
	static void SetDefaultTitleBarColor(const DirectX::XMVECTOR& color);

	FloatingSlate();
	FloatingSlate(const XMVECTOR& size);

	void CalculateFollowHandPositionAndRotation(TrackedHands& hands, XMVECTOR& targetPosition, XMVECTOR& targetRotation);
	void Update(float timeDeltaInSeconds, TrackedHands& hands);
	void Update(float timeDeltaInSeconds, const XMMATRIX& parentTransform, TrackedHands& hands, bool suspendInteractions = false);

	XMVECTOR GetPosition();
	void SetPosition(const XMVECTOR& position);

	XMVECTOR FloatingSlate::GetRotation();
	void SetRotation(const XMVECTOR& rotation);
	void SetRotation(const XMVECTOR& forwardDirection, const XMVECTOR& upDirection);

	XMVECTOR GetSize();
	void SetSize(const XMVECTOR& size);

	void SetMinScale(const XMVECTOR& minScale);
	void SetMaxScale(const XMVECTOR& maxScale);

	void SetHorizontalPivot(SlateHorizontalPivot horizontalPivot);
	void SetVerticalPivot(SlateVerticalPivot verticalPivot);

	const XMVECTOR& GetColor();
	void SetColor(const XMVECTOR& color);
	void SetTitleBarColor(const XMVECTOR& color);

	void ShowTitleBar();
	void HideTitleBar();

	bool TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, float& distance);
	XMVECTOR GetFullHierarchySize();	// Combined size of this slate (if visible) and all child slates (if visible)

	void SetFollowHandMode(FollowHandMode mode) { m_followHand = mode; }
	FollowHandMode GetFollowHandMode() { return m_followHand; }

	void SetFlowMode(FlowMode mode) { m_flowMode = mode; }
	FlowMode GetFlowMode() { return m_flowMode; }

	void SetTitleBarFlowDirection(float direction) { m_titleBarFlowDirection = direction; m_buttonLayoutNeedsUpdate = true; }
	void SetTitleBarText(const std::string& text);
	const std::string& GetTitleBarText();

	void Draw();

	void Show();
	void Hide();

	bool IsHidden();
	bool IsAnimating();

	bool IsOpening();
	bool IsClosing();

	// Open from current position/rotation to target position/rotation
	void AnimateOpen(const XMVECTOR& targetPosition, const XMVECTOR& targetRotation);

	// Open from current position/rotation to target position/rotation
	void AnimateOpen(const XMVECTOR& targetPosition, const XMVECTOR& forwardDirection, const XMVECTOR& upDirection);

	// Open from current position/rotation to target position, current rotation
	void AnimateOpen(const XMVECTOR& targetPosition);

	// Open from current position/rotation to current position/rotation
	void AnimateOpen();

	// Close from current position/rotation to target position/rotation
	void AnimateClose(const XMVECTOR& targetPosition, const XMVECTOR& targetRotation);

	// Close from current position/rotation to target position/rotation
	void AnimateClose(const XMVECTOR& targetPosition, const XMVECTOR& forwardDirection, const XMVECTOR& upDirection);

	// Close from current position/rotation to target position, original rotation
	void AnimateClose(const XMVECTOR& targetPosition);

	// Close from current position/rotation to original position/rotation
	void AnimateClose();

	// Opens the given child slate. Animates open if child slate is closed, otherwise it just repositions the slate.
	void OpenChildSlate(std::shared_ptr<FloatingSlate> childSlate, ChildSlatePositionMode positionMode, bool isModal = false, XMFLOAT2 startOffset = { 0.0f, 0.0f }, XMFLOAT2 targetOffset = { 0.0f, 0.0f }, XMFLOAT2 startScale = { 1.0f, 1.0f });

	// Animation buffer is an extra time delay before and after the animation
	void SetAnimationBuffer(float animationBuffer);

	void AddButton(std::shared_ptr<FloatingSlateButton> button);			// Button will be positioned relative to center of slate
	void AddTitleBarButton(std::shared_ptr<FloatingSlateButton> button);	// Button will be positioned relative to center of title bar
	std::shared_ptr<FloatingSlateButton> GetButton(const unsigned buttonID);
	std::shared_ptr<FloatingSlateButton> GetButton(const std::string& buttonText);
	std::vector<std::shared_ptr<FloatingSlateButton>>& GetButtons();
	std::vector<std::shared_ptr<FloatingSlateButton>>& GetTitleBarButtons();
	void ClearButtons();

	void AddChildSlate(std::shared_ptr<FloatingSlate> slate);

	void CalculateButtonSpacing(float& columnSpacing, float& rowSpacing);

private:

	DrawCall m_baseSlateDrawCall;
	DrawCall m_titleBarDrawCall;

	XMVECTOR m_size;				// Base size of the slate, does not affect child objects
	XMVECTOR m_minScale;			// Minimum scale of the slate when closed
	XMVECTOR m_maxScale;			// Maximum scale of the slate when open

	SlateHorizontalPivot m_horizontalPivot;
	SlateVerticalPivot m_verticalPivot;

	// The InterpolatedTransform lerps between a starting point and real-time target position of slate.
	//  It is used to store the world transform (which is t=1) and support open/close animation.
	InterpolatedTransform m_interpolatedTransform;
	float m_lerpAmount;
	bool m_hideOnAnimationComplete;
	Timer m_animationTimer;
	float m_animationBuffer = 0.0f;

	enum class AnimationState
	{
		PreTimer,
		Lerping,
		PostTimer,
		Complete
	};
	AnimationState m_animationState = AnimationState::Complete;

	XMVECTOR m_color;
	XMVECTOR m_titleBarColor;

	static float m_titleBarHeight;
	static XMVECTOR m_defaultColor;
	static XMVECTOR m_defaultTitleBarColor;

	float m_titleBarFlowDirection;	// 1.0f for left-right, -1.0f for right-left
	std::string m_titleBarText;

	bool m_hidden = false;
	bool m_titleBarHidden = false;
	FollowHandMode m_followHand = FollowHandMode::None;
	FlowMode m_flowMode = FlowMode::Off;

	bool m_buttonLayoutNeedsUpdate = true;

	std::vector<std::shared_ptr<FloatingSlateButton>> m_buttons;

	std::shared_ptr<FloatingSlateButton> m_titleBarSpacerButton;
	std::vector<std::shared_ptr<FloatingSlateButton>> m_titleBarButtons;

	std::shared_ptr<FloatingSlate> m_activeModalChildSlate;			// Only set when there is currently an active modal child slate - causes this slate and all other children to suspend interactions
	std::vector<std::shared_ptr<FloatingSlate>> m_childSlates;

	void ReflowButtons();
	void ReflowTitleBarButtons();

	void CalculateFullHierarchySize(XMVECTOR& runningSize);
	XMVECTOR CalculateQuaternionFromBasisVectors(const XMVECTOR& forwardDirection, const XMVECTOR& upDirection);
};