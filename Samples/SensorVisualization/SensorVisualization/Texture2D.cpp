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

#include "pch.h"
#include "Texture2D.h"

namespace BasicHologram
{
    Texture2D::Texture2D(
        _In_ const std::shared_ptr<DX::DeviceResources>& graphics_device,
        _In_ const int32_t width,
        _In_ const int32_t height,
        _In_ const DXGI_FORMAT format)
        : _deviceResources(graphics_device)
        , _width(width)
        , _height(height)
        , _format(format)
        , _mappedSubresource()
    {
        EnsureDirectXResources();
    }

    void Texture2D::EnsureDirectXResources()
    {
        auto* device = _deviceResources->GetD3DDevice();

        D3D11_TEXTURE2D_DESC desc = {};

        desc.Format = _format;
        desc.Width = _width;
        desc.Height = _height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.CPUAccessFlags = 0;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = 0;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;

        winrt::check_hresult(device->CreateTexture2D(
            &desc,
            nullptr /* pInitialData */,
            &_texture));

        winrt::check_hresult(device->CreateShaderResourceView(
            _texture.Get(),
            nullptr,
            &_sharedResourceView));

        //
        // Now, create the staging texture we'll use to either upload data
        // from CPU to GPU, or download it from GPU to CPU.
        //
        {
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;

            _deviceResources->GetD3DDevice()->CreateTexture2D(
                &desc,
                nullptr /* pInitialData */,
                &_stagingTexture);
        }
    }

    const Microsoft::WRL::ComPtr<ID3D11Texture2D>& Texture2D::GetTexture() const
    {
        return _texture;
    }

    const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& Texture2D::GetShaderResourceView() const
    {
        return _sharedResourceView;
    }

    int32_t Texture2D::GetWidth() const
    {
        return _width;
    }

    int32_t Texture2D::GetHeight() const
    {
        return _height;
    }

    void* Texture2D::MapCPUTextureCore(
        D3D11_MAP mapType)
    {
        _deviceResources->GetD3DDeviceContext()->Map(
            _stagingTexture.Get(),
            0 /* Subresource */,
            mapType,
            0 /* MapFlags */,
            &_mappedSubresource);

        return _mappedSubresource.pData;
    }

    void Texture2D::UnmapCPUTexture()
    {
        _deviceResources->GetD3DDeviceContext()->Unmap(
            _stagingTexture.Get(),
            0 /* Subresource */);

        memset(&_mappedSubresource, 0, sizeof(_mappedSubresource));
    }

    void Texture2D::CopyCPU2GPU()
    {
        _deviceResources->GetD3DDeviceContext()->CopyResource(
            _texture.Get() /* pDstResource */,
            _stagingTexture.Get() /* pSrcResource */);
    }

    void Texture2D::CopyGPU2CPU()
    {
        _deviceResources->GetD3DDeviceContext()->CopyResource(
            _stagingTexture.Get() /* pDstResource */,
            _texture.Get() /* pSrcResource */);
    }
}
