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

#include <winapifamily.h>
#include <wrl/client.h>

#include <intrin.h>
#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.Perception.Spatial.h>
#include <winrt/Windows.Perception.Spatial.Surfaces.h>
#include <winrt/Windows.Perception.People.h>
#include <winrt/Windows.UI.Input.Spatial.h>
#include <WindowsNumerics.h>

#ifdef ENABLE_QRCODE_API
#include <winrt/Microsoft.MixedReality.QR.h>
#endif

#include "Common/Intersectable.h"
#include "DrawCall.h"

#include <d3d11.h>
#include <DirectXMath.h>

#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <thread>
#include <mutex>

enum class SpatialButton
{
	SELECT,
	GRAB,
	MENU,
};

enum class HandJointIndex
{
	Palm,
	Wrist,
	ThumbMetacarpal,
	ThumbProximal,
	ThumbDistal,
	ThumbTip,
	IndexMetacarpal,
	IndexProximal,
	IndexIntermediate,
	IndexDistal,
	IndexTip,
	MiddleMetacarpal,
	MiddleProximal,
	MiddleIntermediate,
	MiddleDistal,
	MiddleTip,
	RingMetacarpal,
	RingProximal,
	RingIntermediate,
	RingDistal,
	RingTip,
	PinkyMetacarpal,
	PinkyProximal,
	PinkyIntermediate,
	PinkyDistal,
	PinkyTip,
	Count,
};

enum class Handedness
{
	Left,
	Right,
	None,
};

enum class InputType
{
	Controller,
	Hand,
	Other,
};

struct HandJoint
{
	XMVECTOR position;
	XMVECTOR orientation;
	float radius;
	bool trackedState;
};

struct InputSource
{
	InputSource();

	unsigned int id;
	long long lastTimestamp;

	InputType type;
	Handedness handedness;

	std::map<SpatialButton, bool> buttonStates;
	std::map<SpatialButton, bool> buttonPresses;
	std::map<SpatialButton, bool> buttonReleases;

	DirectX::XMVECTOR position;
	DirectX::XMVECTOR orientation;	// Quaternion

	DirectX::XMVECTOR rayPosition;
	DirectX::XMVECTOR rayDirection;

	std::vector<HandJoint> handJoints;
	std::vector<unsigned> handMeshIndices;
	std::vector<Mesh::Vertex> handMeshVertices;
	XMMATRIX handMeshWorldTransform;
};

struct QRCode
{
	size_t instanceID = 0;												// Unique ID for this instance of the QR code, guaranteed to be unique for app duration
	std::string value;													// The actual embedded code value for this QR code
	float length = 0.0f;												// Physical length in meters of each side of the QR code
	DirectX::XMMATRIX worldTransform = DirectX::XMMatrixIdentity();		// Transforms HeT-QR coordinate system to world (Y-down, Z-forward)
	DirectX::XMMATRIX displayTransform = DirectX::XMMatrixIdentity();	// Transforms rotated, centered qr code coordinate system to world (Y-up, Z-back)
	long long lastSeenTimestamp = 0;									// Timestamp of last detection by HeT in FILETIME ticks
};

class MixedReality
{
public:

	static bool IsAvailable();

	MixedReality();
	bool EnableMixedReality();
	bool IsEnabled();

	// In order to use Surface Mapping, you must first add the "spatialPerception" capability to your app manifest
	void EnableSurfaceMapping();
	bool IsSurfaceMappingActive();
	std::shared_ptr<class SurfaceMapping> GetSurfaceMappingInterface();

	winrt::Windows::Perception::Spatial::SpatialCoordinateSystem GetWorldCoordinateSystem();
	const winrt::Windows::Perception::Spatial::SpatialLocatability& GetLocatability() const { return m_locatability; }
    winrt::Windows::Graphics::Holographic::HolographicFrame GetHolographicFrame() const { return m_holoFrame; }
    winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference GetStationaryReferenceFrame() const { return m_referenceFrame; }

	void EnableQRCodeTracking();
	bool IsQRCodeTrackingActive();
	void DisableQRCodeTracking();
	const std::vector<QRCode>& GetTrackedQRCodeList();
	const QRCode& GetTrackedQRCode(const std::string& value);

	void SetWorldCoordinateSystemToDefault();
	void SetWorldCoordinateSystemToQRCode(const std::string& qrCodeValue);

	void Update();

	long long GetPredictedDisplayTime();
	bool GetHeadPoseAtTimestamp(long long fileTimeTimestamp, DirectX::XMVECTOR& position, DirectX::XMVECTOR& direction, DirectX::XMVECTOR& up);	// timestamp is FILETIME

	const DirectX::XMVECTOR& GetHeadPosition();
	const DirectX::XMVECTOR& GetHeadForwardDirection();
	const DirectX::XMVECTOR& GetHeadUpDirection();
	const DirectX::XMVECTOR& GetGravityDirection();

	// In order to use ET, you must first add the "gazeInput" capability to your app manifest
	bool IsEyeTrackingAvailable();	// True if system supports eye tracking
	void EnableEyeTracking();
	bool IsEyeTrackingEnabled();	// True if app has access to eye tracking and it enabled successfully
	bool IsEyeTrackingActive();		// True if eye tracking is actively tracking
	const DirectX::XMVECTOR& GetEyeGazeOrigin();
	const DirectX::XMVECTOR& GetEyeGazeDirection();

	// These functions check all input sources
	bool IsButtonDown(SpatialButton button);
	bool WasButtonPressed(SpatialButton button);
	bool WasButtonReleased(SpatialButton button);

	// The InputSource pointers returned are only guaranteed good until the next call to MixedReality::Update()
	InputSource* GetPrimarySource();	// Last source that had a button press
	InputSource* GetHand(size_t handIndex);	// 0 for left, 1 for right

	size_t CreateAnchor(const XMMATRIX& transform);	// Returns the new anchor ID, or 0 if failed
	void DeleteAnchor(size_t anchorID);
	void UpdateAnchors();
	const XMMATRIX GetAnchorWorldTransform(const size_t anchorID);
	const bool IsAnchorFound(const size_t anchorID);
	void ClearAnchors();

	size_t GetCameraPoseCount();
	void SetFocusPoint(size_t cameraPoseIndex, const DirectX::XMVECTOR& focusPoint);
	void CommitDepthBuffer(size_t cameraPoseIndex, ID3D11Texture2D* pD3DDepthTexture);
	void SetNearPlaneDistance(size_t cameraPoseIndex, float nearPlaneDistance);
	void SetFarPlaneDistance(size_t cameraPoseIndex, float farPlaneDistance);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> GetBackBuffer(size_t cameraPoseIndex);
	D3D11_VIEWPORT GetViewport(size_t cameraPoseIndex);
	bool GetViewMatrices(size_t cameraPoseIndex, DirectX::XMMATRIX& leftView, DirectX::XMMATRIX& rightView);
	void GetProjMatrices(size_t cameraPoseIndex, DirectX::XMMATRIX& leftProj, DirectX::XMMATRIX& rightProj);

	void PresentAndWait();
	void PresentAndDontWait();

private:
	winrt::Windows::Graphics::Holographic::HolographicSpace m_holoSpace{ nullptr };
	winrt::Windows::Perception::Spatial::SpatialLocator m_locator{ nullptr };
	winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference m_referenceFrame{ nullptr };
	winrt::Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference m_attachedReferenceFrame{ nullptr };
	winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_currentAttachedCoordinateSystem{ nullptr };
	winrt::Windows::Graphics::Holographic::HolographicFrame m_holoFrame{ nullptr };

	winrt::Windows::UI::Input::Spatial::SpatialInteractionManager m_spatialInteractionManager{ nullptr };
	std::map<unsigned, winrt::Windows::Perception::People::HandMeshObserver> m_activeHandMeshObservers;		// These are arranged by the ID of the corresponding input source
	std::vector<unsigned short> m_indexScratchBuffer;
	std::vector<winrt::Windows::Perception::People::HandMeshVertex> m_vertexScratchBuffer;

	std::shared_ptr<SurfaceMapping> m_surfaceMapping;

#ifdef ENABLE_QRCODE_API
	std::shared_ptr<class QRCodeTracker> m_qrCodeTracker;
#endif

	winrt::Windows::Perception::Spatial::SpatialLocatability m_locatability { winrt::Windows::Perception::Spatial::SpatialLocatability::Unavailable };
	winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_worldCoordinateSystemOverride{ nullptr };

	struct AnchorRecord
	{
		size_t anchorID = 0;
		bool isFound = false;
		XMMATRIX worldTransform = DirectX::XMMatrixIdentity();
		winrt::Windows::Perception::Spatial::SpatialAnchor anchor{ nullptr };
	};
	std::map<size_t, AnchorRecord> m_anchorRecords;
	size_t m_nextAnchorID = 1;

	void UpdateInputSource(winrt::Windows::UI::Input::Spatial::SpatialInteractionSourceState currentState);
	void OnLocatabilityChanged(winrt::Windows::Perception::Spatial::SpatialLocator const& locator, winrt::Windows::Foundation::IInspectable const&);

	bool m_mixedRealityEnabled;

	DirectX::XMVECTOR m_headPosition;
	DirectX::XMVECTOR m_headForwardDirection;
	DirectX::XMVECTOR m_headUpDirection;
	DirectX::XMVECTOR m_gravityDirection;

	std::string m_worldCoordinateSystemQRCodeValue;

	bool m_isArticulatedHandTrackingAPIAvailable;	// True if articulated hand tracking API is available

	bool m_isEyeTrackingAvailable;	// True if system supports eye tracking APIs and an eye tracking system is available
	bool m_isEyeTrackingRequested;	// True if app requested to enable eye tracking
	bool m_isEyeTrackingEnabled;	// True if eye tracking was successfully enabled
	bool m_isEyeTrackingActive;		// True if eye tracking is actively tracking (calibration available, etc)
	DirectX::XMVECTOR m_eyeGazeOrigin;
	DirectX::XMVECTOR m_eyeGazeDirection;

	unsigned int m_primarySourceID = 0;
	std::map<unsigned int, std::shared_ptr<InputSource>> m_activeSources;

	long long m_inputWaitLastFrameTimestamp;
};

enum class SurfaceDrawMode
{
	None,
	Mesh,
	Occlusion,
	Mode_Count
};

class SurfaceMapping : public Intersectable
{
public:

	// If mesh draw is enabled, this class will automatically create draw calls to go with each mesh for debug viz
	SurfaceMapping(winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference const& referenceFrame);

	// Returns true once at least one mesh has been processed
	bool IsActive();

	void SetSurfaceDrawMode(SurfaceDrawMode surfaceDrawMode);
	SurfaceDrawMode GetSurfaceDrawMode();

	void Update(const XMVECTOR& headPosition);

	unsigned GetNumberOfSurfacesInProcessingQueue();

	void DrawMeshes();

	virtual bool TestRayIntersection(XMVECTOR rayOrigin, XMVECTOR rayDirection, float& distance, XMVECTOR& normal);

private:

	struct MeshRecord
	{
		winrt::guid id;

		std::shared_ptr<Mesh> mesh;
		winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceMesh sourceMesh{ nullptr };	// This will be nullptr unless update is in progresss

		long long lastMeshUpdateTime;		// The time when this mesh was last updated with the last surface
		long long lastSurfaceUpdateTime;	// The time when the last surface was last updated by the system

		winrt::Windows::Foundation::Numerics::float4x4 worldTransform;
		XMVECTOR color;

		std::shared_ptr<DrawCall> drawCall;	// For visualization

		MeshRecord()
		{
			memset(&id, 0, sizeof(id));

			lastMeshUpdateTime = 0;
			lastSurfaceUpdateTime = 0;

			XMStoreFloat4x4(&worldTransform, XMMatrixIdentity());
			color = XMVectorZero();
		}

		void InitDrawCall()
		{
			drawCall = std::make_shared<DrawCall>("Lit_VS.cso", "Lit_PS.cso", mesh);
			drawCall->SetColor(color);
			drawCall->SetWorldTransform(XMLoadFloat4x4(&worldTransform));
		}
	};
	typedef std::pair<winrt::guid, MeshRecord> MeshRecordPair;

	winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference m_referenceFrame{ nullptr };
	winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceObserver m_surfaceObserver{ nullptr };
	winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceMeshOptions m_surfaceMeshOptions{ nullptr };

	bool m_isActive;
	SurfaceDrawMode m_surfaceDrawMode;

	XMVECTOR m_headPosition;
	std::mutex m_headPositionMutex;

	std::map<winrt::guid, MeshRecord> m_meshRecords;
	std::vector<winrt::guid> m_meshRecordIDsToErase;
	std::mutex m_meshRecordsMutex;

	unsigned m_numberOfSurfacesInProcessingQueue;
	std::mutex m_numberOfSurfacesInProcessingQueueMutex;

	std::vector<MeshRecord> m_newMeshRecords;
	std::mutex m_newMeshRecordsMutex;

	std::unique_ptr<std::thread> m_surfaceObservationThread;

	typedef std::pair<long long, winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceInfo> TimestampSurfacePair;
	void CreaterObserverIfNeeded();
	void GetLatestSurfacesToProcess(std::vector<TimestampSurfacePair>& surfacesToProcess);
	void SurfaceObservationThreadFunction();
	void ConvertMesh(winrt::Windows::Perception::Spatial::Surfaces::SpatialSurfaceMesh sourceMesh, std::shared_ptr<Mesh> destinationMesh);
};

// To use QR code tracking:
//  - Define ENABLE_QRCODE_API in your project settings (C preprocessory definitions)
//  - Add the "webcam" capability to your package.appxmanifest file
//  - Install the Microsoft.MixedReality.QR NuGet package (https://www.nuget.org/Packages/Microsoft.MixedReality.QR)
//  - Call MixedReality::EnableQRCodeTracking() when you want to turn it on

#ifdef ENABLE_QRCODE_API
class QRCodeTracker
{
public:

	friend MixedReality;

	QRCodeTracker();
	bool IsActive();

	void Update(winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& worldCoordinateSystem);

	void OnAddedQRCode(const winrt::Windows::Foundation::IInspectable& sender, winrt::Microsoft::MixedReality::QR::QRCodeAddedEventArgs const& args);
	void OnUpdatedQRCode(const winrt::Windows::Foundation::IInspectable& sender, winrt::Microsoft::MixedReality::QR::QRCodeUpdatedEventArgs const& args);
	void OnRemovedQRCode(const winrt::Windows::Foundation::IInspectable& sender, winrt::Microsoft::MixedReality::QR::QRCodeRemovedEventArgs const& args);

	const std::vector<QRCode>& GetTrackedQRCodeList();

	// Returns QR Code with most recent timestamp if multiple codes have the same value
	// Test result for success by checking if instance ID is non-zero
	const QRCode& GetTrackedQRCode(const std::string& value);

private:

	static size_t m_nextInstanceID;

	winrt::Microsoft::MixedReality::QR::QRCodeWatcher m_qrWatcher{ nullptr };
	bool m_isActive = false;
	long long m_trackingStartTime = 0;

	std::mutex m_qrcodesMutex;  // Protects data structures accessed by public-funcs & callbacks.
	std::vector<QRCode> m_trackedCodes;  // guarded by m_qrcodesMutex

	struct QRCodeInternalRecord
	{
		QRCode trackedQRCode;
		bool isLocated = false;
		winrt::Windows::Perception::Spatial::SpatialCoordinateSystem coordinateSystem{ nullptr };
	};
	std::map<winrt::guid, QRCodeInternalRecord> m_internalRecords; // guarded by m_qrcodesMutex

	winrt::Windows::Perception::Spatial::SpatialCoordinateSystem GetCoordinateSystemForQRCode(size_t instanceID);
};
#endif
