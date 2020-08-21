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

#pragma once

#include "researchmode\ResearchModeApi.h"
#include "Tar.h"
#include "TimeConverter.h"

#include <mutex>
#include <winrt/Windows.Perception.Spatial.h>
#include <winrt/Windows.Perception.Spatial.Preview.h>


// Struct to store per-frame rig2world transformations
// See also https://docs.microsoft.com/en-us/windows/mixed-reality/locatable-camera
struct FrameLocation
{
	long long timestamp;	
	winrt::Windows::Foundation::Numerics::float4x4 rigToWorldtransform;
};


class RMCameraReader
{
public:
	RMCameraReader(IResearchModeSensor* pLLSensor, HANDLE camConsentGiven, ResearchModeSensorConsent* camAccessConsent, const GUID& guid)
	{
		m_pRMSensor = pLLSensor;
		m_pRMSensor->AddRef();
		m_pSensorFrame = nullptr;

		// Get GUID identifying the rigNode to
		// initialize the SpatialLocator
		SetLocator(guid);
		// Reserve for 10 seconds at 30fps (holds for VLC)
		m_frameLocations.reserve(10 * 30);

		m_pCameraUpdateThread = new std::thread(CameraUpdateThread, this, camConsentGiven, camAccessConsent);
		m_pWriteThread = new std::thread(CameraWriteThread, this);
	}

	void SetStorageFolder(const winrt::Windows::Storage::StorageFolder& storageFolder);
	void SetWorldCoordSystem(const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& coordSystem);
	void ResetStorageFolder();	

	virtual ~RMCameraReader()
	{
		m_fExit = true;
		m_pCameraUpdateThread->join();

		if (m_pRMSensor)
		{
			m_pRMSensor->CloseStream();
			m_pRMSensor->Release();
		}

		m_pWriteThread->join();
	}	

protected:
	// Thread for retrieving frames
	static void CameraUpdateThread(RMCameraReader* pReader, HANDLE camConsentGiven, ResearchModeSensorConsent* camAccessConsent);
	// Thread for writing frames to disk
	static void CameraWriteThread(RMCameraReader* pReader);

	bool IsNewTimestamp(IResearchModeSensorFrame* pSensorFrame);

	void SaveFrame(IResearchModeSensorFrame* pSensorFrame);
	void SaveVLC(IResearchModeSensorFrame* pSensorFrame, IResearchModeSensorVLCFrame* pVLCFrame);
	void SaveDepth(IResearchModeSensorFrame* pSensorFrame, IResearchModeSensorDepthFrame* pDepthFrame);

	void DumpCalibration();

	void SetLocator(const GUID& guid);
	bool AddFrameLocation();
	void DumpFrameLocations();

	// Mutex to access sensor frame
	std::mutex m_sensorFrameMutex;
	IResearchModeSensor* m_pRMSensor = nullptr;
	IResearchModeSensorFrame* m_pSensorFrame = nullptr;

	bool m_fExit = false;
	std::thread* m_pCameraUpdateThread;
	std::thread* m_pWriteThread;
	
	// Mutex to access storage folder
	std::mutex m_storageMutex;
	// conditional variable to enable / disable writing to disk
	std::condition_variable m_storageCondVar;	
	winrt::Windows::Storage::StorageFolder m_storageFolder = nullptr;
	std::unique_ptr<Io::Tarball> m_tarball;

	TimeConverter m_converter;
	UINT64 m_prevTimestamp = 0;

	winrt::Windows::Perception::Spatial::SpatialLocator m_locator = nullptr;
	winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_worldCoordSystem = nullptr;
	std::vector<FrameLocation> m_frameLocations;	
};
