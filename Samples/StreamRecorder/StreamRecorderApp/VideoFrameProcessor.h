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

#include <MemoryBuffer.h>
#include <winrt/Windows.Media.Devices.Core.h>
#include <winrt/Windows.Media.Capture.Frames.h>
#include <winrt/Windows.Perception.Spatial.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include "Tar.h"
#include "TimeConverter.h"
#include <mutex>
#include <shared_mutex>
#include <thread>

// Struct to store per-frame PV information:
// timestamp, PV2world transform, focal length
struct PVFrame
{
    long long timestamp;
    winrt::Windows::Foundation::Numerics::float4x4 PVtoWorldtransform;
    float fx;
    float fy;    
};


class VideoFrameProcessor
{
public:
    VideoFrameProcessor()
    {
    }

    virtual ~VideoFrameProcessor()
    {
        m_fExit = true;
        m_pWriteThread->join();
    }

    void Clear();
    void AddLogFrame();
    bool DumpDataToDisk(const winrt::Windows::Storage::StorageFolder& folder, const std::wstring& datetime_path);
    void StartRecording(const winrt::Windows::Storage::StorageFolder& storageFolder, const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& worldCoordSystem);
    void StopRecording();
    winrt::Windows::Foundation::IAsyncAction InitializeAsync();

protected:
    void OnFrameArrived(const winrt::Windows::Media::Capture::Frames::MediaFrameReader& sender,        
                        const winrt::Windows::Media::Capture::Frames::MediaFrameArrivedEventArgs& args);

private:
    void DumpFrame(const winrt::Windows::Graphics::Imaging::SoftwareBitmap& softwareBitmap, long long timestamp);

    winrt::Windows::Media::Capture::Frames::MediaFrameReader m_mediaFrameReader = nullptr;
    winrt::event_token m_OnFrameArrivedRegistration;

    std::shared_mutex m_frameMutex;
    long long m_latestTimestamp = 0;
    winrt::Windows::Media::Capture::Frames::MediaFrameReference m_latestFrame = nullptr;
    std::vector<PVFrame> m_PVFrameLog;
    
    std::mutex m_storageMutex;
    winrt::Windows::Storage::StorageFolder m_storageFolder = nullptr;
    std::unique_ptr<Io::Tarball> m_tarball;

    TimeConverter m_converter;
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_worldCoordSystem = nullptr;

    // writing thread
    static void CameraWriteThread(VideoFrameProcessor* pProcessor);
    std::thread* m_pWriteThread = nullptr;
    bool m_fExit = false;

    static const int kImageWidth;
    static const wchar_t kSensorName[3];
};
