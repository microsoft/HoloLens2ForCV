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


#include "MixedReality.h"
#include "Common/Timer.h"
#include "Common/FileUtilities.h"

#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Perception.Spatial.Preview.h>
#include <robuffer.h>
#include <WindowsNumerics.h>

#include <algorithm>
#include <set>
#include <thread>
#include <mutex>

using namespace DirectX;
using namespace std;


InputSource::InputSource() :
	id(0),
	lastTimestamp(0),
	type(InputType::Other),
	handedness(Handedness::None)
{
	buttonStates[SpatialButton::SELECT] = false;
	buttonStates[SpatialButton::GRAB] = false;
	buttonStates[SpatialButton::MENU] = false;

	buttonPresses = buttonStates;
	buttonReleases = buttonStates;

	position = XMVectorZero();
	orientation = XMVectorZero();

	rayPosition = XMVectorZero();
	rayDirection = XMVectorZero();

	handMeshWorldTransform = XMMatrixIdentity();
}

bool MixedReality::IsAvailable()
{
	return winrt::Windows::Graphics::Holographic::HolographicSpace::IsAvailable();
}

MixedReality::MixedReality() :
	m_mixedRealityEnabled(false),
	m_inputWaitLastFrameTimestamp(0)
{
	m_headPosition = XMVectorZero();
	m_headForwardDirection = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
	m_headUpDirection = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	m_gravityDirection = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

	m_isArticulatedHandTrackingAPIAvailable = false;

	m_isEyeTrackingAvailable = false;
	m_isEyeTrackingRequested = false;
	m_isEyeTrackingEnabled = false;
	m_isEyeTrackingActive = false;
	m_eyeGazeOrigin = XMVectorZero();
	m_eyeGazeDirection = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
}

bool MixedReality::EnableMixedReality()
{
	if (!IsAvailable())
		return false;

	m_holoSpace = winrt::Windows::Graphics::Holographic::HolographicSpace::CreateForCoreWindow(winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread());
	if (!m_holoSpace)
		return false;

	m_locator = winrt::Windows::Perception::Spatial::SpatialLocator::GetDefault();
	m_locator.LocatabilityChanged({ this, &MixedReality::OnLocatabilityChanged });
	m_locatability = m_locator.Locatability();

	m_referenceFrame = m_locator.CreateStationaryFrameOfReferenceAtCurrentLocation();
	m_attachedReferenceFrame = m_locator.CreateAttachedFrameOfReferenceAtCurrentHeading();

	Microsoft::WRL::ComPtr<ID3D11Device> device = DrawCall::GetD3DDevice();
	Microsoft::WRL::ComPtr<IDXGIDevice3> dxgiDevice;
	device.As(&dxgiDevice);

	winrt::com_ptr<IInspectable> object;
	CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), (IInspectable**)(winrt::put_abi(object)));
	auto interopDevice = object.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
	m_holoSpace.SetDirect3D11Device(interopDevice);

	m_spatialInteractionManager = winrt::Windows::UI::Input::Spatial::SpatialInteractionManager::GetForCurrentView();

	if (winrt::Windows::Foundation::Metadata::ApiInformation::IsMethodPresent(L"Windows.UI.Input.Spatial.SpatialInteractionSourceState", L"TryGetHandPose"))
		m_isArticulatedHandTrackingAPIAvailable = true;

	if (winrt::Windows::Foundation::Metadata::ApiInformation::IsMethodPresent(L"Windows.Perception.People.EyesPose", L"IsSupported"))
		m_isEyeTrackingAvailable = winrt::Windows::Perception::People::EyesPose::IsSupported();

	auto display = winrt::Windows::Graphics::Holographic::HolographicDisplay::GetDefault();
	auto view = display.TryGetViewConfiguration(winrt::Windows::Graphics::Holographic::HolographicViewConfigurationKind::PhotoVideoCamera);
	if (view != nullptr)
		view.IsEnabled(true);

	m_mixedRealityEnabled = true;
	return true;
}

bool MixedReality::IsEnabled()
{
	return m_mixedRealityEnabled;
}

void MixedReality::EnableSurfaceMapping()
{
	if (m_referenceFrame && !m_surfaceMapping)
	{
		m_surfaceMapping = make_shared<SurfaceMapping>(m_referenceFrame);
	}
}

bool MixedReality::IsSurfaceMappingActive()
{
	return m_surfaceMapping != nullptr && m_surfaceMapping->IsActive();
}

std::shared_ptr<SurfaceMapping> MixedReality::GetSurfaceMappingInterface()
{
	return m_surfaceMapping;
}

void MixedReality::EnableQRCodeTracking()
{
#ifdef ENABLE_QRCODE_API
	if(m_mixedRealityEnabled)
		m_qrCodeTracker = make_shared<QRCodeTracker>();
#endif
}

bool MixedReality::IsQRCodeTrackingActive()
{
#ifdef ENABLE_QRCODE_API
	return m_qrCodeTracker != nullptr && m_qrCodeTracker->IsActive();
#endif

	return false;
}

void MixedReality::DisableQRCodeTracking()
{
#ifdef ENABLE_QRCODE_API
	m_qrCodeTracker.reset();
#endif
}

const std::vector<QRCode>& MixedReality::GetTrackedQRCodeList()
{
#ifdef ENABLE_QRCODE_API
	if (m_qrCodeTracker)
		return m_qrCodeTracker->GetTrackedQRCodeList();
#endif

	static std::vector<QRCode> blankList;
	return blankList;
}

const QRCode& MixedReality::GetTrackedQRCode(const std::string& value)
{
#ifdef ENABLE_QRCODE_API
	if (m_qrCodeTracker)
		return m_qrCodeTracker->GetTrackedQRCode(value);
#endif

	static QRCode blankCode;
	return blankCode;
}

void MixedReality::SetWorldCoordinateSystemToDefault()
{
	m_worldCoordinateSystemQRCodeValue.clear();
	m_worldCoordinateSystemOverride = nullptr;
}

void MixedReality::SetWorldCoordinateSystemToQRCode(const std::string& qrCodeValue)
{
	m_worldCoordinateSystemQRCodeValue = qrCodeValue;
}

winrt::Windows::Perception::Spatial::SpatialCoordinateSystem MixedReality::GetWorldCoordinateSystem()
{
	if (m_worldCoordinateSystemOverride && m_locatability == winrt::Windows::Perception::Spatial::SpatialLocatability::PositionalTrackingActive)
	{
		return m_worldCoordinateSystemOverride;
	}
	else if (m_referenceFrame && m_locatability == winrt::Windows::Perception::Spatial::SpatialLocatability::PositionalTrackingActive)
	{
		return m_referenceFrame.CoordinateSystem();
	}
	else if (m_attachedReferenceFrame)
	{
		return m_currentAttachedCoordinateSystem;
	}
	return nullptr;
}

void MixedReality::Update()
{
	if (!m_mixedRealityEnabled)
		return;

	m_holoFrame = m_holoSpace.CreateNextFrame();

	auto prediction = m_holoFrame.CurrentPrediction();
	m_currentAttachedCoordinateSystem = m_attachedReferenceFrame.GetStationaryCoordinateSystemAtTimestamp(prediction.Timestamp());

#ifdef ENABLE_QRCODE_API
	if (m_qrCodeTracker)
	{
		if (!m_worldCoordinateSystemQRCodeValue.empty())
		{
			auto& qrCode = m_qrCodeTracker->GetTrackedQRCode(m_worldCoordinateSystemQRCodeValue);
			if (qrCode.instanceID != 0)
				m_worldCoordinateSystemOverride = m_qrCodeTracker->GetCoordinateSystemForQRCode(qrCode.instanceID);
		}

		m_qrCodeTracker->Update(GetWorldCoordinateSystem());
	}
#endif

	auto currentCoordinateSystem = GetWorldCoordinateSystem();
	auto stationaryCoordinateSystem = m_referenceFrame.CoordinateSystem();
	if (currentCoordinateSystem && stationaryCoordinateSystem)
	{
		auto fromStationary = stationaryCoordinateSystem.TryGetTransformTo(currentCoordinateSystem);
		if (fromStationary)
		{
			XMMATRIX gravityAlignedTransform = XMLoadFloat4x4(&fromStationary.Value());
			m_gravityDirection = XMVector3TransformNormal(XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), gravityAlignedTransform);
		}
	}
	else
	{
		m_gravityDirection = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
	}

	UpdateAnchors();

	winrt::Windows::UI::Input::Spatial::SpatialPointerPose pointerPose = winrt::Windows::UI::Input::Spatial::SpatialPointerPose::TryGetAtTimestamp(GetWorldCoordinateSystem(), prediction.Timestamp());
	if (pointerPose)
	{
		m_headPosition = XMVectorSetW(XMLoadFloat3(&pointerPose.Head().Position()), 1.0f);
		m_headForwardDirection = XMLoadFloat3(&pointerPose.Head().ForwardDirection());
		m_headUpDirection = XMLoadFloat3(&pointerPose.Head().UpDirection());

		if (m_isEyeTrackingEnabled)
		{
			if (pointerPose.Eyes() && pointerPose.Eyes().IsCalibrationValid())
			{
				m_isEyeTrackingActive = true;

				if (pointerPose.Eyes().Gaze())
				{
					auto spatialRay = pointerPose.Eyes().Gaze().Value();
					m_eyeGazeOrigin = XMVectorSetW(XMLoadFloat3(&spatialRay.Origin), 1.0f);
					m_eyeGazeDirection = XMLoadFloat3(&spatialRay.Direction);
				}
			}
			else
			{
				m_isEyeTrackingActive = false;
			}
		}
	}

	if (m_isEyeTrackingAvailable && m_isEyeTrackingRequested && !m_isEyeTrackingEnabled)
	{
		m_isEyeTrackingRequested = false;

		thread requestAccessThread([this]()
			{
				auto status = winrt::Windows::Perception::People::EyesPose::RequestAccessAsync().get();

				if (status == winrt::Windows::UI::Input::GazeInputAccessStatus::Allowed)
					m_isEyeTrackingEnabled = true;
				else
					m_isEyeTrackingEnabled = false;
			});

		requestAccessThread.detach();
	}

	auto sourceStates = m_spatialInteractionManager.GetDetectedSourcesAtTimestamp(prediction.Timestamp());

	set<unsigned> sourcesUpdated;
	for (auto sourceState : sourceStates)
	{
		UpdateInputSource(sourceState);
		sourcesUpdated.insert(sourceState.Source().Id());
	}

	set<unsigned> sourcesToRemove;
	for (auto& sourceEntry : m_activeSources)
	{
		if (sourcesUpdated.find(sourceEntry.first) == sourcesUpdated.end())
			sourcesToRemove.insert(sourceEntry.first);
	}
	for (auto id : sourcesToRemove)
	{
		if (id == m_primarySourceID)
			m_primarySourceID = 0;

		m_activeSources.erase(id);
		m_activeHandMeshObservers.erase(id);
	}

	if(m_surfaceMapping)
		m_surfaceMapping->Update(m_headPosition);
}

long long MixedReality::GetPredictedDisplayTime()
{
	if (m_holoFrame)
		return m_holoFrame.CurrentPrediction().Timestamp().TargetTime().time_since_epoch().count();

	return 0;
}

const DirectX::XMVECTOR& MixedReality::GetHeadPosition()
{
	return m_headPosition;
}

const DirectX::XMVECTOR& MixedReality::GetHeadForwardDirection()
{
	return m_headForwardDirection;
}

const DirectX::XMVECTOR& MixedReality::GetHeadUpDirection()
{
	return m_headUpDirection;
}

const DirectX::XMVECTOR& MixedReality::GetGravityDirection()
{
	return m_gravityDirection;
}

bool MixedReality::IsEyeTrackingAvailable()
{
	return m_isEyeTrackingAvailable;
}

void MixedReality::EnableEyeTracking()
{
	m_isEyeTrackingRequested = true;
}

bool MixedReality::IsEyeTrackingEnabled()
{
	return m_isEyeTrackingEnabled;
}

bool MixedReality::IsEyeTrackingActive()
{
	return m_isEyeTrackingActive;
}

const DirectX::XMVECTOR& MixedReality::GetEyeGazeOrigin()
{
	return m_eyeGazeOrigin;
}

const DirectX::XMVECTOR& MixedReality::GetEyeGazeDirection()
{
	return m_eyeGazeDirection;
}

bool MixedReality::IsButtonDown(SpatialButton button)
{
	bool buttonDown = false;
	for (auto& source : m_activeSources)
	{
		if (source.second->buttonStates[button])
			buttonDown = true;
	}

	return buttonDown;
}

bool MixedReality::WasButtonPressed(SpatialButton button)
{
	bool buttonPressed = false;
	for (auto& source : m_activeSources)
	{
		if (source.second->buttonPresses[button])
			buttonPressed = true;
	}

	return buttonPressed;
}

bool MixedReality::WasButtonReleased(SpatialButton button)
{
	bool buttonReleased = false;
	for (auto& source : m_activeSources)
	{
		if (source.second->buttonReleases[button])
			buttonReleased = true;
	}

	return buttonReleased;
}

InputSource* MixedReality::GetPrimarySource()
{
	if (m_primarySourceID != 0)
	{
		auto it = m_activeSources.find(m_primarySourceID);
		if (it != m_activeSources.end())
		{
			return it->second.get();
		}
	}
	else if (!m_activeSources.empty())
	{
		return m_activeSources.begin()->second.get();
	}

	return nullptr;
}

InputSource* MixedReality::GetHand(size_t handIndex)
{
	Handedness targetHandedness = (handIndex == 0) ? Handedness::Left : Handedness::Right;
	for (auto& source : m_activeSources)
	{
		if (source.second->type == InputType::Hand && source.second->handedness == targetHandedness)
			return source.second.get();
	}

	return nullptr;
}

size_t MixedReality::CreateAnchor(const XMMATRIX& transform)
{
	if (!IsEnabled())
		return 0;

	winrt::Windows::Foundation::Numerics::float3 positionAsFloat3;
	winrt::Windows::Foundation::Numerics::quaternion orientationAsQuaternion;
	XMStoreFloat3((XMFLOAT3*)&positionAsFloat3, XMVector3TransformCoord(XMVectorZero(), transform));
	XMStoreFloat4((XMFLOAT4*)&orientationAsQuaternion, XMQuaternionRotationMatrix(transform));

	auto anchor = winrt::Windows::Perception::Spatial::SpatialAnchor::TryCreateRelativeTo(GetWorldCoordinateSystem(), positionAsFloat3, orientationAsQuaternion);
	if (anchor)
	{
		AnchorRecord anchorRecord;
		anchorRecord.anchor = anchor;
		anchorRecord.anchorID = m_nextAnchorID++;

		m_anchorRecords.insert({ anchorRecord.anchorID, anchorRecord });
		return anchorRecord.anchorID;
	}
	else
	{
		return 0;
	}
}


void MixedReality::DeleteAnchor(size_t anchorID)
{
	auto& anchorRecordIt = m_anchorRecords.find(anchorID);
	if (anchorRecordIt != m_anchorRecords.end())
		m_anchorRecords.erase(anchorID);
}

void MixedReality::UpdateAnchors()
{
	if (!IsEnabled())
		return;

	for (auto& anchorRecordIt : m_anchorRecords)
	{
		auto& worldCoordinateSystem = GetWorldCoordinateSystem();
		auto toWorld = anchorRecordIt.second.anchor.CoordinateSystem().TryGetTransformTo(worldCoordinateSystem);
		if (toWorld)
		{
			anchorRecordIt.second.worldTransform = XMLoadFloat4x4(&toWorld.Value());
			anchorRecordIt.second.isFound = true;
		}
		else
		{
			anchorRecordIt.second.isFound = false;
		}
	}
}

const XMMATRIX MixedReality::GetAnchorWorldTransform(const size_t anchorID)
{
	auto anchorRecordIt = m_anchorRecords.find(anchorID);
	if (anchorRecordIt != m_anchorRecords.end())
		return anchorRecordIt->second.worldTransform;
	else
		return XMMatrixIdentity();
}

const bool MixedReality::IsAnchorFound(const size_t anchorID)
{
	auto anchorRecordIt = m_anchorRecords.find(anchorID);
	if (anchorRecordIt != m_anchorRecords.end())
		return anchorRecordIt->second.isFound;
	else
		return false;
}

void MixedReality::ClearAnchors()
{
	m_anchorRecords.clear();
}

size_t MixedReality::GetCameraPoseCount()
{
	if(m_holoFrame)
		return m_holoFrame.CurrentPrediction().CameraPoses().Size();

	return 0;
}

void MixedReality::SetFocusPoint(size_t cameraPoseIndex, const DirectX::XMVECTOR& focusPoint)
{
	if (!m_holoFrame)
		return;
	
	auto cameraPoses = m_holoFrame.CurrentPrediction().CameraPoses();
	if (cameraPoseIndex < cameraPoses.Size())
	{
		auto renderingParameters = m_holoFrame.GetRenderingParameters(cameraPoses.GetAt((unsigned)cameraPoseIndex));

		winrt::Windows::Foundation::Numerics::float3 float3FocusPoint;
		XMStoreFloat3(&float3FocusPoint, focusPoint);
		renderingParameters.SetFocusPoint(GetWorldCoordinateSystem(), float3FocusPoint);
	}
}

void MixedReality::CommitDepthBuffer(size_t cameraPoseIndex, ID3D11Texture2D* pD3DDepthTexture)
{
	if (!m_holoFrame)
		return;

	auto cameraPoses = m_holoFrame.CurrentPrediction().CameraPoses();
	if (cameraPoseIndex < cameraPoses.Size())
	{
		auto renderingParameters = m_holoFrame.GetRenderingParameters(cameraPoses.GetAt((unsigned)cameraPoseIndex));

		Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture(pD3DDepthTexture);
		Microsoft::WRL::ComPtr<IDXGIResource1> depthStencilResource;
		depthTexture.As(&depthStencilResource);
		Microsoft::WRL::ComPtr<IDXGISurface2> depthDxgiSurface;
		depthStencilResource->CreateSubresourceSurface(0, &depthDxgiSurface);
		winrt::com_ptr<::IInspectable> inspectableSurface;
		CreateDirect3D11SurfaceFromDXGISurface(depthDxgiSurface.Get(), inspectableSurface.put());

		auto depthWinRTSurface = inspectableSurface.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface>();
		renderingParameters.CommitDirect3D11DepthBuffer(depthWinRTSurface);
	}
}

void MixedReality::SetNearPlaneDistance(size_t cameraPoseIndex, float nearPlaneDistance)
{	
	if (!m_holoFrame)
		return;

	auto cameraPoses = m_holoFrame.CurrentPrediction().CameraPoses();
	if (cameraPoseIndex < cameraPoses.Size())
	{
		cameraPoses.GetAt((unsigned)cameraPoseIndex).HolographicCamera().SetNearPlaneDistance(nearPlaneDistance);
	}
}

void MixedReality::SetFarPlaneDistance(size_t cameraPoseIndex, float farPlaneDistance)
{
	if (!m_holoFrame)
		return;

	auto cameraPoses = m_holoFrame.CurrentPrediction().CameraPoses();
	if (cameraPoseIndex < cameraPoses.Size())
	{
		cameraPoses.GetAt((unsigned)cameraPoseIndex).HolographicCamera().SetFarPlaneDistance(farPlaneDistance);
	}
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> MixedReality::GetBackBuffer(size_t cameraPoseIndex)
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> d3dBackBuffer;

	if (m_holoFrame)
	{
		auto cameraPoses = m_holoFrame.CurrentPrediction().CameraPoses();
		if (cameraPoseIndex < cameraPoses.Size())
		{
			auto renderingParameters = m_holoFrame.GetRenderingParameters(cameraPoses.GetAt((unsigned)cameraPoseIndex));
			renderingParameters.Direct3D11BackBuffer().as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>()->GetInterface(IID_PPV_ARGS(&d3dBackBuffer));
		}
	}

	return d3dBackBuffer;
}

D3D11_VIEWPORT MixedReality::GetViewport(size_t cameraPoseIndex)
{
	if (m_holoFrame)
	{
		auto cameraPoses = m_holoFrame.CurrentPrediction().CameraPoses();
		if (cameraPoseIndex < cameraPoses.Size())
		{
			auto viewport = cameraPoses.GetAt((unsigned)cameraPoseIndex).Viewport();
			return CD3D11_VIEWPORT(viewport.X, viewport.Y, viewport.Width, viewport.Height);
		}
	}

	return 	CD3D11_VIEWPORT(0.0f, 0.0f, 0.0f, 0.0f);
}

bool MixedReality::GetViewMatrices(size_t cameraPoseIndex, DirectX::XMMATRIX& leftView, DirectX::XMMATRIX& rightView)
{
	if (m_holoFrame)
	{
		auto cameraPoses = m_holoFrame.CurrentPrediction().CameraPoses();
		if (cameraPoseIndex < cameraPoses.Size())
		{
			auto viewTransform = cameraPoses.GetAt((unsigned)cameraPoseIndex).TryGetViewTransform(GetWorldCoordinateSystem());
			if (viewTransform != nullptr)
			{
				auto viewMatrices = viewTransform.Value();
				leftView = XMLoadFloat4x4(&viewMatrices.Left);
				rightView = XMLoadFloat4x4(&viewMatrices.Right);
				return true;
			}
		}
	}

	return false;
}

void MixedReality::GetProjMatrices(size_t cameraPoseIndex, DirectX::XMMATRIX& leftProj, DirectX::XMMATRIX& rightProj)
{
	if (m_holoFrame)
	{
		auto cameraPoses = m_holoFrame.CurrentPrediction().CameraPoses();
		if (cameraPoseIndex < cameraPoses.Size())
		{
			auto projTransform = cameraPoses.GetAt((unsigned)cameraPoseIndex).ProjectionTransform();
			leftProj = XMLoadFloat4x4(&projTransform.Left);
			rightProj = XMLoadFloat4x4(&projTransform.Right);
		}
	}
}

void MixedReality::PresentAndWait()
{
	if(m_holoFrame)
		m_holoFrame.PresentUsingCurrentPrediction(winrt::Windows::Graphics::Holographic::HolographicFramePresentWaitBehavior::WaitForFrameToFinish);
}

void MixedReality::PresentAndDontWait()
{
	if (m_holoFrame)
		m_holoFrame.PresentUsingCurrentPrediction(winrt::Windows::Graphics::Holographic::HolographicFramePresentWaitBehavior::DoNotWaitForFrameToFinish);
}

winrt::Windows::Foundation::IAsyncAction CreateHandMeshObserver(winrt::Windows::UI::Input::Spatial::SpatialInteractionSourceState currentState,
																	std::map<unsigned, winrt::Windows::Perception::People::HandMeshObserver>& activeHandMeshObservers)
{
	auto newHandMeshObserver = co_await currentState.Source().TryCreateHandMeshObserverAsync();
	if (newHandMeshObserver)
	{
		activeHandMeshObservers.insert({ currentState.Source().Id(), newHandMeshObserver });
	}
}

void MixedReality::UpdateInputSource(winrt::Windows::UI::Input::Spatial::SpatialInteractionSourceState currentState)
{
	auto sourceID = currentState.Source().Id();

	auto it = m_activeSources.find(sourceID);
	if (it == m_activeSources.end())
	{
		shared_ptr<InputSource> newSource = make_shared<InputSource>();
		memset(newSource.get(), 0, sizeof(newSource));
		newSource->id = sourceID;

		m_activeSources[sourceID] = newSource;
		it = m_activeSources.find(sourceID);

		if (m_isArticulatedHandTrackingAPIAvailable)
		{
			CreateHandMeshObserver(currentState, m_activeHandMeshObservers);
		}
	}

	if (it->second->lastTimestamp != currentState.Timestamp().TargetTime().time_since_epoch().count())
	{
		shared_ptr<InputSource> recordedSource = it->second;

		recordedSource->lastTimestamp = currentState.Timestamp().TargetTime().time_since_epoch().count();

		auto lastButtonStates = recordedSource->buttonStates;

		recordedSource->buttonStates[SpatialButton::SELECT] = currentState.IsSelectPressed();
		recordedSource->buttonStates[SpatialButton::GRAB] = currentState.IsGrasped();
		recordedSource->buttonStates[SpatialButton::MENU] = currentState.IsMenuPressed();

		auto type = currentState.Source().Kind();
		switch (type)
		{
			case winrt::Windows::UI::Input::Spatial::SpatialInteractionSourceKind::Controller:
				recordedSource->type = InputType::Controller;
				break;
			case winrt::Windows::UI::Input::Spatial::SpatialInteractionSourceKind::Hand:
				recordedSource->type = InputType::Hand;
				break;
			default:
				recordedSource->type = InputType::Other;
				break;
		}

		auto handedness = currentState.Source().Handedness();
		switch (handedness)
		{
			case winrt::Windows::UI::Input::Spatial::SpatialInteractionSourceHandedness::Left:
				recordedSource->handedness = Handedness::Left;
				break;
			case winrt::Windows::UI::Input::Spatial::SpatialInteractionSourceHandedness::Right:
				recordedSource->handedness = Handedness::Right;
				break;
			default:
				recordedSource->handedness = Handedness::None;
				break;
		}

		winrt::Windows::Perception::People::HandPose handPose{ nullptr };
		if (m_isArticulatedHandTrackingAPIAvailable)
		{
			handPose = currentState.TryGetHandPose();
		}

		if (handPose)
		{
			static vector<winrt::Windows::Perception::People::HandJointKind> requestedJointIndices;
			if (requestedJointIndices.empty())
			{
				requestedJointIndices.resize((size_t)HandJointIndex::Count);
				for (int jointIndex = 0; jointIndex < (int)HandJointIndex::Count; ++jointIndex)
					requestedJointIndices[jointIndex] = (winrt::Windows::Perception::People::HandJointKind)jointIndex;
			}

			static vector<winrt::Windows::Perception::People::JointPose> jointPoses(requestedJointIndices.size());

			if (handPose.TryGetJoints(GetWorldCoordinateSystem(), requestedJointIndices, jointPoses))
			{
				auto jointCount = requestedJointIndices.size();
				recordedSource->handJoints.resize(jointCount);

				for (unsigned jointIndex = 0; jointIndex < jointCount; ++jointIndex)
				{
					HandJoint &currentHandJoint = recordedSource->handJoints[jointIndex];
					currentHandJoint.position = XMVectorSetW(XMLoadFloat3(&jointPoses[jointIndex].Position), 1.0f);
					currentHandJoint.orientation = XMLoadFloat4((XMFLOAT4*)&jointPoses[jointIndex].Orientation);
					currentHandJoint.radius = jointPoses[jointIndex].Radius;
					currentHandJoint.trackedState = jointPoses[jointIndex].Accuracy == winrt::Windows::Perception::People::JointPoseAccuracy::High;
				}
			}

			auto handMeshObserverRecord = m_activeHandMeshObservers.find(recordedSource->id);
			if (handMeshObserverRecord != m_activeHandMeshObservers.end())
			{
				auto handMeshObserver = handMeshObserverRecord->second;

				if (recordedSource->handMeshIndices.empty())
				{
					unsigned sourceIndexCount = handMeshObserver.TriangleIndexCount();
					m_indexScratchBuffer.resize(sourceIndexCount);
					handMeshObserver.GetTriangleIndices(m_indexScratchBuffer);

					recordedSource->handMeshIndices.resize(sourceIndexCount);
					for (unsigned i = 0; i < sourceIndexCount; ++i)
						recordedSource->handMeshIndices[i] = (unsigned int)m_indexScratchBuffer[i];
				}

				auto vertexState = handMeshObserver.GetVertexStateForPose(handPose);
				unsigned int sourceVertexCount = handMeshObserver.VertexCount();
				m_vertexScratchBuffer.resize(sourceVertexCount);
				vertexState.GetVertices(m_vertexScratchBuffer);

				if (vertexState.CoordinateSystem())	// WORKAROUND: We shouldn't have to check this.  It should never null, but sometimes it is.  It's a platform bug.
				{
					auto tryTransform = vertexState.CoordinateSystem().TryGetTransformTo(GetWorldCoordinateSystem());
					if (tryTransform)
						recordedSource->handMeshWorldTransform = XMLoadFloat4x4(&tryTransform.Value());

					recordedSource->handMeshVertices.resize(sourceVertexCount);
					for (unsigned vertexIndex = 0; vertexIndex < sourceVertexCount; ++vertexIndex)
					{
						recordedSource->handMeshVertices[vertexIndex].position = XMVectorSetW(XMLoadFloat3(&m_vertexScratchBuffer[vertexIndex].Position), 1.0f);
						recordedSource->handMeshVertices[vertexIndex].normal = XMLoadFloat3(&m_vertexScratchBuffer[vertexIndex].Normal);
						recordedSource->handMeshVertices[vertexIndex].texcoord = { 0.0f, 0.0f };
					}
				}
			}
		}

		if (currentState.IsSelectPressed() || currentState.IsGrasped() || currentState.IsMenuPressed())
		{
			m_primarySourceID = sourceID;
		}

		for (auto& entry : recordedSource->buttonStates)
		{
			if (entry.second == true && lastButtonStates[entry.first] == false)
				recordedSource->buttonPresses[entry.first] = true;
			else
				recordedSource->buttonPresses[entry.first] = false;

			if (entry.second == false && lastButtonStates[entry.first] == true)
				recordedSource->buttonReleases[entry.first] = true;
			else
				recordedSource->buttonReleases[entry.first] = false;
		}

		auto location = currentState.Properties().TryGetLocation(GetWorldCoordinateSystem());
		if (location)
		{
			auto position = location.Position();
			if(position)
				recordedSource->position = XMLoadFloat3(&position.Value());
			
			auto orientation = location.Orientation();
			if (orientation)
				recordedSource->orientation = XMLoadFloat4((XMFLOAT4*)&orientation.Value());

			auto pointingRay = location.SourcePointerPose();
			if (pointingRay)
			{
				recordedSource->rayPosition = XMVectorSetW(XMLoadFloat3(&pointingRay.Position()), 1.0f);
				recordedSource->rayDirection = XMVectorSetW(XMLoadFloat3(&pointingRay.ForwardDirection()), 0.0f);
			}
		}
	}
}

void MixedReality::OnLocatabilityChanged(winrt::Windows::Perception::Spatial::SpatialLocator const& locator, winrt::Windows::Foundation::IInspectable const&)
{
	m_locatability = locator.Locatability();
}

bool MixedReality::GetHeadPoseAtTimestamp(long long fileTimeTimestamp, DirectX::XMVECTOR& position, DirectX::XMVECTOR& direction, DirectX::XMVECTOR& up)
{
	if (m_referenceFrame)
	{
		auto dateTime = winrt::clock::from_file_time(winrt::file_time(fileTimeTimestamp));
		auto timestampObject = winrt::Windows::Perception::PerceptionTimestampHelper::FromHistoricalTargetTime(dateTime);
		auto pointerPose = winrt::Windows::UI::Input::Spatial::SpatialPointerPose::TryGetAtTimestamp(GetWorldCoordinateSystem(), timestampObject);
		if (pointerPose)
		{
			position = XMLoadFloat3(&pointerPose.Head().Position());
			direction = XMLoadFloat3(&pointerPose.Head().ForwardDirection());
			up = XMLoadFloat3(&pointerPose.Head().UpDirection());
			return true;
		}
	}

	return false;
}

SurfaceMapping::SurfaceMapping(winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference const& referenceFrame) :
	m_referenceFrame(referenceFrame),
	m_isActive(false),
	m_surfaceDrawMode(SurfaceDrawMode::None),
	m_headPosition(XMVectorZero()),
	m_numberOfSurfacesInProcessingQueue(0),
	m_surfaceObservationThread(new std::thread(&SurfaceMapping::SurfaceObservationThreadFunction, this))
{
}

void SurfaceMapping::CreaterObserverIfNeeded()
{
	if (m_surfaceObserver)
		return;

	auto status = winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceObserver::RequestAccessAsync().get();
	if (status == winrt::Windows::Perception::Spatial::SpatialPerceptionAccessStatus::Allowed)
	{
		m_surfaceObserver = winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceObserver();
	}
}

// Returns the list of observed surfaces that are new or in need of an update.
// The list is sorted newest to oldest with brand new meshes appearing after the oldest.
// Code processing this list should work from back to front, so then new meshes get processed first,
//  followed by meshes that have gone the longest without an update.

void SurfaceMapping::GetLatestSurfacesToProcess(std::vector<TimestampSurfacePair>& surfacesToProcess)
{
	auto observedSurfaces = m_surfaceObserver.GetObservedSurfaces();

	m_meshRecordsMutex.lock();

	for (auto const& observedSurfacePair : observedSurfaces)
	{
		auto surfaceInfo = observedSurfacePair.Value();

		auto meshRecordIterator = m_meshRecords.find(surfaceInfo.Id());
		if (meshRecordIterator == m_meshRecords.end())
		{
			surfacesToProcess.push_back(pair<long long, winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceInfo>(0, surfaceInfo));
		}
		else if (surfaceInfo.UpdateTime().time_since_epoch().count() - meshRecordIterator->second.lastSurfaceUpdateTime > 5 * 10000000 || !meshRecordIterator->second.mesh)
		{
			surfacesToProcess.push_back(pair<long long, winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceInfo>(meshRecordIterator->second.lastMeshUpdateTime, surfaceInfo));
		}
	}

	for (auto& meshRecordPair : m_meshRecords)
	{
		if (!observedSurfaces.HasKey(meshRecordPair.first))
			m_meshRecordIDsToErase.push_back(meshRecordPair.first);
	}

	m_meshRecordsMutex.unlock();

	sort(surfacesToProcess.begin(), surfacesToProcess.end(), [](const pair<long long, winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceInfo>& a, const pair<long long, winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceInfo>& b)
		{
			return a.first > b.first;
		});
}

void SurfaceMapping::SurfaceObservationThreadFunction()
{
	vector<TimestampSurfacePair> surfacesToProcess;

	for (;;)
	{
		CreaterObserverIfNeeded();
		if (!m_surfaceObserver || !m_referenceFrame)
		{
			Sleep(50);
			continue;
		}

		m_headPositionMutex.lock();
		winrt::Windows::Perception::Spatial::SpatialBoundingBox box = { { XMVectorGetX(m_headPosition), XMVectorGetY(m_headPosition), XMVectorGetZ(m_headPosition) }, { 10.f, 10.f, 5.f } };
		winrt::Windows::Perception::Spatial::SpatialBoundingVolume bounds = winrt::Windows::Perception::Spatial::SpatialBoundingVolume::FromBox(m_referenceFrame.CoordinateSystem(), box);
		m_surfaceObserver.SetBoundingVolume(bounds);
		m_headPositionMutex.unlock();

		if (surfacesToProcess.empty())
		{
			Sleep(50);
			GetLatestSurfacesToProcess(surfacesToProcess);
		}

		while (!surfacesToProcess.empty())
		{
			Sleep(50);

			auto surfaceInfo = surfacesToProcess.back().second;
			surfacesToProcess.pop_back();

			m_numberOfSurfacesInProcessingQueueMutex.lock();
			m_numberOfSurfacesInProcessingQueue = (unsigned)surfacesToProcess.size();
			m_numberOfSurfacesInProcessingQueueMutex.unlock();

			auto options = winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceMeshOptions();
			options.IncludeVertexNormals(true);
			auto sourceMesh = surfaceInfo.TryComputeLatestMeshAsync(1000.0, options).get();
			if (sourceMesh)
			{
				MeshRecord newMeshRecord;
				newMeshRecord.id = sourceMesh.SurfaceInfo().Id();
				newMeshRecord.sourceMesh = sourceMesh;
				newMeshRecord.lastMeshUpdateTime = Timer::GetSystemRelativeTime();
				newMeshRecord.lastSurfaceUpdateTime = sourceMesh.SurfaceInfo().UpdateTime().time_since_epoch().count();
				newMeshRecord.color = XMVectorSet(0.5f, 0.5f, 0.5f, 1.0f);

				auto tryTransform = sourceMesh.CoordinateSystem().TryGetTransformTo(m_referenceFrame.CoordinateSystem());
				if (tryTransform)
					newMeshRecord.worldTransform = tryTransform.Value();

				newMeshRecord.mesh = make_shared<Mesh>(nullptr, 0);
				ConvertMesh(newMeshRecord.sourceMesh, newMeshRecord.mesh);
				newMeshRecord.sourceMesh = nullptr;
				newMeshRecord.mesh->UpdateBoundingBox();

				if (m_surfaceDrawMode != SurfaceDrawMode::None)
					newMeshRecord.InitDrawCall();

				m_newMeshRecordsMutex.lock();
				m_newMeshRecords.push_back(newMeshRecord);
				m_newMeshRecordsMutex.unlock();
			}
		}
	}
}

unsigned SurfaceMapping::GetNumberOfSurfacesInProcessingQueue()
{
	lock_guard<mutex> lock(m_numberOfSurfacesInProcessingQueueMutex);
	return m_numberOfSurfacesInProcessingQueue;
}

bool SurfaceMapping::IsActive()
{
	return m_isActive;
}

void SurfaceMapping::SetSurfaceDrawMode(SurfaceDrawMode surfaceDrawMode)
{
	m_surfaceDrawMode = surfaceDrawMode;
}

SurfaceDrawMode SurfaceMapping::GetSurfaceDrawMode()
{
	return m_surfaceDrawMode;
}

void SurfaceMapping::Update(const XMVECTOR& headPosition)
{
	m_headPositionMutex.lock();
	m_headPosition = headPosition;
	m_headPositionMutex.unlock();

	m_meshRecordsMutex.lock();

	for (auto& guid : m_meshRecordIDsToErase)
	{
		m_meshRecords.erase(guid);
	}
	m_meshRecordIDsToErase.clear();

	for (auto& meshRecord : m_newMeshRecords)
	{
		m_meshRecords[meshRecord.id] = meshRecord;
	}
	m_newMeshRecords.clear();

	if (!m_meshRecords.empty())
		m_isActive = true;

	m_meshRecordsMutex.unlock();
}

void SurfaceMapping::ConvertMesh(winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceMesh sourceMesh, shared_ptr<Mesh> destinationMesh)
{
	if (!sourceMesh || !destinationMesh)
		return;

	auto spatialIndexFormat = sourceMesh.TriangleIndices().Format();
	assert((DXGI_FORMAT)spatialIndexFormat == DXGI_FORMAT_R16_UINT);

	auto spatialPositionsFormat = sourceMesh.VertexPositions().Format();
	assert((DXGI_FORMAT)spatialPositionsFormat == DXGI_FORMAT_R16G16B16A16_SNORM);

	auto spatialNormalsFormat = sourceMesh.VertexNormals().Format();
	assert((DXGI_FORMAT)spatialNormalsFormat == DXGI_FORMAT_R8G8B8A8_SNORM);

	Microsoft::WRL::ComPtr<IUnknown> unknown;
	Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> bufferByteAccess;

	unknown = (IUnknown*)winrt::get_abi(sourceMesh.TriangleIndices().Data());
	unknown.As(&bufferByteAccess);
	unsigned short* pSourceIndexBuffer = nullptr;
	bufferByteAccess->Buffer((unsigned char**)& pSourceIndexBuffer);

	unknown = (IUnknown*)winrt::get_abi(sourceMesh.VertexPositions().Data());
	unknown.As(&bufferByteAccess);
	short* pSourcePositionsBuffer = nullptr;
	bufferByteAccess->Buffer((unsigned char**)& pSourcePositionsBuffer);

	unknown = (IUnknown*)winrt::get_abi(sourceMesh.VertexNormals().Data());
	unknown.As(&bufferByteAccess);
	char* pSourceNormalsBuffer = nullptr;
	bufferByteAccess->Buffer((unsigned char**)& pSourceNormalsBuffer);

	auto vertexScaleFactor = sourceMesh.VertexPositionScale();
	float short_max = pow(2.0f, 15.0f);
	float char_max = pow(2.0f, 7.0f);

	assert(sourceMesh.VertexPositions().ElementCount() == sourceMesh.VertexNormals().ElementCount());

	auto& vertexBuffer = destinationMesh->GetVertices();
	vertexBuffer.resize(sourceMesh.VertexPositions().ElementCount());
	auto& indexBuffer = destinationMesh->GetIndices();
	indexBuffer.resize(sourceMesh.TriangleIndices().ElementCount());

	for (unsigned i = 0; i < indexBuffer.size(); ++i)
	{
		indexBuffer[i] = pSourceIndexBuffer[i];
	}

	for (unsigned i = 0; i < vertexBuffer.size(); ++i)
	{
		unsigned sourceIndex = i * 4;

		vertexBuffer[i].position = XMVectorSet(pSourcePositionsBuffer[sourceIndex + 0] / short_max * vertexScaleFactor.x,
			pSourcePositionsBuffer[sourceIndex + 1] / short_max * vertexScaleFactor.y,
			pSourcePositionsBuffer[sourceIndex + 2] / short_max * vertexScaleFactor.z,
			1.0f);

		vertexBuffer[i].normal = XMVectorSet(pSourceNormalsBuffer[sourceIndex + 0] / char_max,
			pSourceNormalsBuffer[sourceIndex + 1] / char_max,
			pSourceNormalsBuffer[sourceIndex + 2] / char_max,
			0.0f);

		vertexBuffer[i].texcoord.x = 0;
		vertexBuffer[i].texcoord.y = 0;
	}
}

void SurfaceMapping::DrawMeshes()
{
	if (m_surfaceDrawMode == SurfaceDrawMode::None)
		return;

	if(m_surfaceDrawMode == SurfaceDrawMode::Occlusion)
		DrawCall::PushAlphaBlendState(DrawCall::BLEND_COLOR_DISABLED);

	m_meshRecordsMutex.lock();
	for (auto& pair : m_meshRecords)
	{
		if (!pair.second.drawCall)
		{
			pair.second.InitDrawCall();
		}

		pair.second.drawCall->Draw();
	}
	m_meshRecordsMutex.unlock();

	if (m_surfaceDrawMode == SurfaceDrawMode::Occlusion)
		DrawCall::PopAlphaBlendState();
}

bool SurfaceMapping::TestRayIntersection(XMVECTOR rayOrigin, XMVECTOR rayDirection, float& distance, XMVECTOR& normal)
{
	bool hit = false;
	distance = FLT_MAX;

	m_meshRecordsMutex.lock();
	for (auto& meshRecordPair : m_meshRecords)
	{
		if (!meshRecordPair.second.mesh)
			continue;

		XMMATRIX worldTransform = XMLoadFloat4x4(&meshRecordPair.second.worldTransform);

		float currentDistance;
		XMVECTOR currentNormal;

		bool result = meshRecordPair.second.mesh->TestRayIntersection(rayOrigin, rayDirection, worldTransform, currentDistance, currentNormal);

		if (result && currentDistance < distance)
		{
			hit = true;
			distance = currentDistance;
			normal = currentNormal;
		}
	}
	m_meshRecordsMutex.unlock();

	return hit;
}

#ifdef ENABLE_QRCODE_API
size_t QRCodeTracker::m_nextInstanceID = 1;

QRCodeTracker::QRCodeTracker()
{
	if (winrt::Microsoft::MixedReality::QR::QRCodeWatcher::IsSupported())
	{
		thread requestAccessThread([this]()
		{
			auto permissionResult = winrt::Microsoft::MixedReality::QR::QRCodeWatcher::RequestAccessAsync().get();
			if (permissionResult == winrt::Microsoft::MixedReality::QR::QRCodeWatcherAccessStatus::Allowed)
			{
				m_qrWatcher = winrt::Microsoft::MixedReality::QR::QRCodeWatcher();

				m_qrWatcher.Added({ this, &QRCodeTracker::OnAddedQRCode });
				m_qrWatcher.Updated({ this, &QRCodeTracker::OnUpdatedQRCode });
				m_qrWatcher.Removed({ this, &QRCodeTracker::OnRemovedQRCode });

				m_qrWatcher.Start();
				m_isActive = true;
				m_trackingStartTime = Timer::GetFileTime();
			}
		});

		requestAccessThread.detach();
	}
}

bool QRCodeTracker::IsActive()
{
	return m_isActive;
}

void QRCodeTracker::Update(winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& worldCoordinateSystem)
{
	if (!worldCoordinateSystem)
		return;

	std::lock_guard<std::mutex> lock(m_qrcodesMutex);

	m_trackedCodes.clear();
	for (auto& internalRecordPair : m_internalRecords)
	{
		QRCode& qrCode = internalRecordPair.second.trackedQRCode;

		auto tryTransform = internalRecordPair.second.coordinateSystem.TryGetTransformTo(worldCoordinateSystem);
		if (tryTransform && qrCode.lastSeenTimestamp > m_trackingStartTime)
		{
			qrCode.worldTransform = XMLoadFloat4x4(&tryTransform.Value());
			XMMATRIX localRotation = XMMatrixRotationX(XM_PI);
			XMMATRIX localTranslation = XMMatrixTranslation(qrCode.length / 2.0f, qrCode.length / 2.0f, 0.0f);
			qrCode.displayTransform = localRotation * localTranslation * qrCode.worldTransform;

			internalRecordPair.second.isLocated = true;
			m_trackedCodes.push_back(qrCode);
		}
		else
		{
			internalRecordPair.second.isLocated = false;
		}
	}
}

void QRCodeTracker::OnAddedQRCode(const winrt::Windows::Foundation::IInspectable& sender, winrt::Microsoft::MixedReality::QR::QRCodeAddedEventArgs const& args)
{
	std::lock_guard<std::mutex> lock(m_qrcodesMutex);

	QRCodeInternalRecord internalRecord;
	internalRecord.coordinateSystem = winrt::Windows::Perception::Spatial::Preview::SpatialGraphInteropPreview::CreateCoordinateSystemForNode(args.Code().SpatialGraphNodeId());

	internalRecord.trackedQRCode.instanceID = m_nextInstanceID++;
	internalRecord.trackedQRCode.value = WideStringToString(wstring(args.Code().Data()));
	internalRecord.trackedQRCode.length = args.Code().PhysicalSideLength();
	internalRecord.trackedQRCode.lastSeenTimestamp = args.Code().LastDetectedTime().time_since_epoch().count();

	m_internalRecords[args.Code().Id()] = internalRecord;
}

void QRCodeTracker::OnUpdatedQRCode(const winrt::Windows::Foundation::IInspectable& sender, winrt::Microsoft::MixedReality::QR::QRCodeUpdatedEventArgs const& args)
{
	std::lock_guard<std::mutex> lock(m_qrcodesMutex);

	auto it = m_internalRecords.find(args.Code().Id());
	if (it != m_internalRecords.end())
	{
		it->second.trackedQRCode.length = args.Code().PhysicalSideLength();
		it->second.trackedQRCode.lastSeenTimestamp = args.Code().LastDetectedTime().time_since_epoch().count();
		// Position will be polled during ::Update call.
	}
}

void QRCodeTracker::OnRemovedQRCode(const winrt::Windows::Foundation::IInspectable& sender, winrt::Microsoft::MixedReality::QR::QRCodeRemovedEventArgs const& args)
{
	std::lock_guard<std::mutex> lock(m_qrcodesMutex);

	auto it = m_internalRecords.find(args.Code().Id());
	if (it != m_internalRecords.end())
		m_internalRecords.erase(it);
}

const std::vector<QRCode>& QRCodeTracker::GetTrackedQRCodeList()
{
	std::lock_guard<std::mutex> lock(m_qrcodesMutex);
	return m_trackedCodes;
}

const QRCode& QRCodeTracker::GetTrackedQRCode(const std::string& value)
{
	std::lock_guard<std::mutex> lock(m_qrcodesMutex);

	bool found = false;
	size_t mostRecentIndex = 0;
	long long mostRecentTimestamp = 0;

	for (size_t index = 0; index < m_trackedCodes.size(); ++index)
	{
		QRCode& qrCode = m_trackedCodes[index];
		if (qrCode.value == value && qrCode.lastSeenTimestamp > mostRecentTimestamp)
		{
			found = true;
			mostRecentIndex = index;
			mostRecentTimestamp = qrCode.lastSeenTimestamp;
		}
	}

	static const QRCode blankCode;

	if (found)
		return m_trackedCodes[mostRecentIndex];
	else
		return blankCode;
}

winrt::Windows::Perception::Spatial::SpatialCoordinateSystem QRCodeTracker::GetCoordinateSystemForQRCode(size_t instanceID)
{
	for (auto& qrCodeIt : m_internalRecords)
	{
		if (qrCodeIt.second.trackedQRCode.instanceID == instanceID)
		{
			return qrCodeIt.second.coordinateSystem;
		}
	}

	return nullptr;
}

#endif
