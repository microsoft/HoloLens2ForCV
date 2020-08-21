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

#include <vector>
#include "../Cannon/DrawCall.h"
#include "../Cannon/MixedReality.h"

__declspec(align(16))
struct HeTHaTEyeFrame
{
    DirectX::XMMATRIX headTransform;
    std::array<DirectX::XMMATRIX, (size_t)HandJointIndex::Count> leftHandTransform;
    std::array<DirectX::XMMATRIX, (size_t)HandJointIndex::Count> rightHandTransform;
    DirectX::XMVECTOR eyeGazeOrigin;
    DirectX::XMVECTOR eyeGazeDirection;
    float eyeGazeDistance;
    bool leftHandPresent;
    bool rightHandPresent;
    bool eyeGazePresent;
    long long timestamp;
};

class HeTHaTEyeStream
{
public:
    HeTHaTEyeStream();

    void AddFrame(HeTHaTEyeFrame&& frame);
    void Clear();
    const std::vector<HeTHaTEyeFrame>& Log() const;
    size_t FrameCount() const;
    bool DumpToDisk(const winrt::Windows::Storage::StorageFolder& folder, const std::wstring& datetime_path) const;
    bool DumpTransformToDisk(const DirectX::XMMATRIX& mtx, const winrt::Windows::Storage::StorageFolder& folder,
                             const std::wstring& datetime_path, const std::wstring& suffix) const;

private:
    std::vector<HeTHaTEyeFrame> m_hethateyeLog;
};

class HeTHaTStreamVisualizer
{
public:
    HeTHaTStreamVisualizer();

    void Draw();
    void Update(const HeTHaTEyeStream& stream);

private:
    static const int kStride = 10;

    std::vector<std::shared_ptr<DrawCall>> m_drawCalls;
};
