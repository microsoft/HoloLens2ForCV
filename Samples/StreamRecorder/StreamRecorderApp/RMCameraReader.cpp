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

#include "RMCameraReader.h"
#include <sstream>

using namespace winrt::Windows::Perception;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::Storage;


namespace Depth
{
    enum InvalidationMasks
    {
        Invalid = 0x80,
    };
    static constexpr UINT16 AHAT_INVALID_VALUE = 4090;
}

void RMCameraReader::CameraUpdateThread(RMCameraReader* pCameraReader, HANDLE camConsentGiven, ResearchModeSensorConsent* camAccessConsent)
{
	HRESULT hr = S_OK;

    DWORD waitResult = WaitForSingleObject(camConsentGiven, INFINITE);

    if (waitResult == WAIT_OBJECT_0)
    {
        switch (*camAccessConsent)
        {
        case ResearchModeSensorConsent::Allowed:
            OutputDebugString(L"Access is granted");
            break;
        case ResearchModeSensorConsent::DeniedBySystem:
            OutputDebugString(L"Access is denied by the system");
            hr = E_ACCESSDENIED;
            break;
        case ResearchModeSensorConsent::DeniedByUser:
            OutputDebugString(L"Access is denied by the user");
            hr = E_ACCESSDENIED;
            break;
        case ResearchModeSensorConsent::NotDeclaredByApp:
            OutputDebugString(L"Capability is not declared in the app manifest");
            hr = E_ACCESSDENIED;
            break;
        case ResearchModeSensorConsent::UserPromptRequired:
            OutputDebugString(L"Capability user prompt required");
            hr = E_ACCESSDENIED;
            break;
        default:
            OutputDebugString(L"Access is denied by the system");
            hr = E_ACCESSDENIED;
            break;
        }
    }
    else
    {
        hr = E_UNEXPECTED;
    }

    if (SUCCEEDED(hr))
    {
        hr = pCameraReader->m_pRMSensor->OpenStream();

        if (FAILED(hr))
        {
            pCameraReader->m_pRMSensor->Release();
            pCameraReader->m_pRMSensor = nullptr;
        }

        while (!pCameraReader->m_fExit && pCameraReader->m_pRMSensor)
        {
            HRESULT hr = S_OK;
            IResearchModeSensorFrame* pSensorFrame = nullptr;

            hr = pCameraReader->m_pRMSensor->GetNextBuffer(&pSensorFrame);

            if (SUCCEEDED(hr))
            {
                std::lock_guard<std::mutex> guard(pCameraReader->m_sensorFrameMutex);
                if (pCameraReader->m_pSensorFrame)
                {
                    pCameraReader->m_pSensorFrame->Release();
                }
                pCameraReader->m_pSensorFrame = pSensorFrame;
            }
        }

        if (pCameraReader->m_pRMSensor)
        {
            pCameraReader->m_pRMSensor->CloseStream();
        }
    }
}

void RMCameraReader::CameraWriteThread(RMCameraReader* pReader)
{
    while (!pReader->m_fExit)
    {
        std::unique_lock<std::mutex> storage_lock(pReader->m_storageMutex);
        if (pReader->m_storageFolder == nullptr)
        {
            pReader->m_storageCondVar.wait(storage_lock);
        }
        
        std::lock_guard<std::mutex> reader_guard(pReader->m_sensorFrameMutex);
        if (pReader->m_pSensorFrame)
        {
            if (pReader->IsNewTimestamp(pReader->m_pSensorFrame))
            {
                pReader->SaveFrame(pReader->m_pSensorFrame);
            }
        }       
    }	
}

void RMCameraReader::DumpCalibration()
{   
    // Get frame resolution (could also be stored once at the beginning of the capture)
    ResearchModeSensorResolution resolution;
    {
        std::lock_guard<std::mutex> guard(m_sensorFrameMutex);
        // Assuming we are at the end of the capture
        assert(m_pSensorFrame != nullptr);
        winrt::check_hresult(m_pSensorFrame->GetResolution(&resolution)); 
    }

    // Get camera sensor object
    IResearchModeCameraSensor* pCameraSensor = nullptr;    
    HRESULT hr = m_pRMSensor->QueryInterface(IID_PPV_ARGS(&pCameraSensor));
    winrt::check_hresult(hr);

    // Get extrinsics (rotation and translation) with respect to the rigNode
    wchar_t outputExtrinsicsPath[MAX_PATH] = {};
    swprintf_s(outputExtrinsicsPath, L"%s\\%s_extrinsics.txt", m_storageFolder.Path().data(), m_pRMSensor->GetFriendlyName());

    std::ofstream fileExtrinsics(outputExtrinsicsPath);
    DirectX::XMFLOAT4X4 cameraViewMatrix;

    pCameraSensor->GetCameraExtrinsicsMatrix(&cameraViewMatrix);

    fileExtrinsics << cameraViewMatrix.m[0][0] << "," << cameraViewMatrix.m[1][0] << "," << cameraViewMatrix.m[2][0] << "," << cameraViewMatrix.m[3][0] << "," 
                   << cameraViewMatrix.m[0][1] << "," << cameraViewMatrix.m[1][1] << "," << cameraViewMatrix.m[2][1] << "," << cameraViewMatrix.m[3][1] << "," 
                   << cameraViewMatrix.m[0][2] << "," << cameraViewMatrix.m[1][2] << "," << cameraViewMatrix.m[2][2] << "," << cameraViewMatrix.m[3][2] << "," 
                   << cameraViewMatrix.m[0][3] << "," << cameraViewMatrix.m[1][3] << "," << cameraViewMatrix.m[2][3] << "," << cameraViewMatrix.m[3][3] << "\n";
    
    fileExtrinsics.close();

    wchar_t outputPath[MAX_PATH] = {};    
    swprintf_s(outputPath, L"%s\\%s_lut.bin", m_storageFolder.Path().data(), m_pRMSensor->GetFriendlyName());
    
    float uv[2];
    float xy[2];
    std::vector<float> lutTable(size_t(resolution.Width * resolution.Height) * 3);
    auto pLutTable = lutTable.data();

    for (size_t y = 0; y < resolution.Height; y++)
    {
        uv[1] = (y + 0.5f);
        for (size_t x = 0; x < resolution.Width; x++)
        {
            uv[0] = (x + 0.5f);
            hr = pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);
            if (FAILED(hr))
            {
				*pLutTable++ = xy[0];
				*pLutTable++ = xy[1];
				*pLutTable++ = 0.f;
                continue;
            }
            float z = 1.0f;
            const float norm = sqrtf(xy[0] * xy[0] + xy[1] * xy[1] + z * z);
            const float invNorm = 1.0f / norm;
            xy[0] *= invNorm;
            xy[1] *= invNorm;
            z *= invNorm;

            // Dump LUT row
            *pLutTable++ = xy[0];
            *pLutTable++ = xy[1];
            *pLutTable++ = z;
        }
    }    
    pCameraSensor->Release();

    // Save binary LUT to disk
    std::ofstream file(outputPath, std::ios::out | std::ios::binary);
	file.write(reinterpret_cast<char*> (lutTable.data()), lutTable.size() * sizeof(float));
    file.close();
}

void RMCameraReader::DumpFrameLocations()
{
    wchar_t outputPath[MAX_PATH] = {};
    swprintf_s(outputPath, L"%s\\%s_rig2world.txt", m_storageFolder.Path().data(), m_pRMSensor->GetFriendlyName());
    
    std::ofstream file(outputPath);
    for (const FrameLocation& location : m_frameLocations)
    {
        file << location.timestamp << "," <<
            location.rigToWorldtransform.m11 << "," << location.rigToWorldtransform.m21 << "," << location.rigToWorldtransform.m31 << "," << location.rigToWorldtransform.m41 << "," <<
            location.rigToWorldtransform.m12 << "," << location.rigToWorldtransform.m22 << "," << location.rigToWorldtransform.m32 << "," << location.rigToWorldtransform.m42 << "," <<
            location.rigToWorldtransform.m13 << "," << location.rigToWorldtransform.m23 << "," << location.rigToWorldtransform.m33 << "," << location.rigToWorldtransform.m43 << "," <<
            location.rigToWorldtransform.m14 << "," << location.rigToWorldtransform.m24 << "," << location.rigToWorldtransform.m34 << "," << location.rigToWorldtransform.m44 << std::endl;
    }
    file.close();

    m_frameLocations.clear();
}

void RMCameraReader::SetLocator(const GUID& guid)
{
    m_locator = Preview::SpatialGraphInteropPreview::CreateLocatorForNode(guid);
}

bool RMCameraReader::IsNewTimestamp(IResearchModeSensorFrame* pSensorFrame)
{
    ResearchModeSensorTimestamp timestamp;
    winrt::check_hresult(pSensorFrame->GetTimeStamp(&timestamp));

    if (m_prevTimestamp == timestamp.HostTicks)
    {
        return false;
    }

    m_prevTimestamp = timestamp.HostTicks;

    return true;
}

void RMCameraReader::SetStorageFolder(const StorageFolder& storageFolder)
{
    std::lock_guard<std::mutex> storage_guard(m_storageMutex);
    m_storageFolder = storageFolder;
    wchar_t fileName[MAX_PATH] = {};    
    swprintf_s(fileName, L"%s\\%s.tar", m_storageFolder.Path().data(), m_pRMSensor->GetFriendlyName());
    m_tarball.reset(new Io::Tarball(fileName));
    m_storageCondVar.notify_all();
}

void RMCameraReader::ResetStorageFolder()
{
    std::lock_guard<std::mutex> storage_guard(m_storageMutex);
    DumpCalibration();
    DumpFrameLocations();
    m_tarball.reset();
    m_storageFolder = nullptr;
}

void RMCameraReader::SetWorldCoordSystem(const SpatialCoordinateSystem& coordSystem)
{
    m_worldCoordSystem = coordSystem;
}

std::string CreateHeader(const ResearchModeSensorResolution& resolution, int maxBitmapValue)
{
    std::string bitmapFormat = "P5"; 

    // Compose PGM header string
    std::stringstream header;
    header << bitmapFormat << "\n"
        << resolution.Width << " "
        << resolution.Height << "\n"
        << maxBitmapValue << "\n";
    return header.str();
}

void RMCameraReader::SaveDepth(IResearchModeSensorFrame* pSensorFrame, IResearchModeSensorDepthFrame* pDepthFrame)
{        
    // Get resolution (will be used for PGM header)
    ResearchModeSensorResolution resolution;    
    pSensorFrame->GetResolution(&resolution);   
            
    bool isLongThrow = (m_pRMSensor->GetSensorType() == DEPTH_LONG_THROW);

    const UINT16* pAbImage = nullptr;
    size_t outAbBufferCount = 0;
    wchar_t outputAbPath[MAX_PATH];

    const UINT16* pDepth = nullptr;
    size_t outDepthBufferCount = 0;
    wchar_t outputDepthPath[MAX_PATH];

    const BYTE* pSigma = nullptr;
    size_t outSigmaBufferCount = 0;

    HundredsOfNanoseconds timestamp = m_converter.RelativeTicksToAbsoluteTicks(HundredsOfNanoseconds((long long)m_prevTimestamp));

    if (isLongThrow)
    {
        winrt::check_hresult(pDepthFrame->GetSigmaBuffer(&pSigma, &outSigmaBufferCount));
    }

    winrt::check_hresult(pDepthFrame->GetAbDepthBuffer(&pAbImage, &outAbBufferCount));
    winrt::check_hresult(pDepthFrame->GetBuffer(&pDepth, &outDepthBufferCount));

    // Get header for AB and Depth (16 bits)
    // Prepare the data to save for AB
    const std::string abHeaderString = CreateHeader(resolution, 65535);
    swprintf_s(outputAbPath, L"%llu_ab.pgm", timestamp.count());
    std::vector<BYTE> abPgmData;
    abPgmData.reserve(abHeaderString.size() + outAbBufferCount * sizeof(UINT16));
    abPgmData.insert(abPgmData.end(), abHeaderString.c_str(), abHeaderString.c_str() + abHeaderString.size());
    
    // Prepare the data to save for Depth
    const std::string depthHeaderString = CreateHeader(resolution, 65535);
    swprintf_s(outputDepthPath, L"%llu.pgm", timestamp.count());
    std::vector<BYTE> depthPgmData;
    depthPgmData.reserve(depthHeaderString.size() + outDepthBufferCount * sizeof(UINT16));
    depthPgmData.insert(depthPgmData.end(), depthHeaderString.c_str(), depthHeaderString.c_str() + depthHeaderString.size());
    
    assert(outAbBufferCount == outDepthBufferCount);
    if (isLongThrow)
        assert(outAbBufferCount == outSigmaBufferCount);
    // Validate depth
    for (size_t i = 0; i < outAbBufferCount; ++i)
    {
        UINT16 abVal;
        UINT16 d;
        const bool invalid = isLongThrow ? ((pSigma[i] & Depth::InvalidationMasks::Invalid) > 0) :
                                           (pDepth[i] >= Depth::AHAT_INVALID_VALUE);
        if (invalid)
        {
            d = 0;
        }
        else
        {            
            d = pDepth[i];
        }

        abVal = pAbImage[i];

        abPgmData.push_back((BYTE)(abVal >> 8));
        abPgmData.push_back((BYTE)abVal);
        depthPgmData.push_back((BYTE)(d >> 8));
        depthPgmData.push_back((BYTE)d);
    }

    m_tarball->AddFile(outputAbPath, &abPgmData[0], abPgmData.size());
    m_tarball->AddFile(outputDepthPath, &depthPgmData[0], depthPgmData.size());
}

void RMCameraReader::SaveVLC(IResearchModeSensorFrame* pSensorFrame, IResearchModeSensorVLCFrame* pVLCFrame)
{        
    wchar_t outputPath[MAX_PATH];

    // Get PGM header
    int maxBitmapValue = 255;
    ResearchModeSensorResolution resolution;
    winrt::check_hresult(pSensorFrame->GetResolution(&resolution));

    const std::string headerString = CreateHeader(resolution, maxBitmapValue);

    // Compose the output file name using absolute ticks
    swprintf_s(outputPath, L"%llu.pgm", m_converter.RelativeTicksToAbsoluteTicks(HundredsOfNanoseconds(checkAndConvertUnsigned(m_prevTimestamp))).count());

    // Convert the software bitmap to raw bytes    
    std::vector<BYTE> pgmData;
    size_t outBufferCount = 0;
    const BYTE* pImage = nullptr;

    winrt::check_hresult(pVLCFrame->GetBuffer(&pImage, &outBufferCount));

    pgmData.reserve(headerString.size() + outBufferCount);
    pgmData.insert(pgmData.end(), headerString.c_str(), headerString.c_str() + headerString.size());
    pgmData.insert(pgmData.end(), pImage, pImage + outBufferCount);

    m_tarball->AddFile(outputPath, &pgmData[0], pgmData.size());
}

void RMCameraReader::SaveFrame(IResearchModeSensorFrame* pSensorFrame)
{
    AddFrameLocation();

	IResearchModeSensorVLCFrame* pVLCFrame = nullptr;
	IResearchModeSensorDepthFrame* pDepthFrame = nullptr;

    HRESULT hr = pSensorFrame->QueryInterface(IID_PPV_ARGS(&pVLCFrame));

	if (FAILED(hr))
	{
		hr = pSensorFrame->QueryInterface(IID_PPV_ARGS(&pDepthFrame));
	}

	if (pVLCFrame)
	{
		SaveVLC(pSensorFrame, pVLCFrame);
        pVLCFrame->Release();
	}

	if (pDepthFrame)
	{		
		SaveDepth(pSensorFrame, pDepthFrame);
        pDepthFrame->Release();
	}    
}

bool RMCameraReader::AddFrameLocation()
{         
    auto timestamp = PerceptionTimestampHelper::FromSystemRelativeTargetTime(HundredsOfNanoseconds(checkAndConvertUnsigned(m_prevTimestamp)));
    auto location = m_locator.TryLocateAtTimestamp(timestamp, m_worldCoordSystem);
    if (!location)
    {
        return false;
    }
    const float4x4 dynamicNodeToCoordinateSystem = make_float4x4_from_quaternion(location.Orientation()) * make_float4x4_translation(location.Position());
    auto absoluteTimestamp = m_converter.RelativeTicksToAbsoluteTicks(HundredsOfNanoseconds((long long)m_prevTimestamp)).count();
    m_frameLocations.push_back(std::move(FrameLocation{absoluteTimestamp, dynamicNodeToCoordinateSystem}));

    return true;
}
