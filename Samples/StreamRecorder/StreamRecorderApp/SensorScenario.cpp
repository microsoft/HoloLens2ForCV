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

#include "SensorScenario.h"

extern "C"
HMODULE LoadLibraryA(
	LPCSTR lpLibFileName
);

static ResearchModeSensorConsent camAccessCheck;
static HANDLE camConsentGiven;

SensorScenario::SensorScenario(const std::vector<ResearchModeSensorType>& kEnabledSensorTypes):
	m_kEnabledSensorTypes(kEnabledSensorTypes)
{
}

SensorScenario::~SensorScenario()
{
	if (m_pLFCameraSensor)
	{
		m_pLFCameraSensor->Release();
	}
	if (m_pRFCameraSensor)
	{
		m_pRFCameraSensor->Release();
	}
	if (m_pLLCameraSensor)
	{
		m_pLLCameraSensor->Release();
	}
	if (m_pRRCameraSensor)
	{
		m_pRRCameraSensor->Release();
	}
	if (m_pLTSensor)
	{
		m_pLTSensor->Release();
	}
	if (m_pAHATSensor)
	{
		m_pAHATSensor->Release();
	}

	if (m_pSensorDevice)
	{
		m_pSensorDevice->EnableEyeSelection();
		m_pSensorDevice->Release();
	}

	if (m_pSensorDeviceConsent)
	{
		m_pSensorDeviceConsent->Release();
	}
}

void SensorScenario::GetRigNodeId(GUID& outGuid) const
{
	IResearchModeSensorDevicePerception* pSensorDevicePerception;
	winrt::check_hresult(m_pSensorDevice->QueryInterface(IID_PPV_ARGS(&pSensorDevicePerception)));
	winrt::check_hresult(pSensorDevicePerception->GetRigNodeId(&outGuid));
	pSensorDevicePerception->Release();
}

void SensorScenario::InitializeSensors()
{
	size_t sensorCount = 0;
	camConsentGiven = CreateEvent(nullptr, true, false, nullptr);

	// Load Research Mode library
	HMODULE hrResearchMode = LoadLibraryA("ResearchModeAPI");
	if (hrResearchMode)
	{
		typedef HRESULT(__cdecl* PFN_CREATEPROVIDER) (IResearchModeSensorDevice** ppSensorDevice);
		PFN_CREATEPROVIDER pfnCreate = reinterpret_cast<PFN_CREATEPROVIDER>(GetProcAddress(hrResearchMode, "CreateResearchModeSensorDevice"));
		if (pfnCreate)
		{
			winrt::check_hresult(pfnCreate(&m_pSensorDevice));
		}
	}

	// Manage Sensor Consent
	winrt::check_hresult(m_pSensorDevice->QueryInterface(IID_PPV_ARGS(&m_pSensorDeviceConsent)));
	winrt::check_hresult(m_pSensorDeviceConsent->RequestCamAccessAsync(SensorScenario::CamAccessOnComplete));	

	m_pSensorDevice->DisableEyeSelection();

	m_pSensorDevice->GetSensorCount(&sensorCount);
	m_sensorDescriptors.resize(sensorCount);

	m_pSensorDevice->GetSensorDescriptors(m_sensorDescriptors.data(), m_sensorDescriptors.size(), &sensorCount);

	for (auto& sensorDescriptor : m_sensorDescriptors)
	{
		if (sensorDescriptor.sensorType == LEFT_FRONT)
		{
			if (std::find(m_kEnabledSensorTypes.begin(), m_kEnabledSensorTypes.end(), LEFT_FRONT) == m_kEnabledSensorTypes.end())
			{
				continue;
			}

			winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pLFCameraSensor));
		}

		if (sensorDescriptor.sensorType == RIGHT_FRONT)
		{
			if (std::find(m_kEnabledSensorTypes.begin(), m_kEnabledSensorTypes.end(), RIGHT_FRONT) == m_kEnabledSensorTypes.end())
			{
				continue;
			}

			winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pRFCameraSensor));
		}

		if (sensorDescriptor.sensorType == LEFT_LEFT)
		{
			if (std::find(m_kEnabledSensorTypes.begin(), m_kEnabledSensorTypes.end(), LEFT_LEFT) == m_kEnabledSensorTypes.end())
			{
				continue;
			}

			winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pLLCameraSensor));
		}

		if (sensorDescriptor.sensorType == RIGHT_RIGHT)
		{
			if (std::find(m_kEnabledSensorTypes.begin(), m_kEnabledSensorTypes.end(), RIGHT_RIGHT) == m_kEnabledSensorTypes.end())
			{
				continue;
			}

			winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pRRCameraSensor));
		}

		if (sensorDescriptor.sensorType == DEPTH_LONG_THROW)
		{
			if (std::find(m_kEnabledSensorTypes.begin(), m_kEnabledSensorTypes.end(), DEPTH_LONG_THROW) == m_kEnabledSensorTypes.end())
			{
				continue;
			}

			winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pLTSensor));
		}

		if (sensorDescriptor.sensorType == DEPTH_AHAT)
		{
			if (std::find(m_kEnabledSensorTypes.begin(), m_kEnabledSensorTypes.end(), DEPTH_AHAT) == m_kEnabledSensorTypes.end())
			{
				continue;
			}
			winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pAHATSensor));
		}
	}	
}

void SensorScenario::CamAccessOnComplete(ResearchModeSensorConsent consent)
{
	camAccessCheck = consent;
	SetEvent(camConsentGiven);
}

void SensorScenario::InitializeCameraReaders()
{
	// Get RigNode id which will be used to initialize
	// the spatial locators for camera readers objects
	GUID guid;
	GetRigNodeId(guid);

	if (m_pLFCameraSensor)
	{
		auto cameraReader = std::make_shared<RMCameraReader>(m_pLFCameraSensor, camConsentGiven, &camAccessCheck, guid);
		m_cameraReaders.push_back(cameraReader);
	}

	if (m_pRFCameraSensor)
	{
		auto cameraReader = std::make_shared<RMCameraReader>(m_pRFCameraSensor, camConsentGiven, &camAccessCheck, guid);
		m_cameraReaders.push_back(cameraReader);
	}

	if (m_pLLCameraSensor)
	{
		auto cameraReader = std::make_shared<RMCameraReader>(m_pLLCameraSensor, camConsentGiven, &camAccessCheck, guid);
		m_cameraReaders.push_back(cameraReader);
	}

	if (m_pRRCameraSensor)
	{
		auto cameraReader = std::make_shared<RMCameraReader>(m_pRRCameraSensor, camConsentGiven, &camAccessCheck, guid);
		m_cameraReaders.push_back(cameraReader);
	}

	if (m_pLTSensor)
	{
		auto cameraReader = std::make_shared<RMCameraReader>(m_pLTSensor, camConsentGiven, &camAccessCheck, guid);
		m_cameraReaders.push_back(cameraReader);
	}

	if (m_pAHATSensor)
	{
		auto cameraReader = std::make_shared<RMCameraReader>(m_pAHATSensor, camConsentGiven, &camAccessCheck, guid);
		m_cameraReaders.push_back(cameraReader);
	}	
}

void SensorScenario::StartRecording(const winrt::Windows::Storage::StorageFolder& folder,
									const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& worldCoordSystem)
{
	for (int i = 0; i < m_cameraReaders.size(); ++i)
	{
		m_cameraReaders[i]->SetWorldCoordSystem(worldCoordSystem);
		m_cameraReaders[i]->SetStorageFolder(folder);		
	}
}

void SensorScenario::StopRecording()
{
	for (int i = 0; i < m_cameraReaders.size(); ++i)
	{
		m_cameraReaders[i]->ResetStorageFolder();
	}
}
