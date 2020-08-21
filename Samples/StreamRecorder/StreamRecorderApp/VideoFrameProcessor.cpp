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

#include "VideoFrameProcessor.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <fstream>

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Media::Capture::Frames;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage;

const int VideoFrameProcessor::kImageWidth = 760;
const wchar_t VideoFrameProcessor::kSensorName[3] = L"PV";

winrt::Windows::Foundation::IAsyncAction VideoFrameProcessor::InitializeAsync()
{
    auto mediaFrameSourceGroups{ co_await MediaFrameSourceGroup::FindAllAsync() };

    MediaFrameSourceGroup selectedSourceGroup = nullptr;
    MediaCaptureVideoProfile profile = nullptr;
    MediaCaptureVideoProfileMediaDescription desc = nullptr;
    std::vector<MediaFrameSourceInfo> selectedSourceInfos;

    // Find MediaFrameSourceGroup
    for (const MediaFrameSourceGroup& mediaFrameSourceGroup : mediaFrameSourceGroups)
    {
        auto knownProfiles = MediaCapture::FindKnownVideoProfiles(mediaFrameSourceGroup.Id(), KnownVideoProfile::VideoConferencing);
        for (const auto& knownProfile : knownProfiles)
        {
            for (auto knownDesc : knownProfile.SupportedRecordMediaDescription())
            {
                if ((knownDesc.Width() == kImageWidth)) // && (std::round(knownDesc.FrameRate()) == 15))
                {
                    profile = knownProfile;
                    desc = knownDesc;
                    selectedSourceGroup = mediaFrameSourceGroup;
                    break;
                }
            }
        }
    }

    winrt::check_bool(selectedSourceGroup != nullptr);

    for (auto sourceInfo : selectedSourceGroup.SourceInfos())
    {
        // Workaround since multiple Color sources can be found,
        // and not all of them are necessarily compatible with the selected video profile
        if (sourceInfo.SourceKind() == MediaFrameSourceKind::Color)
        {
            selectedSourceInfos.push_back(sourceInfo);
        }
    }
    winrt::check_bool(!selectedSourceInfos.empty());
    
    // Initialize a MediaCapture object
    MediaCaptureInitializationSettings settings;
    settings.VideoProfile(profile);
    settings.RecordMediaDescription(desc);
    settings.VideoDeviceId(selectedSourceGroup.Id());
    settings.StreamingCaptureMode(StreamingCaptureMode::Video);
    settings.MemoryPreference(MediaCaptureMemoryPreference::Cpu);
    settings.SharingMode(MediaCaptureSharingMode::ExclusiveControl);
    settings.SourceGroup(selectedSourceGroup);

    MediaCapture mediaCapture = MediaCapture();
    co_await mediaCapture.InitializeAsync(settings);

    MediaFrameSource selectedSource = nullptr;
    MediaFrameFormat preferredFormat = nullptr;

    for (MediaFrameSourceInfo sourceInfo : selectedSourceInfos)
    {
        auto tmpSource = mediaCapture.FrameSources().Lookup(sourceInfo.Id());
        for (MediaFrameFormat format : tmpSource.SupportedFormats())
        {
            if (format.VideoFormat().Width() == kImageWidth)
            {
                selectedSource = tmpSource;
                preferredFormat = format;
                break;
            }
        }
    }

    winrt::check_bool(preferredFormat != nullptr);

    co_await selectedSource.SetFormatAsync(preferredFormat);
    auto mediaFrameReader = co_await mediaCapture.CreateFrameReaderAsync(selectedSource);
    auto status = co_await mediaFrameReader.StartAsync();

    winrt::check_bool(status == MediaFrameReaderStartStatus::Success);

    // reserve for 10 seconds at 30fps
    m_PVFrameLog.reserve(10 * 30);
    m_pWriteThread = new std::thread(CameraWriteThread, this);

    m_OnFrameArrivedRegistration = mediaFrameReader.FrameArrived({ this, &VideoFrameProcessor::OnFrameArrived });
}

void VideoFrameProcessor::OnFrameArrived(const MediaFrameReader& sender, const MediaFrameArrivedEventArgs& args)
{
    if (MediaFrameReference frame = sender.TryAcquireLatestFrame())
    {    
        std::lock_guard<std::shared_mutex> lock(m_frameMutex);
        m_latestFrame = frame;
    }
}

void VideoFrameProcessor::Clear()
{
    std::lock_guard<std::shared_mutex> lock(m_frameMutex);
    m_PVFrameLog.clear();
}

void VideoFrameProcessor::AddLogFrame()
{
    // Lock on m_frameMutex from caller
    PVFrame frame;

    frame.timestamp = m_latestTimestamp;
    frame.fx = m_latestFrame.VideoMediaFrame().CameraIntrinsics().FocalLength().x;
    frame.fy = m_latestFrame.VideoMediaFrame().CameraIntrinsics().FocalLength().y;

    auto PVtoWorld = m_latestFrame.CoordinateSystem().TryGetTransformTo(m_worldCoordSystem);
    if (PVtoWorld)
    {
        frame.PVtoWorldtransform = PVtoWorld.Value();
    }
    m_PVFrameLog.push_back(std::move(frame));
}

void VideoFrameProcessor::DumpFrame(const SoftwareBitmap& softwareBitmap, long long timestamp)
{        
    // Compose the output file name
    wchar_t bitmapPath[MAX_PATH];
    swprintf_s(bitmapPath, L"%lld.%s", timestamp, L"bytes");

    // Get bitmap buffer object of the frame
    BitmapBuffer bitmapBuffer = softwareBitmap.LockBuffer(BitmapBufferAccessMode::Read);

    // Get raw pointer to the buffer object
    uint32_t pixelBufferDataLength = 0;
    uint8_t* pixelBufferData;

    auto spMemoryBufferByteAccess{ bitmapBuffer.CreateReference().as<::Windows::Foundation::IMemoryBufferByteAccess>() };
    winrt::check_hresult(spMemoryBufferByteAccess->GetBuffer(&pixelBufferData, &pixelBufferDataLength));

    m_tarball->AddFile(bitmapPath, &pixelBufferData[0], pixelBufferDataLength);    
}

bool VideoFrameProcessor::DumpDataToDisk(const StorageFolder& folder, const std::wstring& datetime_path)
{
    auto path = folder.Path().data();
    std::wstring fullName(path);
    fullName += L"\\" + datetime_path + L"_pv.txt";
    std::ofstream file(fullName);
    if (!file)
    {
        return false;
    }
    
    std::lock_guard<std::shared_mutex> lock(m_frameMutex);
    // assuming this is called at the end of the capture session, and m_latestFrame is not nullptr
    assert(m_latestFrame != nullptr); 
    file << m_latestFrame.VideoMediaFrame().CameraIntrinsics().PrincipalPoint().x << "," << m_latestFrame.VideoMediaFrame().CameraIntrinsics().PrincipalPoint().y << ","
         << m_latestFrame.VideoMediaFrame().CameraIntrinsics().ImageWidth() << "," << m_latestFrame.VideoMediaFrame().CameraIntrinsics().ImageHeight() << "\n";
    
    for (const PVFrame& frame : m_PVFrameLog)
    {
        file << frame.timestamp << ",";
        file << frame.fx << "," << frame.fy << ",";
        file << frame.PVtoWorldtransform.m11 << "," << frame.PVtoWorldtransform.m21 << "," << frame.PVtoWorldtransform.m31 << "," << frame.PVtoWorldtransform.m41 << ","
            << frame.PVtoWorldtransform.m12 << "," << frame.PVtoWorldtransform.m22 << "," << frame.PVtoWorldtransform.m32 << "," << frame.PVtoWorldtransform.m42 << ","
            << frame.PVtoWorldtransform.m13 << "," << frame.PVtoWorldtransform.m23 << "," << frame.PVtoWorldtransform.m33 << "," << frame.PVtoWorldtransform.m43 << ","
            << frame.PVtoWorldtransform.m14 << "," << frame.PVtoWorldtransform.m24 << "," << frame.PVtoWorldtransform.m34 << "," << frame.PVtoWorldtransform.m44;
        file << "\n";
    }
    file.close();
    return true;
}

void VideoFrameProcessor::StartRecording(const StorageFolder& storageFolder, const SpatialCoordinateSystem& worldCoordSystem)
{
    std::lock_guard<std::mutex> guard(m_storageMutex);
    m_storageFolder = storageFolder;

    // Create the tarball for the image files
    wchar_t fileName[MAX_PATH] = {};
    swprintf_s(fileName, L"%s\\%s.tar", m_storageFolder.Path().data(), kSensorName);
    m_tarball.reset(new Io::Tarball(fileName));

    m_worldCoordSystem = worldCoordSystem;
}

void VideoFrameProcessor::StopRecording()
{
    std::lock_guard<std::mutex> guard(m_storageMutex);
    m_tarball.reset();
    m_storageFolder = nullptr;
}

void VideoFrameProcessor::CameraWriteThread(VideoFrameProcessor* pProcessor)
{
    while (!pProcessor->m_fExit)
    {
        std::lock_guard<std::mutex> guard(pProcessor->m_storageMutex);
        if (pProcessor->m_storageFolder != nullptr)
        {
            SoftwareBitmap softwareBitmap = nullptr;
            {
                std::lock_guard<std::shared_mutex> lock(pProcessor->m_frameMutex);
                if (pProcessor->m_latestFrame != nullptr)
                {
                    auto frame = pProcessor->m_latestFrame;                    
                    long long timestamp = pProcessor->m_converter.RelativeTicksToAbsoluteTicks(HundredsOfNanoseconds(frame.SystemRelativeTime().Value().count())).count();
                    if (timestamp != pProcessor->m_latestTimestamp)
                    {
                        softwareBitmap = SoftwareBitmap::Convert(frame.VideoMediaFrame().SoftwareBitmap(), BitmapPixelFormat::Bgra8);
                        pProcessor->m_latestTimestamp = timestamp;
                        pProcessor->AddLogFrame();
                    }
                }
            }
            // Convert and write the bitmap
            if (softwareBitmap != nullptr)
            {
                pProcessor->DumpFrame(softwareBitmap, pProcessor->m_latestTimestamp);
            }
        }
    }
}
