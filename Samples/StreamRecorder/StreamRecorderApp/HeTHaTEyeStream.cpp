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

#include "HeTHaTEyeStream.h"

#include <winrt/Windows.Storage.h>
#include <fstream>
#include <iomanip>

using namespace DirectX;
using namespace winrt::Windows::Storage;

HeTHaTEyeStream::HeTHaTEyeStream()
{
    // reserve for 10 seconds at 60fps
    m_hethateyeLog.reserve(10 * 60); 
}

void HeTHaTEyeStream::AddFrame(HeTHaTEyeFrame&& frame)
{
    m_hethateyeLog.push_back(std::move(frame));
}

void HeTHaTEyeStream::Clear()
{
    m_hethateyeLog.clear();
}

size_t HeTHaTEyeStream::FrameCount() const
{
    return m_hethateyeLog.size();
}

const std::vector<HeTHaTEyeFrame>& HeTHaTEyeStream::Log() const
{
    return m_hethateyeLog;
}

std::ostream& operator<<(std::ostream& out, const XMMATRIX& m)
{
    XMFLOAT4X4 mView;
    XMStoreFloat4x4(&mView, m);
    for (int j = 0; j < 4; ++j)
    {
        for (int i = 0; i < 4; ++i)
        {
            out << std::setprecision(8) << mView.m[i][j];
            if (i < 3 || j < 3)
            {
                out << ",";
            }
        }
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const XMVECTOR& v)
{
    XMFLOAT4 mView;
    XMStoreFloat4(&mView, v);
    
    out << std::setprecision(8) << mView.x << "," << mView.y << "," << mView.z << "," << mView.w;

    return out;
}

void DumpHandIfPresentElseZero(bool present, const XMMATRIX& handTransform, std::ostream& out)
{
    static const float zeros[16] = { 0.0 };
    static const XMMATRIX zero4x4(zeros);
    if (present)
    {
        out << handTransform;
    }
    else
    {
        out << zero4x4;
    }
}

void DumpEyeGazeIfPresentElseZero(bool present, const XMVECTOR& origin, const XMVECTOR& direction, float distance, std::ostream& out)
{
    XMFLOAT4 zeros{ 0, 0, 0, 0 };
    XMVECTOR zero4 = XMLoadFloat4(&zeros);
    out << present << ",";
    if (present)
    {
        out << origin << "," << direction;
    }
    else
    {
        out << zero4 << "," << zero4;
    }
    out << "," << distance;
}

bool HeTHaTEyeStream::DumpToDisk(const StorageFolder& folder, const std::wstring& datetime_path) const
{  
    auto path = folder.Path().data();
    std::wstring fullName(path);
    fullName += +L"\\" + datetime_path + L"_head_hand_eye.csv";
    std::ofstream file(fullName);
    if (!file)
    {
        return false;
    }

    for (const HeTHaTEyeFrame& frame : m_hethateyeLog)
    {
        file << frame.timestamp << ",";
        file << frame.headTransform;
        file << ",";
        file << frame.leftHandPresent;
        for (int j = 0; j < (int)HandJointIndex::Count; ++j)
        {
            file << ",";
            DumpHandIfPresentElseZero(frame.leftHandPresent, frame.leftHandTransform[j], file);
        }
        file << ",";
        file << frame.rightHandPresent;
        for (int j = 0; j < (int)HandJointIndex::Count; ++j)
        {
            file << ",";
            DumpHandIfPresentElseZero(frame.rightHandPresent, frame.rightHandTransform[j], file);
        }
        file << ",";
        DumpEyeGazeIfPresentElseZero(frame.eyeGazePresent, frame.eyeGazeOrigin, frame.eyeGazeDirection, frame.eyeGazeDistance, file);
        file << std::endl;
    }
    file.close();
    return true;
}

bool HeTHaTEyeStream::DumpTransformToDisk(const XMMATRIX& mtx, const StorageFolder& folder, const std::wstring& datetime_path, const std::wstring& suffix) const
{
    auto path = folder.Path().data();
    std::wstring fullName(path);
    fullName += +L"\\" + datetime_path + suffix;
    std::ofstream file(fullName);
    if (!file)
    {
        return false;
    }
    file << mtx;
    file.close();
    return true;
}

HeTHaTStreamVisualizer::HeTHaTStreamVisualizer()
{
}

void HeTHaTStreamVisualizer::Draw()
{
    for (auto drawCall : m_drawCalls)
    {
        drawCall->Draw();
    }
}

void HeTHaTStreamVisualizer::Update(const HeTHaTEyeStream& stream)
{
    m_drawCalls.clear();
    
    const XMMATRIX scale = XMMatrixScaling(0.03f, 0.03f, 0.03f);
    for (int i_frame = 0; i_frame < stream.FrameCount(); i_frame += kStride)
    {
        const HeTHaTEyeFrame& frame = stream.Log()[i_frame];

        auto drawCallLeft = std::make_shared<DrawCall>("Lit_VS.cso", "Lit_PS.cso", Mesh::MT_PLANE);
        drawCallLeft->SetWorldTransform(scale * frame.leftHandTransform[(int)HandJointIndex::Palm]);
        drawCallLeft->SetColor(XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f));
        m_drawCalls.push_back(drawCallLeft);

        auto drawCallRight = std::make_shared<DrawCall>("Lit_VS.cso", "Lit_PS.cso", Mesh::MT_PLANE);
        drawCallRight->SetWorldTransform(scale * frame.rightHandTransform[(int)HandJointIndex::Palm]);
        drawCallRight->SetColor(XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
        m_drawCalls.push_back(drawCallRight);

        auto drawCallHead = std::make_shared<DrawCall>("Lit_VS.cso", "Lit_PS.cso", Mesh::MT_UIPLANE);
        drawCallHead->SetWorldTransform(scale * frame.headTransform);
        drawCallHead->SetColor(XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f));
        m_drawCalls.push_back(drawCallHead);
    }
}
