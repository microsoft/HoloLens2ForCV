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

#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"
#include "..\Common\DirectXHelper.h"

namespace BasicHologram
{

    class Texture2D
    {
    public:
        Texture2D(
            _In_ const std::shared_ptr<DX::DeviceResources>& graphics_device,
            _In_ const int32_t width,
            _In_ const int32_t height,
            _In_ const DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM);

        const Microsoft::WRL::ComPtr<ID3D11Texture2D>& GetTexture() const;

        const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& GetShaderResourceView() const;

        int32_t GetWidth() const;

        int32_t GetHeight() const;

        int32_t GetRowPitch() const
        {
            return _mappedSubresource.RowPitch;
        }

        template <typename PixelTy>
        PixelTy* MapCPUTexture(
            D3D11_MAP mapType)
        {
            return reinterpret_cast<PixelTy*>(
                MapCPUTextureCore(
                    mapType));
        }

        void UnmapCPUTexture();

        void CopyCPU2GPU();

        void CopyGPU2CPU();

    private:
        void EnsureDirectXResources();

        void* MapCPUTextureCore(
            D3D11_MAP mapType);

    private:
        const DX::DeviceResourcesPtr _deviceResources;

        const int32_t _width;
        const int32_t _height;
        const DXGI_FORMAT _format;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> _texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _sharedResourceView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> _stagingTexture;

        D3D11_MAPPED_SUBRESOURCE _mappedSubresource;
    };

    typedef std::shared_ptr<Texture2D> Texture2DPtr;
};



