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

#include "AppMain.h"
#include <winrt/Windows.Foundation.h>
#include <ctime>

using namespace DirectX;
using namespace std;
using namespace winrt::Windows::Storage;

enum class ButtonID
{
	Start,
	Stop
};

// Set streams to capture
/* Supported ResearchMode streams:
{
	LEFT_FRONT,
	LEFT_LEFT,
	RIGHT_FRONT,
	RIGHT_RIGHT,
	DEPTH_AHAT,
	DEPTH_LONG_THROW
}*/
// Note that concurrent access to AHAT and Long Throw is currently not supported
std::vector<ResearchModeSensorType> AppMain::kEnabledRMStreamTypes = { ResearchModeSensorType::DEPTH_LONG_THROW };
/* Supported not-ResearchMode streams:
{
	PV,  // RGB
	EYE  // Eye gaze tracking
}*/
std::vector<StreamTypes> AppMain::kEnabledStreamTypes = { StreamTypes::PV };

AppMain::AppMain() :
	m_recording(false),
	m_currentHeight(1.0f),
	m_coordAxes("Unlit_VS.cso", "UnlitTexture_PS.cso", make_shared<Mesh>("coord_axes.obj")),
	m_qrCodeCoordAxes("Unlit_VS.cso", "UnlitTexture_PS.cso", make_shared<Mesh>("coord_axes.obj")),
	m_coordAxesTexture("coord_axes.png"),
	m_coordAxisTransform(XMMatrixIdentity()),
	m_qrCodeValue("")
{
	DrawCall::vAmbient = XMVectorSet(.25f, .25f, .25f, 1.f);
	DrawCall::vLights[0].vLightPosW = XMVectorSet(0.0f, 1.0f, 0.0f, 0.f);
	DrawCall::PushBackfaceCullingState(false);

	m_mixedReality.EnableMixedReality();
	// SurfaceMapping will be used to roughly estimate user height
	// and to estimate where the user looks at, if eye gaze tracking is enabled
	m_mixedReality.EnableSurfaceMapping();
	m_mixedReality.EnableQRCodeTracking();

	const float rootMenuHeight = 0.10f;
	XMVECTOR mainButtonSize = XMVectorSet(0.04f, 0.04f, 0.015f, 0.0f);

	m_menu.HideTitleBar();
	m_menu.SetSize(XMVectorSet(0.15f, rootMenuHeight, 0.01f, 1.0f));

	m_menu.AddButton(make_shared<FloatingSlateButton>(XMVectorSet(-0.025f, 0.0f, 0.0f, 1.0f), mainButtonSize, XMVectorSet(0.0f, 0.5f, 0.0f, 1.0f), (unsigned)ButtonID::Start, this, "Start"));
	m_menu.AddButton(make_shared<FloatingSlateButton>(XMVectorSet(0.025f, 0.0f, 0.0f, 1.0f), mainButtonSize, XMVectorSet(0.5f, 0.0f, 0.0f, 1.0f), (unsigned)ButtonID::Stop, this, "Stop"));

	m_hethateyeStream.Clear();


	if (AppMain::kEnabledRMStreamTypes.size() > 0)
	{
		// Enable SensorScenario for RM
		m_scenario = std::make_unique<SensorScenario>(kEnabledRMStreamTypes);
		m_scenario->InitializeSensors();
		m_scenario->InitializeCameraReaders();
	}	

	for (int i = 0; i < kEnabledStreamTypes.size(); ++i)
	{
		if (kEnabledStreamTypes[i] == StreamTypes::PV)
		{
			m_videoFrameProcessorOperation = InitializeVideoFrameProcessorAsync();
		}
		else if (kEnabledStreamTypes[i] == StreamTypes::EYE)
		{
			m_mixedReality.EnableEyeTracking();
		}
	}
}

void AppMain::Update()
{
	float frameDelta = m_frameDeltaTimer.GetTime();
	m_frameDeltaTimer.Reset();

	m_mixedReality.Update();
	m_hands.UpdateFromMixedReality(m_mixedReality);

	auto startButton = m_menu.GetButton((unsigned)ButtonID::Start);
	auto stopButton = m_menu.GetButton((unsigned)ButtonID::Stop);
	startButton->SetDisabled(m_recording || (!IsVideoFrameProcessorWantedAndReady()));
	stopButton->SetDisabled(!m_recording);

	const XMVECTOR headPosition = m_mixedReality.GetHeadPosition();
	const XMVECTOR headForward = m_mixedReality.GetHeadForwardDirection();
	const XMVECTOR headUp = m_mixedReality.GetHeadUpDirection();
	const XMVECTOR headRight = XMVector3Cross(headUp, -headForward);

	m_menu.SetPosition(headPosition + headForward * 0.5f);
	m_menu.SetRotation(-headForward, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	m_menu.Update(frameDelta, m_hands);

	// Look for a QR code in the scene and get its transformation, if we didn't find one yet;
	// but do not change the world coordinate system if we're recording already.
	// Note that, with the current implementation, after finding a QRCode we'll not look
	// for new codes anymore. This behavior should be changed if it's not the expected one
	if (!m_recording && !IsQRCodeDetected())
	{
		std::vector<QRCode> qrcodes = m_mixedReality.GetTrackedQRCodeList();
		if (!qrcodes.empty())
		{
			m_qrCodeValue = qrcodes[0].value;
			m_qrCodeCoordAxes.SetWorldTransform(XMMatrixMultiply(XMMatrixScaling(0.1f, 0.1f, 0.1f), qrcodes[0].worldTransform));
			m_qrCodeTransform = qrcodes[0].worldTransform;
		}
	}

	if (m_recording)
	{		
		HeTHaTEyeFrame frame;
		// Get head transform
		frame.headTransform = m_hands.GetHeadTransform();
		// Get hand joints transforms
		for (int j = 0; j < (int)HandJointIndex::Count; ++j)
		{
			frame.leftHandTransform[j] = m_hands.GetOrientedJoint(0, HandJointIndex(j));
			frame.rightHandTransform[j] = m_hands.GetOrientedJoint(1, HandJointIndex(j));
		}
		// Check if hands are tracked
		frame.leftHandPresent = m_hands.IsHandTracked(0);
		frame.rightHandPresent = m_hands.IsHandTracked(1);
		// Get timestamp
		frame.timestamp = m_mixedReality.GetPredictedDisplayTime();

		// Get eye gaze tracking data
		if (m_mixedReality.IsEyeTrackingEnabled() && m_mixedReality.IsEyeTrackingActive())
		{
			frame.eyeGazePresent = true;
			frame.eyeGazeOrigin = m_mixedReality.GetEyeGazeOrigin();
			frame.eyeGazeDirection = m_mixedReality.GetEyeGazeDirection();
			frame.eyeGazeDistance = 0.0f;
			// Use surface mapping to compute the distance the user is looking at
			if (m_mixedReality.IsSurfaceMappingActive())
			{				
				float distance;
				XMVECTOR normal;
				if (m_mixedReality.GetSurfaceMappingInterface()->TestRayIntersection(frame.eyeGazeOrigin, frame.eyeGazeDirection, distance, normal))
				{
					frame.eyeGazeDistance = distance;
				}
			}
		}
		else
		{
			frame.eyeGazePresent = false;
		}
		m_hethateyeStream.AddFrame(std::move(frame));			
		
	}
	// Roughly estimate user height by projecting a ray from the HL to the floor
	// using surface mapping.
	// This is useful for visualization purposes at postprocessing time,
	// and can be disabled
	else if (m_mixedReality.IsSurfaceMappingActive())
	{
		// Find intersection with surface mapping mesh when casting a ray
		// from head position (projected forwards 1m) in the -y direction
		XMVECTOR normal;
		float distance;
		const XMVECTOR minusY = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
		const XMVECTOR horizontalOffset = XMVector3Normalize(headForward - XMVector3Dot(headForward, minusY) * minusY);
		const XMVECTOR projectPosition = headPosition + horizontalOffset;	
		const XMVECTOR headSide = XMVector3Normalize(XMVector3Cross(headForward, minusY));

		if (m_mixedReality.GetSurfaceMappingInterface()->TestRayIntersection(projectPosition, minusY, distance, normal))
		{
			m_currentHeight = 0.9f * m_currentHeight + 0.1f * distance;
		}

		m_coordAxisTransform = XMMatrixMultiply(
			XMMatrixTranslationFromVector(-projectPosition - m_currentHeight * minusY),
			XMMatrixLookToRH(XMVectorSet(0.0f, 0.0f, 0.f, 1.0f), horizontalOffset, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))
		);
		m_coordAxes.SetWorldTransform(XMMatrixMultiply(XMMatrixScaling(0.1f, 0.1f, 0.1f), XMMatrixInverse(nullptr, m_coordAxisTransform)));
	}

	// Show some debug info
	string debugTextString = "Current height: " + to_string(m_currentHeight) + "\n";
	debugTextString += "Surface mapping ";
	if (!m_mixedReality.IsSurfaceMappingActive()) debugTextString += "in";
	debugTextString += "active";
	debugTextString += " QRCode: ";
	if (IsQRCodeDetected())
	{
		debugTextString += m_qrCodeValue;
	}
	else
	{
		debugTextString += "None";
	}
	
	m_debugText.SetText(debugTextString);

	const XMVECTOR textPosition = headPosition + headForward * 2.0f + headUp * 0.25f + headRight * 0.3f;
	m_debugText.SetPosition(textPosition);
	m_debugText.SetForwardDirection(-headForward);
	m_debugText.SetSize(XMVectorSet(0.40f, 0.60f, 1.0f, 1.0f));
	m_debugText.SetFontSize(128.0f);
}

bool AppMain::SetDateTimePath()
{
	wchar_t m_datetime_c[200];
	const std::time_t now = std::time(nullptr);
	std::tm tm;
	if (localtime_s(&tm, &now))
	{
		return false;
	}
	if (!std::wcsftime(m_datetime_c, sizeof(m_datetime_c), L"%F-%H%M%S", &tm))
	{
		return false;
	}
	m_datetime.assign(m_datetime_c);
	return true;
}

winrt::Windows::Foundation::IAsyncAction AppMain::StartRecordingAsync()
{
	StorageFolder localFolder = ApplicationData::Current().LocalFolder();
	auto archiveSourceFolder = co_await localFolder.CreateFolderAsync(
																	L"archiveSource",
																	CreationCollisionOption::ReplaceExisting);
	if (archiveSourceFolder)
	{
		m_archiveFolder = archiveSourceFolder;

		if (m_scenario)
		{
			m_scenario->StartRecording(archiveSourceFolder, m_mixedReality.GetWorldCoordinateSystem());
		}
		if (m_videoFrameProcessor)
		{
			m_videoFrameProcessor->Clear();
			m_videoFrameProcessor->StartRecording(archiveSourceFolder, m_mixedReality.GetWorldCoordinateSystem());
		}
		m_recording = true;
	}
}

void AppMain::StopRecording()
{
	if (m_videoFrameProcessor)
	{
		m_videoFrameProcessor->StopRecording();
		m_videoFrameProcessor->DumpDataToDisk(m_archiveFolder, m_datetime);
	}
	if (m_scenario)
	{
		m_scenario->StopRecording();
	}
	
	m_recording = false;
	m_hethatStreamVis.Update(m_hethateyeStream);
	m_hethateyeStream.DumpToDisk(m_archiveFolder, m_datetime);

	if (IsQRCodeDetected())
	{
		std::wstring suffix = L"_qrcode.txt";
		m_hethateyeStream.DumpTransformToDisk(m_qrCodeTransform, m_archiveFolder, m_datetime, suffix);
	}

	winrt::hstring archiveName{m_datetime.c_str()};
	m_archiveFolder.RenameAsync(archiveName);

	m_archiveFolder = nullptr;
}

winrt::Windows::Foundation::IAsyncAction AppMain::InitializeVideoFrameProcessorAsync()
{
	if (m_videoFrameProcessorOperation &&
		m_videoFrameProcessorOperation.Status() == winrt::Windows::Foundation::AsyncStatus::Completed)
	{
		return;
	}

	m_videoFrameProcessor = make_unique<VideoFrameProcessor>();
	if (!m_videoFrameProcessor.get())
	{
		throw winrt::hresult(E_POINTER);
	}

	co_await m_videoFrameProcessor->InitializeAsync();
}

bool AppMain::IsVideoFrameProcessorWantedAndReady() const
{
	return (m_videoFrameProcessorOperation == nullptr ||
		m_videoFrameProcessorOperation.Status() == winrt::Windows::Foundation::AsyncStatus::Completed);
}

void AppMain::OnButtonPressed(FloatingSlateButton* pButton)
{
	if (pButton->GetID() == (unsigned)ButtonID::Start)
	{
		m_hethateyeStream.Clear();
		m_hethatStreamVis.Update(m_hethateyeStream);
		SetDateTimePath();
		StartRecordingAsync();
	}
	else if (pButton->GetID() == (unsigned)ButtonID::Stop)
	{		
		StopRecording();		
	}
}

void AppMain::DrawObjects()
{
	m_menu.Draw();
	m_hethatStreamVis.Draw();
	m_debugText.Render();
	m_coordAxesTexture.BindAsPixelShaderResource(0);
	m_coordAxes.Draw();
	if (IsQRCodeDetected())
	{
		m_qrCodeCoordAxes.Draw();
	}		
}

void AppMain::Render()
{
	if (m_mixedReality.IsEnabled())
	{
		DrawCall::vLights[0].vLightPosW = m_mixedReality.GetHeadPosition() + XMVectorSet(0.0f, 1.0f, 0.0f, 0.f);

		for (size_t cameraPoseIndex = 0; cameraPoseIndex < m_mixedReality.GetCameraPoseCount(); ++cameraPoseIndex)
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> d3dBackBuffer = m_mixedReality.GetBackBuffer(cameraPoseIndex);
			D3D11_VIEWPORT d3dViewport = m_mixedReality.GetViewport(cameraPoseIndex);
			DrawCall::SetBackBuffer(d3dBackBuffer.Get(), d3dViewport);

			// Default clip planes are 0.1 and 20
			static XMMATRIX leftView, rightView, leftProj, rightProj;
			m_mixedReality.GetViewMatrices(cameraPoseIndex, leftView, rightView);
			m_mixedReality.GetProjMatrices(cameraPoseIndex, leftProj, rightProj);
			DrawCall::PushView(leftView, rightView);
			DrawCall::PushProj(leftProj, rightProj);

			DrawCall::GetBackBuffer()->Clear(0.f, 0.f, 0.f, 0.f);

			DrawCall::PushRenderPass(0, DrawCall::GetBackBuffer());
			DrawObjects();
			DrawCall::PopRenderPass();

			if (!DrawCall::IsSinglePassSteroEnabled())
			{
				DrawCall::PushRightEyePass(0, DrawCall::GetBackBuffer());
				DrawObjects();
				DrawCall::PopRightEyePass();
			}

			DrawCall::PopView();
			DrawCall::PopProj();

			m_mixedReality.CommitDepthBuffer(cameraPoseIndex, DrawCall::GetBackBuffer()->GetD3DDepthStencilTexture());
		}

		m_mixedReality.PresentAndWait();
	}
	else
	{
		DrawCall::PushView(XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		DrawCall::PushProj(XM_PIDIV4, DrawCall::GetCurrentRenderTarget()->GetWidth() / (float)DrawCall::GetCurrentRenderTarget()->GetHeight(), 0.01f, 20.0f);

		DrawCall::PushRenderPass(0, DrawCall::GetBackBuffer());

		DrawCall::GetBackBuffer()->Clear(0.f, 0.f, 0.f, 0.f);
		DrawObjects();

		DrawCall::PopView();
		DrawCall::PopProj();
		DrawCall::PopRenderPass();

		DrawCall::GetD3DSwapChain()->Present(1, 0);
	}
}
