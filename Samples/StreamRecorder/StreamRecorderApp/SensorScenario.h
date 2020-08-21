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
#include "RMCameraReader.h"


class SensorScenario
{
public:
	SensorScenario(const std::vector<ResearchModeSensorType>& kEnabledSensorTypes);
	virtual ~SensorScenario();

	void InitializeSensors();
	void InitializeCameraReaders();	
	void StartRecording(const winrt::Windows::Storage::StorageFolder& folder, const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& worldCoordSystem);
	void StopRecording();
	static void CamAccessOnComplete(ResearchModeSensorConsent consent);

private:
	void GetRigNodeId(GUID& outGuid) const;

	const std::vector<ResearchModeSensorType>& m_kEnabledSensorTypes;
	std::vector<std::shared_ptr<RMCameraReader>> m_cameraReaders;

	IResearchModeSensorDevice* m_pSensorDevice = nullptr;
	IResearchModeSensorDeviceConsent* m_pSensorDeviceConsent = nullptr;

	std::vector<ResearchModeSensorDescriptor> m_sensorDescriptors;
	// Supported RM sensors
	IResearchModeSensor* m_pLFCameraSensor = nullptr;
	IResearchModeSensor* m_pRFCameraSensor = nullptr;
	IResearchModeSensor* m_pLLCameraSensor = nullptr;
	IResearchModeSensor* m_pRRCameraSensor = nullptr;
	IResearchModeSensor* m_pLTSensor = nullptr;
	IResearchModeSensor* m_pAHATSensor = nullptr;		
};
