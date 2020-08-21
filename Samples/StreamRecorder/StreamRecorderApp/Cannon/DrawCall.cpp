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
// Author: Casey Meekhof cmeekhof@microsoft.com


#include "DrawCall.h"
#include "Common/FileUtilities.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <wincodec.h>

#include <cassert>

using namespace std;

Microsoft::WRL::ComPtr<ID3D11Device> g_d3dDevice;
Microsoft::WRL::ComPtr<ID3D11DeviceContext> g_d3dContext;

Microsoft::WRL::ComPtr<ID2D1Factory1> g_d2dFactory;
Microsoft::WRL::ComPtr<ID2D1Device> g_d2dDevice;
Microsoft::WRL::ComPtr<ID2D1DeviceContext> g_d2dContext;
Microsoft::WRL::ComPtr<IDWriteFactory> g_dwriteFactory;

Microsoft::WRL::ComPtr<IDXGISwapChain1> g_d3dSwapChain;

//TODO: Move these into the class
std::map<float, Microsoft::WRL::ComPtr<IDWriteTextFormat>> g_textFormats;	// Text format cache, organized by font size
Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> g_whiteBrush;
Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> g_grayBrush;
Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> g_blackBrush;

const unsigned GetBitDepthForD3DFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32_FLOAT:
		return 64;

	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_R32_FLOAT:
		return 32;

	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_UNORM:
		return 16;

	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_UNORM:
		return 8;

	default:
		return 0;
	}
}

struct DDS_PIXELFORMAT
{
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwFourCC;
	DWORD dwRGBBitCount;
	DWORD dwRBitMask;
	DWORD dwGBitMask;
	DWORD dwBBitMask;
	DWORD dwABitMask;
};

struct DDS_HEADER
{
	DWORD			dwSize;
	DWORD			dwFlags;
	DWORD			dwHeight;
	DWORD			dwWidth;
	DWORD			dwPitchOrLinearSize;
	DWORD			dwDepth;
	DWORD			dwMipMapCount;
	DWORD			dwReserved1[11];
	DDS_PIXELFORMAT ddspf;
	DWORD			dwCaps;
	DWORD			dwCaps2;
	DWORD			dwCaps3;
	DWORD			dwCaps4;
	DWORD			dwReserved2;
};

Image CreateImageUsingDDSLoader(string filename)
{
	Image image;

	FILE* pFile = OpenFile(filename, "rb");
	assert(pFile);

	unsigned magicNumber;
	fread(&magicNumber, sizeof(magicNumber), 1, pFile);
	assert(magicNumber == 0x20534444);

	DDS_HEADER header;
	fread(&header, sizeof(header), 1, pFile);

	unsigned dx10FourCC = ('D' << 24) + ('X' << 16) + ('1' << 8) + ('0' << 0);
	assert(header.ddspf.dwFourCC != dx10FourCC);

	image.width = header.dwWidth;
	image.height = header.dwHeight;
	image.rowPitch = header.dwPitchOrLinearSize;

	if (header.dwMipMapCount != 0)
		image.mipCount = header.dwMipMapCount;

	if (header.ddspf.dwFlags & 0x40)	// Contains uncompressed RGB data
	{
		if (header.ddspf.dwFlags & 0x1) // Contains alpha data
		{
			image.format = DXGI_FORMAT_B8G8R8A8_UNORM;
			image.bytesPerPixel = 4;
		}
		else
		{
			image.format = DXGI_FORMAT_B8G8R8A8_UNORM;
			image.bytesPerPixel = 4;
		}
	}

	for (unsigned i = 0; i < image.mipCount; ++i)
	{
		unsigned downsampleFactor = (unsigned) pow(2, i);

		unsigned mipDataSize = image.rowPitch / downsampleFactor * image.height / downsampleFactor;
		unique_ptr<unsigned char> mipData(new unsigned char[mipDataSize]);
		fread(mipData.get(), mipDataSize, 1, pFile);
		image.mips.push_back(move(mipData));
	}

	fclose(pFile);

	return image;
}

Image CreateImageUsingWICLoader(string filename)
{
	Image image;

	typedef HRESULT(WINAPI *CreateProxyFn)(UINT SDKVersion, IWICImagingFactory **ppIWICImagingFactory);

	IWICImagingFactory* pImageFactory = nullptr;
#if defined WINAPI_FAMILY && (WINAPI_FAMILY == WINAPI_FAMILY_TV_APP || WINAPI_FAMILY == WINAPI_FAMILY_APP)
	CoCreateInstance(CLSID_WICImagingFactory1, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (void**)&pImageFactory);
#else
	CreateProxyFn pWICCreateImagingFactory_Proxy;
	static HMODULE m = LoadLibraryW(L"WindowsCodecs.dll");
	pWICCreateImagingFactory_Proxy = (CreateProxyFn)GetProcAddress(m, "WICCreateImagingFactory_Proxy");
	(*pWICCreateImagingFactory_Proxy)(WINCODEC_SDK_VERSION2, (IWICImagingFactory**)&pImageFactory);
#endif 
	assert(pImageFactory);

	// First try filename as-is
	IWICBitmapDecoder* pDecoder = nullptr;
	wstring wideFilename = StringToWideString(filename);
	pImageFactory->CreateDecoderFromFilename(wideFilename.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
	if (!pDecoder)
	{
		// Then try the filename relative to the executable location
		wstring exePath = StringToWideString(GetExecutablePath());
		pImageFactory->CreateDecoderFromFilename((exePath + L"/" + wideFilename).c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
	}
	assert(pDecoder);	// Unsupported format or file not found

	IWICBitmapFrameDecode* pSource = nullptr;
	pDecoder->GetFrame(0, &pSource);

	pSource->GetSize(&image.width, &image.height);

	WICPixelFormatGUID wicFormat;
	pSource->GetPixelFormat(&wicFormat);
	if (wicFormat == GUID_WICPixelFormat32bppRGBA)
	{
		image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		image.bytesPerPixel = GetBitDepthForD3DFormat(image.format) / 8;
		image.rowPitch = image.width * image.bytesPerPixel;
		unique_ptr<unsigned char> data(new unsigned char[image.rowPitch * image.height]);
		pSource->CopyPixels(nullptr, image.rowPitch, image.rowPitch * image.height, data.get());
		image.mips.push_back(move(data));
	}
	else if ((wicFormat == GUID_WICPixelFormat8bppIndexed || wicFormat == GUID_WICPixelFormat8bppGray))
	{
		image.format = DXGI_FORMAT_R32_FLOAT;
		image.bytesPerPixel = GetBitDepthForD3DFormat(image.format) / 8;
		image.rowPitch = image.width * image.bytesPerPixel;
		unique_ptr<unsigned char> data(new unsigned char[image.rowPitch * image.height]);

		IWICFormatConverter* pConverter = nullptr;
		pImageFactory->CreateFormatConverter(&pConverter);
		assert(pConverter);
		pConverter->Initialize(pSource, GUID_WICPixelFormat8bppGray, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut);

		// Copy image data into buffer (have to do an intermediate copy because converting to GUID_WICPixelFormat32bppGrayFloat doesn't seem to result in the right values
		unsigned uScratchDataSize = image.width * image.height;
		unsigned char* pScratchData = new unsigned char[uScratchDataSize];
		pConverter->CopyPixels(nullptr, image.width * 1, uScratchDataSize, pScratchData);
		for (unsigned i = 0; i<image.rowPitch * image.height / 4; ++i)
		{
			((float*)data.get())[i] = (pScratchData)[i] / 255.f;
		}
		pConverter->Release();

		delete[] pScratchData;

		image.mips.push_back(move(data));
	}
	else
	{
		image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		image.bytesPerPixel = GetBitDepthForD3DFormat(image.format) / 8;
		image.rowPitch = image.width * image.bytesPerPixel;
		unique_ptr<unsigned char> data(new unsigned char[image.rowPitch * image.height]);

		IWICFormatConverter* pConverter = nullptr;
		pImageFactory->CreateFormatConverter(&pConverter);
		assert(pConverter);
		pConverter->Initialize(pSource, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut);

		pConverter->CopyPixels(nullptr, image.rowPitch, image.rowPitch * image.height, data.get());
		pConverter->Release();

		image.mips.push_back(move(data));
	}

	pSource->Release();
	pDecoder->Release();
	pImageFactory->Release();

	return image;
}

Image CreateImageFromFile(string filename)
{
	if(!FileExists(filename))
		filename = string("Media/Textures/") + filename;

	if (GetFilenameExtension(filename) == "dds")
		return CreateImageUsingDDSLoader(filename);
	else
		return CreateImageUsingWICLoader(filename);
}

//
//Texture2D
//

Texture2D::Texture2D(unsigned width, unsigned height, DXGI_FORMAT format)
{
	m_width = width;
	m_height = height;
	m_format = format;

	m_isStereo = false;

	m_mappedCount = 0;
	memset(&m_mapped, 0, sizeof(m_mapped));

	CD3D11_TEXTURE2D_DESC textureDesc(
		m_format,
		m_width,
		m_height,
		1,
		1,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

	ID3D11Texture2D* pTexture = nullptr;
	g_d3dDevice->CreateTexture2D(&textureDesc, nullptr, &pTexture);
	SetD3DTexture(pTexture);
}

Texture2D::Texture2D(ID3D11Texture2D* pD3DTexture)
{
	m_width = 0;
	m_height = 0;
	m_format = DXGI_FORMAT_UNKNOWN;

	m_isStereo = false;

	m_mappedCount = 0;
	memset(&m_mapped, 0, sizeof(m_mapped));

	SetD3DTexture(pD3DTexture);
}

Texture2D::Texture2D(string filename)
{
	m_isStereo = false;

	m_mappedCount = 0;
	memset(&m_mapped, 0, sizeof(m_mapped));

	Image image = CreateImageFromFile(filename);
	assert(image.mips[0].get());

	vector<D3D11_SUBRESOURCE_DATA> initialData;
	for (unsigned i = 0; i < image.mipCount; ++i)
	{
		unsigned downsampleFactor = (unsigned) pow(2, i);

		D3D11_SUBRESOURCE_DATA mip;
		mip.pSysMem = image.mips[i].get();
		mip.SysMemPitch = image.rowPitch / downsampleFactor;
		mip.SysMemSlicePitch = mip.SysMemPitch * image.height / downsampleFactor;
		initialData.push_back(mip);
	}

	ID3D11Texture2D* pTexture = nullptr;
	CD3D11_TEXTURE2D_DESC textureDesc(image.format, image.width, image.height, 1, image.mipCount);
	g_d3dDevice->CreateTexture2D(&textureDesc, initialData.data(), &pTexture);
	assert(pTexture);

	SetD3DTexture(pTexture);
}

void Texture2D::SetD3DTexture(ID3D11Texture2D* pD3DTexture)
{
	Reset();

	assert(pD3DTexture);
	m_texture = pD3DTexture;

	D3D11_TEXTURE2D_DESC textureDesc;
	m_texture->GetDesc(&textureDesc);

	if (textureDesc.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS)
		textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

	m_width = textureDesc.Width;
	m_height = textureDesc.Height;
	m_format = textureDesc.Format;

	m_viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float) m_width, (float) m_height);

	if (textureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		CD3D11_SHADER_RESOURCE_VIEW_DESC viewDesc(
			(textureDesc.ArraySize > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DARRAY : D3D11_SRV_DIMENSION_TEXTURE2D,
			textureDesc.Format);

		g_d3dDevice->CreateShaderResourceView(m_texture.Get(), &viewDesc, &m_shaderResourceView);
		assert(m_shaderResourceView);
	}

	if (textureDesc.BindFlags & D3D11_BIND_RENDER_TARGET)
	{
		m_isRenderTarget = true;

		const CD3D11_RENDER_TARGET_VIEW_DESC rtViewDesc(
			(textureDesc.ArraySize > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DARRAY : D3D11_RTV_DIMENSION_TEXTURE2D,
			textureDesc.Format, 0, 0, textureDesc.ArraySize);
		g_d3dDevice->CreateRenderTargetView(m_texture.Get(), &rtViewDesc, &m_renderTargetView);
		assert(m_renderTargetView);

		CD3D11_TEXTURE2D_DESC depthStencilDesc(
			DXGI_FORMAT_R16_TYPELESS,
			textureDesc.Width,
			textureDesc.Height,
			textureDesc.ArraySize,
			1,
			D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
		g_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, &m_depthStencilTexture);
		assert(m_depthStencilTexture);

		CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(
			(depthStencilDesc.ArraySize > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DARRAY : D3D11_DSV_DIMENSION_TEXTURE2D,
			DXGI_FORMAT_D16_UNORM);
		g_d3dDevice->CreateDepthStencilView(m_depthStencilTexture.Get(), &depthStencilViewDesc, &m_depthStencilView);
		assert(m_depthStencilView);

		if (textureDesc.ArraySize == 2)
		{
			m_isStereo = true;

			CD3D11_RENDER_TARGET_VIEW_DESC rightRenderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2DARRAY, textureDesc.Format, 0, 1);
			g_d3dDevice->CreateRenderTargetView(m_texture.Get(), &rightRenderTargetViewDesc, &m_renderTargetViewRight);
			assert(m_renderTargetViewRight);

			CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewRightDesc(D3D11_DSV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_D16_UNORM, 0, 1);
			g_d3dDevice->CreateDepthStencilView(m_depthStencilTexture.Get(), &depthStencilViewRightDesc, &m_depthStencilViewRight);
			assert(m_depthStencilView);

		}

		Microsoft::WRL::ComPtr<IDXGISurface2> dxgiSurface;
		m_texture.As(&dxgiSurface);
		if (dxgiSurface && (textureDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || textureDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || textureDesc.Format == DXGI_FORMAT_A8_UNORM))
		{
			D2D1_PIXEL_FORMAT pixelFormat;
			pixelFormat.format = textureDesc.Format;
			pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
			D2D1_BITMAP_PROPERTIES1 bitmapProperties;
			memset(&bitmapProperties, 0, sizeof(bitmapProperties));
			bitmapProperties.pixelFormat = pixelFormat;
			bitmapProperties.dpiX = 96.0f;
			bitmapProperties.dpiY = 96.0f;
			bitmapProperties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

			g_d2dContext->CreateBitmapFromDxgiSurface(dxgiSurface.Get(), &bitmapProperties, &m_d2dTargetBitmap);
		}
	}

	CD3D11_DEFAULT d3dDefaults;
	CD3D11_SAMPLER_DESC samplerDesc(d3dDefaults);
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

	g_d3dDevice->CreateSamplerState(&samplerDesc, &m_samplerState);
	assert(m_samplerState);
}

Texture2D::~Texture2D()
{
	Reset();
}

void Texture2D::Reset()
{
	m_width = m_height = 0;
	m_format = DXGI_FORMAT_UNKNOWN;

	m_isStereo = false;
	m_isRenderTarget = false;

	if (m_mappedCount != 0)
		Unmap();
	m_mappedCount = 0;
	memset(&m_mapped, 0, sizeof(m_mapped));
	m_currentMapType = MapType::Read;

	m_samplerState.Reset();
	m_shaderResourceView.Reset();
	m_renderTargetView.Reset();
	m_renderTargetViewRight.Reset();
	m_texture.Reset();

	m_depthStencilView.Reset();
	m_depthStencilViewRight.Reset();
	m_depthStencilTexture.Reset();

	m_stagingTexture.Reset();

	m_d2dTargetBitmap.Reset();
}

bool Texture2D::UploadData(unsigned char* pData, unsigned size)
{
	unsigned uBytesPerPixel = GetBitDepthForD3DFormat(m_format) / 8;
	assert(uBytesPerPixel != 0);
	unsigned uRequiredSize = m_width * m_height * uBytesPerPixel;
	if (size < uRequiredSize)
		return false;

	if (!m_stagingTexture)
		InitStagingTexture();

	Map(MapType::Write);

	for (unsigned uRow = 0; uRow<m_height; ++uRow)
	{
		unsigned char* pSrc = &(pData[uRow*m_width*uBytesPerPixel]);
		unsigned char* pDst = &(((unsigned char*)m_mapped.pData)[uRow*m_mapped.RowPitch]);

		memcpy(pDst, pSrc, m_width*uBytesPerPixel);
	}

	Unmap();

	return true;
}

void Texture2D::SaveToFile(string filename)
{
	IWICImagingFactory* pImageFactory = nullptr;
	CoCreateInstance(CLSID_WICImagingFactory1, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (void**)&pImageFactory);
	assert(pImageFactory);

	IWICStream* pStream = nullptr;
	pImageFactory->CreateStream(&pStream);
	assert(pStream);

	wstring wideFilename = StringToWideString(filename);
	pStream->InitializeFromFilename(wideFilename.c_str(), GENERIC_WRITE);

	IWICBitmapEncoder* pEncoder = nullptr;
	pImageFactory->CreateEncoder(GUID_ContainerFormatPng, 0, &pEncoder);
	assert(pEncoder);

	pEncoder->Initialize(pStream, WICBitmapEncoderNoCache);

	IWICBitmapFrameEncode* pFrame = nullptr;
	IPropertyBag2* pPropertyBag;
	pEncoder->CreateNewFrame(&pFrame, &pPropertyBag);
	assert(pFrame && pPropertyBag);

	pFrame->Initialize(pPropertyBag);
	pFrame->SetSize(m_width, m_height);
	pFrame->SetResolution(72, 72);

	WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
	pFrame->SetPixelFormat(&format);

	Map(MapType::Read);
	pFrame->WritePixels(m_height, GetPitch(), GetPitch() * m_height, reinterpret_cast<BYTE*>(GetDataPtr()));
	Unmap();

	pFrame->Commit();
	pEncoder->Commit();

	pFrame->Release();
	pPropertyBag->Release();
	pEncoder->Release();
	pStream->Release();
	pImageFactory->Release();
}

void Texture2D::BindAsVertexShaderResource(unsigned slot)
{
	g_d3dContext->VSSetShaderResources(slot, 1, m_shaderResourceView.GetAddressOf());
	g_d3dContext->VSSetSamplers(slot, 1, m_samplerState.GetAddressOf());
}

void Texture2D::BindAsPixelShaderResource(unsigned slot)
{
	g_d3dContext->PSSetShaderResources(slot, 1, m_shaderResourceView.GetAddressOf());
	g_d3dContext->PSSetSamplers(slot, 1, m_samplerState.GetAddressOf());
}

void Texture2D::Clear(float r, float g, float b, float a)
{
	float vColor[4] = { r, g, b, a };

	if(m_renderTargetView)
		g_d3dContext->ClearRenderTargetView(m_renderTargetView.Get(), vColor);

	if (m_depthStencilTexture)
		g_d3dContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void Texture2D::Clear(const XMVECTOR& color)
{
	Clear(XMVectorGetX(color), XMVectorGetY(color), XMVectorGetZ(color), XMVectorGetW(color));
}

void* Texture2D::Map(MapType mapType)
{
	++m_mappedCount;

	if(m_mappedCount > 1)
		return m_mapped.pData;

	if (!m_stagingTexture)
		InitStagingTexture();

	m_currentMapType = mapType;

	if (mapType == MapType::Read || mapType == MapType::ReadWrite)
	{
		g_d3dContext->CopySubresourceRegion(m_stagingTexture.Get(), 0, 0, 0, 0, m_texture.Get(), 0, nullptr);
		g_d3dContext->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &m_mapped);
	}
	else if (mapType == MapType::Write)
	{
		g_d3dContext->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ_WRITE, 0, &m_mapped);
	}

	if(m_mapped.pData)
	{
		m_mappedCount = 1;
		return m_mapped.pData;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

void Texture2D::Unmap()
{
	if (!m_texture || !m_stagingTexture)
		return;

	if(m_mappedCount == 0)
		return;

	--m_mappedCount;

	if(m_mappedCount == 0)
	{
		g_d3dContext->Unmap(m_stagingTexture.Get(), 0);

		if (m_currentMapType == MapType::Write || m_currentMapType == MapType::ReadWrite)
			g_d3dContext->CopySubresourceRegion(m_texture.Get(), 0, 0, 0, 0, m_stagingTexture.Get(), 0, nullptr);
	}
}

void* Texture2D::GetDataPtr()
{
	return m_mapped.pData;
}

unsigned Texture2D::GetPitch()
{
	return m_mapped.RowPitch;
}

const D3D11_VIEWPORT& Texture2D::GetViewport()
{
	return m_viewport;
}

void Texture2D::SetViewport(D3D11_VIEWPORT viewport)
{
	m_viewport = viewport;
}

void Texture2D::SetViewport(unsigned width, unsigned height, float minDepth, float maxDepth, float topLeftX, float topLeftY)
{
	m_viewport.Width = (float)width;
	m_viewport.Height = (float)height;
	m_viewport.MinDepth = minDepth;
	m_viewport.MaxDepth = maxDepth;
	m_viewport.TopLeftX = topLeftX;
	m_viewport.TopLeftY = topLeftY;
}

void Texture2D::EnableComparisonSampling(bool enabled)
{
	m_comparisonSamplingEnabled = enabled;

	CD3D11_DEFAULT d3dDefaults;
	CD3D11_SAMPLER_DESC samplerDesc(d3dDefaults);
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

	if (m_comparisonSamplingEnabled)
	{
		samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
	}

	g_d3dDevice->CreateSamplerState(&samplerDesc, &m_samplerState);
	assert(m_samplerState);
}

void Texture2D::InitStagingTexture()
{
	assert(g_d3dDevice);
	if(!m_stagingTexture)
	{
		D3D11_TEXTURE2D_DESC desc;
		memset(&desc, 0, sizeof(D3D11_TEXTURE2D_DESC));
		desc.ArraySize = 1;
		desc.CPUAccessFlags	 = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.Format = m_format;
		desc.Height = m_height;
		desc.Width = m_width;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;

		g_d3dDevice->CreateTexture2D(&desc, nullptr, &m_stagingTexture);
	}
	assert(m_stagingTexture);
}

vector<D3D11_INPUT_ELEMENT_DESC> g_instancedElements =
{
	{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},

	{"WORLDMATRIX_ROW", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
	{"WORLDMATRIX_ROW", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
	{"WORLDMATRIX_ROW", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
	{"WORLDMATRIX_ROW", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},
	{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1},
};

vector<D3D11_INPUT_ELEMENT_DESC> g_instancedParticleElements =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },

	{ "TRANSLATIONSCALE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
};

//
// Shader
//

Shader::Shader(ShaderType type, string filename)
{
	m_type = type;

	FILE* pFile = OpenFile(filename, "rb");
	assert(pFile);

	fseek(pFile, 0, SEEK_END);	// Might not be portable
	m_shaderBlobSize = ftell(pFile);
	rewind(pFile);

	m_shaderBlob.reset(new unsigned char[m_shaderBlobSize]);
	fread(m_shaderBlob.get(), m_shaderBlobSize, 1, pFile);
	fclose(pFile);

	if(m_type == ST_VERTEX)
	{
		g_d3dDevice->CreateVertexShader(m_shaderBlob.get(), m_shaderBlobSize, nullptr, &m_vertexShader);
		assert(m_vertexShader);
	}
	else if(m_type == ST_PIXEL)
	{
		g_d3dDevice->CreatePixelShader(m_shaderBlob.get(), m_shaderBlobSize, nullptr, &m_pixelShader);
		assert(m_pixelShader);
	}
	else if (m_type == ST_GEOMETRY)
	{
		g_d3dDevice->CreateGeometryShader(m_shaderBlob.get(), m_shaderBlobSize, nullptr, &m_geometryShader);
		assert(m_geometryShader);
	}
	else if (m_type == ST_VERTEX_SPS)
	{
		g_d3dDevice->CreateVertexShader(m_shaderBlob.get(), m_shaderBlobSize, nullptr, &m_vertexShaderSPS);
		assert(m_vertexShaderSPS);
	}

	Microsoft::WRL::ComPtr<ID3D11ShaderReflection> shaderReflect;
	D3DReflect(m_shaderBlob.get(), m_shaderBlobSize, IID_ID3D11ShaderReflection, (void**) &shaderReflect);

	D3D11_SHADER_DESC shaderDesc;
	shaderReflect->GetDesc(&shaderDesc);
	for(unsigned i=0; i<shaderDesc.ConstantBuffers; ++i)
	{
		ID3D11ShaderReflectionConstantBuffer* pBufferReflect = shaderReflect->GetConstantBufferByIndex(i);

		D3D11_SHADER_BUFFER_DESC bufferDesc;
		pBufferReflect->GetDesc(&bufferDesc);

		ConstantBuffer buffer;
		buffer.slot = i;		// Assuming slot is equal to index. This is always true as long as registers aren't manually specified in the shader.
		buffer.size = bufferDesc.Size;
		buffer.staging.reset(new unsigned char[buffer.size]);

		D3D11_BUFFER_DESC desc = {buffer.size, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, buffer.size};
		g_d3dDevice->CreateBuffer(&desc, nullptr, &buffer.d3dBuffer);

		for(unsigned j=0; j<bufferDesc.Variables; ++j)
		{
			ID3D11ShaderReflectionVariable* pVariableReflect = pBufferReflect->GetVariableByIndex(j);
			D3D11_SHADER_VARIABLE_DESC variableDesc;
			pVariableReflect->GetDesc(&variableDesc);

			D3D11_SHADER_TYPE_DESC typeDesc;
			ID3D11ShaderReflectionType* pType = pVariableReflect->GetType();
			pType->GetDesc(&typeDesc);

			Constant constant;
			constant.id = CONST_COUNT;
			constant.startOffset = variableDesc.StartOffset;
			constant.size = variableDesc.Size;
			constant.elementCount = typeDesc.Elements;

			for(ConstantID id=CONST_WORLD_MATRIX; id<CONST_COUNT; id = (ConstantID) (id+1))
			{
				if(strcmp(variableDesc.Name, g_vContantIDStrings[id]) == 0)
				{
					constant.id = id;
					break;
				}
			}

			assert(constant.id != CONST_COUNT);
			buffer.constants.push_back(constant);
		}

		m_vConstantBuffers.push_back(move(buffer));
	}
}

void Shader::Bind()
{
	if(m_type == ST_VERTEX)
		g_d3dContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
	else if(m_type == ST_PIXEL)
		g_d3dContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);
	else if (m_type == ST_GEOMETRY)
		g_d3dContext->GSSetShader(m_geometryShader.Get(), nullptr, 0);
	else if (m_type == ST_VERTEX_SPS)
		g_d3dContext->VSSetShader(m_vertexShaderSPS.Get(), nullptr, 0);
}

void* Shader::GetBytecode()
{
	if(!m_shaderBlob)
		return nullptr;

	return m_shaderBlob.get();
}

unsigned long Shader::GetBytecodeSize()
{
	return m_shaderBlobSize;
}

unsigned Shader::GetContantBufferCount()
{
	return (unsigned) m_vConstantBuffers.size();
}

ConstantBuffer& Shader::GetConstantBuffer(unsigned uIdx)
{
	return m_vConstantBuffers[uIdx];
}

//
// Mesh
//

Mesh::Mesh(MeshType type)
	: m_drawStyle(DS_TRILIST), m_d3dBuffersNeedUpdate(true), m_boundingBoxNeedsUpdate(true)
{
	if(type == MT_PLANE || type == MT_UIPLANE || type == MT_ZERO_ONE_PLANE_XY_NEGATIVE_Z_NORMAL)
		LoadPlane(type);
	else if(type == MT_BOX)
		LoadBox(1.0f, 1.0f, 1.0f);
	else if (type == MT_CYLINDER)
		LoadCylinder(0.5f, 1.0f);
	else if (type == MT_ROUNDEDBOX)
		LoadRoundedBox(1.0f, 1.0f, 1.0f);
	else if (type == MT_SPHERE)
		LoadSphere();
}

Mesh::Mesh(Mesh::Vertex* pVertices, unsigned vertexCount)
	: m_drawStyle(DS_TRILIST), m_d3dBuffersNeedUpdate(true), m_boundingBoxNeedsUpdate(true)
{
	UpdateVertices(pVertices, vertexCount);
}

Mesh::Mesh(Mesh::Vertex* pVertices, unsigned vertexCount, unsigned* pIndices, unsigned indexCount)
	: m_drawStyle(DS_TRILIST), m_d3dBuffersNeedUpdate(true), m_boundingBoxNeedsUpdate(true)
{
	UpdateVertices(pVertices, vertexCount, pIndices, indexCount);
}

Mesh::Mesh(string filename)
	: m_drawStyle(DS_TRILIST), m_d3dBuffersNeedUpdate(true), m_boundingBoxNeedsUpdate(true)
{
	if (!FileExists(filename))
		filename = string("Media/Meshes/") + filename;

	FILE* pFile = OpenFile(filename, "r");
	assert(pFile);
	if (!pFile)
		return;

	auto &vertices = GetVertices();
	vertices.clear();
	auto &indices = GetIndices();
	indices.clear();

	unsigned verticesPerPolygon = 0;
	vector<XMVECTOR> positions;
	vector<XMFLOAT2> texcoords;
	vector<XMVECTOR> normals;

	map<vector<unsigned>, unsigned> knownVertices;

	char rawLine[1024];
	stringstream line;
	while(feof(pFile) == 0)
	{
		line.clear();
		fgets(rawLine, 1024, pFile);
		line.str(rawLine);

		if(line.peek() == '#')
			continue;

		string word;
		line >> word;

		if(word == "v")
		{
			XMFLOAT4 position;
			line >> position.x >> position.y >> position.z;
			position.w = 1.0f;

			positions.push_back(XMLoadFloat4(&position));
		}

		if(word == "vn")
		{
			XMFLOAT4 normal;
			line >> normal.x >> normal.y >> normal.z;
			normal.w = 0.0f;

			normals.push_back(XMLoadFloat4(&normal));
		}

		if(word == "vt")
		{
			XMFLOAT2 texcoord;
			line >> texcoord.x >> texcoord.y;
			texcoord.y = 1-texcoord.y;	// Invert v because DX likes texcoords top-down

			texcoords.push_back(texcoord);
		}

		if(word == "f")
		{
			verticesPerPolygon = 0;

			for(;;)
			{
				line >> word;
				if (line.fail())
					break;

				word += "/";
				++verticesPerPolygon;

				size_t startIndex = 0;
				vector<unsigned> ptnSet { 0, 0, 0 };
				for (size_t i = 0; i < 3; ++i)
				{
					size_t endIndex = word.find_first_of('/', startIndex);
					if (endIndex != startIndex)
					{
						string value = word.substr(startIndex, endIndex - startIndex);
						ptnSet[i] = atoi(value.c_str());
					}
					startIndex = endIndex + 1;
				}
				
				auto iterator = knownVertices.find(ptnSet);
				if (iterator != knownVertices.end())
				{
					indices.push_back(iterator->second);
				}
				else
				{
					Mesh::Vertex vertex;
					memset(&vertex, 0, sizeof(vertex));
					if (ptnSet[0] != 0)
						vertex.position = positions[ptnSet[0] - 1];
					if (ptnSet[1] != 0)
						vertex.texcoord = texcoords[ptnSet[1] - 1];
					if (ptnSet[2] != 0)
						vertex.normal = normals[ptnSet[2] - 1];

					vertices.push_back(vertex);
					indices.push_back((unsigned) vertices.size() - 1);
					knownVertices[ptnSet] = (unsigned) vertices.size() - 1;
				}
			}
		}
	}

	fclose(pFile);

	assert(verticesPerPolygon == 3);	// Loader only handles triangle meshes

	// Invert the winding order to account for DX default (clockwise)
	for(size_t i = 0; i < indices.size(); i +=3)
	{
		unsigned temp = indices[i + 0];
		indices[i + 0] = indices[i + 2];
		indices[i + 2] = temp;
	}
}

void Mesh::LoadPlane(MeshType type)
{
	if (type != MT_PLANE && type != MT_UIPLANE && type != MT_ZERO_ONE_PLANE_XY_NEGATIVE_Z_NORMAL)
		return;

	float fWidth = 1.f;
	float fHeight = 1.f;

	float fLeft = -fWidth / 2;
	float fRight = fWidth / 2;
	float fBottom = -fHeight / 2;
	float fTop = fHeight / 2;

	auto& vertices = GetVertices();
	vertices.resize(4);
	auto& indices = GetIndices();
	indices.resize(6);

	if (type == MT_PLANE)
	{
		vertices[0].position = XMVectorSet(fLeft, 0.0f, fBottom, 1.0f);
		vertices[1].position = XMVectorSet(fRight, 0.0f, fBottom, 1.0f);
		vertices[2].position = XMVectorSet(fLeft, 0.0f, fTop, 1.0f);
		vertices[3].position = XMVectorSet(fRight, 0.0f, fTop, 1.0f);

		for (unsigned i = 0; i < 4; ++i)
			vertices[i].normal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	}
	else if (type == MT_UIPLANE)
	{
		vertices[0].position = XMVectorSet(fLeft, fTop, 0.0f, 1.0f);
		vertices[1].position = XMVectorSet(fRight, fTop, 0.0f, 1.0f);
		vertices[2].position = XMVectorSet(fLeft, fBottom, 0.0f, 1.0f);
		vertices[3].position = XMVectorSet(fRight, fBottom, 0.0f, 1.0f);

		for (unsigned i = 0; i < 4; ++i)
			vertices[i].normal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}
else if (type == MT_ZERO_ONE_PLANE_XY_NEGATIVE_Z_NORMAL)
	{
		vertices[0].position = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); 
		vertices[1].position = XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
		vertices[2].position = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
		vertices[3].position = XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f);

		for (unsigned i = 0; i < 4; ++i)
			vertices[i].normal = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
	}
	else
	{
		std::abort(); // Error^M
	}



	vertices[0].texcoord.x = 0.0f; vertices[0].texcoord.y = 0.0f;
	vertices[1].texcoord.x = 1.0f; vertices[1].texcoord.y = 0.0f;
	vertices[2].texcoord.x = 0.0f; vertices[2].texcoord.y = 1.0f;
	vertices[3].texcoord.x = 1.0f; vertices[3].texcoord.y = 1.0f;

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 3;
	indices[4] = 2;
	indices[5] = 1;
};

void Mesh::LoadBox(const float width, const float height, const float depth)
{
	float fLeft = -width / 2;
	float fRight = width / 2;

	float fBottom = -height / 2;
	float fTop = height / 2;

	float fNear = depth / 2;
	float fFar = -depth / 2;

	XMFLOAT4 ltn, rtn, rbn, lbn;
	XMFLOAT4 ltf, rtf, rbf, lbf;

	ltn.x = lbn.x = ltf.x = lbf.x = fLeft;
	rtn.x = rbn.x = rtf.x = rbf.x = fRight;

	lbn.y = lbf.y = rbn.y = rbf.y = fBottom;
	ltn.y = rtn.y = ltf.y = rtf.y = fTop;

	ltn.z = rtn.z = rbn.z = lbn.z = fNear;
	ltf.z = rtf.z = rbf.z = lbf.z = fFar;

	ltn.w = rtn.w = rbn.w = lbn.w = 1.0f;
	ltf.w = rtf.w = rbf.w = lbf.w = 1.0f;

	XMFLOAT4 a, b, c, d;
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	memset(&c, 0, sizeof(c));
	memset(&d, 0, sizeof(d));

	XMFLOAT4 normal;

	XMFLOAT2 at = { 0.f, 0.f };
	XMFLOAT2 bt = { 1.f, 0.f };
	XMFLOAT2 ct = { 1.f, 1.f };
	XMFLOAT2 dt = { 0.f, 1.f };

	m_vertices.resize(6 * 2 * 3);
	for (size_t i = 0; i < 6; ++i)
	{
		memset(&normal, 0, sizeof(normal));

		if (i == 0)	//near
		{
			a = ltn;
			b = rtn;
			c = rbn;
			d = lbn;

			normal.z = 1.f;
		}
		if (i == 1)	//right
		{
			a = rtn;
			b = rtf;
			c = rbf;
			d = rbn;

			normal.x = 1.f;
		}
		if (i == 2)	//far
		{
			a = rtf;
			b = ltf;
			c = lbf;
			d = rbf;

			normal.z = -1.f;
		}
		if (i == 3)	//left
		{
			a = ltf;
			b = ltn;
			c = lbn;
			d = lbf;

			normal.x = -1.f;
		}
		if (i == 4)	//top
		{
			a = ltf;
			b = rtf;
			c = rtn;
			d = ltn;

			normal.y = 1.f;
		}
		if (i == 5)	//bottom
		{
			a = lbn;
			b = rbn;
			c = rbf;
			d = lbf;

			normal.y = -1.f;
		}

		m_vertices[i * 6 + 0].position = XMLoadFloat4(&a);
		m_vertices[i * 6 + 1].position = XMLoadFloat4(&b);
		m_vertices[i * 6 + 2].position = XMLoadFloat4(&d);

		m_vertices[i * 6 + 3].position = XMLoadFloat4(&b);
		m_vertices[i * 6 + 4].position = XMLoadFloat4(&c);
		m_vertices[i * 6 + 5].position = XMLoadFloat4(&d);

		for (unsigned j = 0; j < 6; ++j)
			m_vertices[i * 6 + j].normal = XMLoadFloat4(&normal);

		m_vertices[i * 6 + 0].texcoord = at;
		m_vertices[i * 6 + 1].texcoord = bt;
		m_vertices[i * 6 + 2].texcoord = dt;
		m_vertices[i * 6 + 3].texcoord = bt;
		m_vertices[i * 6 + 4].texcoord = ct;
		m_vertices[i * 6 + 5].texcoord = dt;
	}

	m_indices.clear();
	for (unsigned i = 0; i < m_vertices.size(); ++i)
		m_indices.push_back(i);

	m_d3dBuffersNeedUpdate = true;
}

void Mesh::LoadCylinder(const float radius, const float height)
{
	const XMVECTOR center = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	const XMVECTOR upDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMVECTOR rightDir = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	const unsigned segmentCount = 32;

	vector<Disc> discs;
	discs.push_back({ center - upDir * height / 2.0f, upDir, rightDir, 0.0f, 0.0f });
	discs.push_back({ center - upDir * height / 2.0f, upDir, rightDir, radius, radius });
	discs.push_back({ center + upDir * height / 2.0f, upDir, rightDir, radius, radius });
	discs.push_back({ center + upDir * height / 2.0f, upDir, rightDir, 0.0f, 0.0f });

	m_vertices.clear();
	m_indices.clear();
	AppendGeometryForDiscs(discs, segmentCount);
	GenerateSmoothNormals();
}

void Mesh::LoadRoundedBox(const float width, const float height, const float depth)
{
	const XMVECTOR center = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	const XMVECTOR upDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMVECTOR rightDir = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	const unsigned segmentCount = 32;

	vector<Disc> discs;
	discs.push_back({ center - upDir * height / 2.0f, upDir, rightDir, 0.0f, 0.0f });
	discs.push_back({ center - upDir * height / 2.0f, upDir, rightDir, width / 2.0f, depth / 2.0f });
	discs.push_back({ center + upDir * height / 2.0f, upDir, rightDir, width / 2.0f, depth / 2.0f });
	discs.push_back({ center + upDir * height / 2.0f, upDir, rightDir, 0.0f, 0.0f });

	m_vertices.clear();
	m_indices.clear();
	AppendGeometryForDiscs(discs, segmentCount, DiscMode::RoundedSquare);
	GenerateSmoothNormals();
}

void Mesh::LoadSphere()
{
	const XMVECTOR center = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	const float radius = 0.5f;
	const unsigned segmentCount = 32;

	XMVECTOR yAxis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR zAxis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	m_vertices.clear();
	for (unsigned discIndex = 0; discIndex <= segmentCount; ++discIndex)
	{
		float pitchAngle = XM_PI * ((float)discIndex / (float)segmentCount);
		XMVECTOR discStartPosition = XMVector3Transform(-yAxis * radius, XMMatrixRotationAxis(zAxis, pitchAngle));
		for (unsigned segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
		{
			float yawAngle = XM_PI * 2.0f * ((float)segmentIndex / (float)segmentCount);

			Mesh::Vertex v;
			v.position = XMVector3Transform(discStartPosition, XMMatrixRotationAxis(yAxis, yawAngle));
			v.normal = XMVector3Normalize(v.position);
			v.texcoord.x = (float)segmentIndex / (float)segmentCount;
			v.texcoord.y = 1.0f - ((float)discIndex / (float)segmentCount);
			m_vertices.push_back(v);

			if (discIndex == 0 || discIndex == segmentCount)
				break;
		}
	}

	m_indices.clear();
	for (unsigned discIndex = 0; discIndex <= segmentCount; ++discIndex)
	{
		unsigned geometryLoopIndex = (discIndex > 0) ? discIndex - 1 : discIndex;
		for (unsigned segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
		{
			unsigned bottomLeft = 1 + geometryLoopIndex * segmentCount + segmentIndex;
			unsigned bottomRight = 1 + geometryLoopIndex * segmentCount + (segmentIndex + 1) % segmentCount;
			unsigned topLeft = bottomLeft + segmentCount;
			unsigned topRight = bottomRight + segmentCount;

			if (discIndex == 0)
			{
				m_indices.push_back(0);
				m_indices.push_back(bottomLeft);
				m_indices.push_back(bottomRight);
			}
			else if (discIndex == segmentCount - 1)
			{
				m_indices.push_back(bottomRight);
				m_indices.push_back(bottomLeft);
				m_indices.push_back((unsigned)m_vertices.size() - 1);
			}
			else
			{
				m_indices.push_back(bottomLeft);
				m_indices.push_back(topLeft);
				m_indices.push_back(topRight);

				m_indices.push_back(topRight);
				m_indices.push_back(bottomRight);
				m_indices.push_back(bottomLeft);
			}
		}
	}

	m_d3dBuffersNeedUpdate = true;
}

void AppendVerticesForCircleDisc(std::vector<Mesh::Vertex>& vertices, const Mesh::Disc& disc, const unsigned segmentCount)
{
	Mesh::Vertex v;
	memset(&v, 0, sizeof(v));

	XMVECTOR discEdgeStartPosition = disc.rightDir * disc.radiusRight;
	for (unsigned discEdgeIndex = 0; discEdgeIndex < segmentCount; ++discEdgeIndex)
	{
		float yawAngle = XM_PI * 2.0f * ((float)discEdgeIndex / (float)segmentCount);

		XMVECTOR localPosition = XMVector3Transform(discEdgeStartPosition, XMMatrixRotationAxis(disc.upDir, yawAngle));
		v.position = XMVectorSetW(disc.center + localPosition, 1.0f);
		vertices.push_back(v);
	}
}

void AppendVerticesForSquareDisc(std::vector<Mesh::Vertex>& vertices, const Mesh::Disc& disc, const unsigned segmentCount)
{
	const float curveRadius = disc.radiusRight * 0.2f;

	Mesh::Vertex v;
	memset(&v, 0, sizeof(v));

	XMVECTOR backDir = XMVector3Cross(disc.rightDir, disc.upDir);

	// Start in bottom right corner
	XMVECTOR centerToCorner = backDir * disc.radiusBack + disc.rightDir * disc.radiusRight;
	XMMATRIX boxCornerStepRotation = XMMatrixRotationAxis(disc.upDir, XM_PIDIV2);

	for (unsigned cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
	{
		v.position = XMVectorSetW(disc.center + centerToCorner, 1.0f);
		v.texcoord.x = XMVectorGetX(disc.rightDir * XMVector3Dot(centerToCorner, disc.rightDir) / (disc.radiusRight * 2.0f) + 0.5f * disc.rightDir);
		v.texcoord.y = XMVectorGetZ(backDir * XMVector3Dot(centerToCorner, backDir) / (disc.radiusBack * 2.0f) + 0.5f * backDir);
		vertices.push_back(v);

		centerToCorner = XMVector3TransformNormal(centerToCorner, boxCornerStepRotation);
	}
}

void AppendVerticesForRoundedSquareDisc(std::vector<Mesh::Vertex>& vertices, const Mesh::Disc& disc, const unsigned segmentCount)
{
	float curveRadius = disc.radiusRight * 0.2f;

	// Curve radius can't be bigger than either disc radius 
	if (curveRadius > disc.radiusBack)
		curveRadius = disc.radiusBack;

	Mesh::Vertex v;
	memset(&v, 0, sizeof(v));

	XMVECTOR backDir = XMVector3Cross(disc.rightDir, disc.upDir);

	// Start in bottom right corner, and back it un such that edge of the rounding circle touches the edges of the box
	XMVECTOR centerToBottom = backDir * (disc.radiusBack - curveRadius);
	XMVECTOR centerToRight = disc.rightDir * (disc.radiusRight - curveRadius);
	XMVECTOR centerToCornerCircleCenters[4] = {
		centerToBottom + centerToRight,
		-centerToBottom + centerToRight,
		-centerToBottom + -centerToRight,
		centerToBottom + -centerToRight};

	XMVECTOR circleEdgeStartPosition = backDir * curveRadius;
	XMMATRIX boxCornerStepRotation = XMMatrixRotationAxis(disc.upDir, XM_PIDIV2);

	unsigned segmentsPerCorner = segmentCount / 4;
	for (unsigned cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
	{
		for (unsigned discEdgeIndex = 0; discEdgeIndex < segmentsPerCorner; ++discEdgeIndex)
		{
			float yawAngle = XM_PIDIV2 * ((float)discEdgeIndex / ((float)segmentsPerCorner));

			XMVECTOR localPosition = centerToCornerCircleCenters[cornerIndex] + XMVector3Transform(circleEdgeStartPosition, XMMatrixRotationAxis(disc.upDir, yawAngle));
			v.position = XMVectorSetW(disc.center + localPosition, 1.0f);
			v.texcoord.x = XMVectorGetX(disc.rightDir * XMVector3Dot(localPosition, disc.rightDir) / (disc.radiusRight * 2.0f) + 0.5f * disc.rightDir);
			v.texcoord.y = XMVectorGetZ(backDir * XMVector3Dot(localPosition, backDir) / (disc.radiusBack * 2.0f) + 0.5f * backDir);
			vertices.push_back(v);
		}

		circleEdgeStartPosition = XMVector3TransformNormal(circleEdgeStartPosition, boxCornerStepRotation);
	}
}

void AppendIndicesForCylinderBody(std::vector<unsigned>& indices, const unsigned startingVertexIndex, const unsigned geometryLoopCount, const unsigned segmentCount)
{
	for (unsigned loopIndex = 0; loopIndex < geometryLoopCount - 1; ++loopIndex)
	{
		for (unsigned segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
		{
			unsigned bottomLeft = startingVertexIndex + loopIndex * segmentCount + segmentIndex;
			unsigned bottomRight = startingVertexIndex + loopIndex * segmentCount + (segmentIndex + 1) % segmentCount;
			unsigned topLeft = bottomLeft + segmentCount;
			unsigned topRight = bottomRight + segmentCount;

			indices.push_back(bottomLeft);
			indices.push_back(topLeft);
			indices.push_back(topRight);

			indices.push_back(topRight);
			indices.push_back(bottomRight);
			indices.push_back(bottomLeft);
		}
	}
}

void AppendIndicesForCylinderBeginCap(std::vector<unsigned>& indices, const unsigned startingVertexIndex, const unsigned segmentCount)
{
	for (unsigned segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
	{
		unsigned topLeft = startingVertexIndex + 1 + segmentIndex;
		unsigned topRight = startingVertexIndex + 1 + (segmentIndex + 1) % segmentCount;

		indices.push_back(startingVertexIndex);
		indices.push_back(topLeft);
		indices.push_back(topRight);
	}
}

void AppendIndicesForCylinderEndCap(std::vector<unsigned>& indices, const unsigned startingVertexIndex, const unsigned segmentCount)
{
	unsigned endingVertexIndex = startingVertexIndex + segmentCount;
	for (unsigned segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
	{
		unsigned bottomLeft = startingVertexIndex + segmentIndex;
		unsigned bottomRight = startingVertexIndex + (segmentIndex + 1) % segmentCount;

		indices.push_back(bottomRight);
		indices.push_back(bottomLeft);
		indices.push_back(endingVertexIndex);
	}
}

enum class CapType
{
	None,		// No endcap
	Flat,		// Hard edge between endcap and body
	Smooth		// Smoothed edge between endcap and body
};

void Mesh::AppendGeometryForDiscs(const std::vector<Disc>& discs, unsigned segmentCount, const DiscMode discMode)
{
	if (discs.empty())
		return;

	auto AppendVerticesFunction = (discMode == DiscMode::Circle) ? AppendVerticesForCircleDisc : 
		(discMode == DiscMode::Square) ? AppendVerticesForSquareDisc : AppendVerticesForRoundedSquareDisc;

	Vertex v;
	memset(&v, 0, sizeof(v));
	unsigned startingVertexIndex = (unsigned)m_vertices.size();

	CapType beginCapType = CapType::None;
	if (discs.size() >= 2)
	{
		// Enable begin cap if the first point has 0 size
		if (discs[0].radiusRight == 0.0f && discs[0].radiusBack == 0.0f)
		{
			// If the first and second discs are at the same spot, make the begin cap flat
			if (XMVector3Equal(discs[0].center, discs[1].center))
				beginCapType = CapType::Flat;
			else
				beginCapType = CapType::Smooth;
		}
	}

	CapType endCapType = CapType::None;
	if ((beginCapType == CapType::None && discs.size() >= 2) || (beginCapType != CapType::None && discs.size() >= 4))
	{
		// Enable end cap if the last point has 0 size
		if (discs[discs.size() - 1].radiusRight == 0.0f && discs[discs.size() - 1].radiusBack == 0.0f)
		{
			// If the last two discs at the same spot, make the end cap flat
			if (XMVector3Equal(discs[discs.size() - 1].center, discs[discs.size() - 2].center))
				endCapType = CapType::Flat;
			else
				endCapType = CapType::Smooth;
		}
	}

	unsigned geometryLoopCount = 0;
	for (size_t discIndex = 0; discIndex < discs.size(); ++discIndex)
	{
		const Disc& disc = discs[discIndex];
		if (discIndex == 0 && beginCapType != CapType::None)
		{
			v.position = XMVectorSetW(disc.center, 1.0f);
			v.texcoord = { 0.5f, 0.5f };
			m_vertices.push_back(v);
		}
		else if (discIndex == discs.size() - 1 && endCapType != CapType::None)
		{
			v.position = XMVectorSetW(disc.center, 1.0f);
			v.texcoord = { 0.5f, 0.5f };
			m_vertices.push_back(v);
		}
		else
		{
			if ((beginCapType == CapType::Flat && discIndex == 1) ||
				(endCapType == CapType::Flat && discIndex == discs.size() - 2))
			{
				AppendVerticesFunction(m_vertices, discs[discIndex], segmentCount);
			}

			AppendVerticesFunction(m_vertices, discs[discIndex], segmentCount);
			++geometryLoopCount;
		}
	}

	unsigned bodyStartIndex = startingVertexIndex;
	if (beginCapType == CapType::Flat)
	{
		AppendIndicesForCylinderBeginCap(m_indices, startingVertexIndex, segmentCount);
		bodyStartIndex += (1 + segmentCount);
	}
	else if (beginCapType == CapType::Smooth)
	{
		AppendIndicesForCylinderBeginCap(m_indices, startingVertexIndex, segmentCount);
		bodyStartIndex += 1;
	}

	AppendIndicesForCylinderBody(m_indices, bodyStartIndex, geometryLoopCount, segmentCount);

	if (endCapType == CapType::Flat)
	{
		AppendIndicesForCylinderEndCap(m_indices, bodyStartIndex + geometryLoopCount * segmentCount, segmentCount);
	}
	else if (endCapType == CapType::Smooth)
	{
		AppendIndicesForCylinderEndCap(m_indices, bodyStartIndex + (geometryLoopCount - 1) * segmentCount, segmentCount);
	}

	m_d3dBuffersNeedUpdate = true;
}

bool Mesh::SaveToFile(const std::string& filename)
{
	ofstream out(filename);
	if (!out.is_open())
		return false;

	for (auto& vertex : m_vertices)
		out << "v " << XMVectorGetX(vertex.position) << " " << XMVectorGetY(vertex.position) << " " << XMVectorGetZ(vertex.position) << "\n";
	for (auto& vertex : m_vertices)
		out << "vn " << XMVectorGetX(vertex.normal) << " " << XMVectorGetY(vertex.normal) << " " << XMVectorGetZ(vertex.normal) << "\n";
	
	// Invert winding order because Cannon is clockwise winding and obj is counter-clockwise
	for (size_t vertexIndexIndex = 0; vertexIndexIndex < m_indices.size(); vertexIndexIndex+=3)
		out << "f " << m_indices[vertexIndexIndex + 2] + 1 << "//" << m_indices[vertexIndexIndex + 2] + 1 << " " << m_indices[vertexIndexIndex + 1] + 1 << "//" << m_indices[vertexIndexIndex + 1] + 1 << " " << m_indices[vertexIndexIndex + 0] + 1 << "//" << m_indices[vertexIndexIndex + 0] + 1 << "\n";

	out.close();
	return true;
}

void Mesh::Clear()
{
	UpdateVertices(nullptr, 0, nullptr, 0);
}

void Mesh::BakeTransform(const XMMATRIX& transform)
{
	m_boundingBoxNeedsUpdate = true;
	m_d3dBuffersNeedUpdate = true;

	for (auto& vertex : m_vertices)
	{
		vertex.position = XMVector3TransformCoord(vertex.position, transform);
		vertex.normal = XMVector3Normalize(XMVector3TransformNormal(vertex.normal, transform));
	}
}

void Mesh::GenerateSmoothNormals()
{
	m_d3dBuffersNeedUpdate = true;

	for (size_t index = 0; index < m_indices.size(); index += 3)
	{
		auto& a = m_vertices[m_indices[index + 0]];
		auto& b = m_vertices[m_indices[index + 1]];
		auto& c = m_vertices[m_indices[index + 2]];

		XMVECTOR normal = XMVector3Cross(a.position - b.position, c.position - b.position);
		a.normal += normal;
		b.normal += normal;
		c.normal += normal;
	}

	for (size_t vertexIndex = 0; vertexIndex < m_vertices.size(); ++vertexIndex)
	{
		Mesh::Vertex& vertex = m_vertices[vertexIndex];
		vertex.normal = XMVector3Normalize(m_vertices[vertexIndex].normal);
	}
}

void Mesh::UpdateVertices(Vertex* pVertices, unsigned vertexCount)
{
	m_boundingBoxNeedsUpdate = true;
	m_d3dBuffersNeedUpdate = true;

	if (!pVertices)
	{
		m_vertices.clear();
		m_indices.clear();
		return;
	}

	m_vertices.resize(vertexCount);
	memcpy(m_vertices.data(), pVertices, vertexCount * sizeof(Vertex));

	m_indices.resize(vertexCount);
	for (unsigned i = 0; i < vertexCount; ++i)
		m_indices[i] = i;
}

void Mesh::UpdateVertices(Vertex* pVertices, unsigned vertexCount, unsigned* pIndices, unsigned indexCount)
{
	m_boundingBoxNeedsUpdate = true;
	m_d3dBuffersNeedUpdate = true;

	if (!pVertices || !pIndices)
	{
		m_vertices.clear();
		m_indices.clear();
		return;
	}

	m_vertices.resize(vertexCount);
	memcpy(m_vertices.data(), pVertices, vertexCount * sizeof(Vertex));

	m_indices.resize(indexCount);
	memcpy(m_indices.data(), pIndices, indexCount* sizeof(unsigned));
}

std::vector<Mesh::Vertex>& Mesh::GetVertices()
{
	m_d3dBuffersNeedUpdate = true;
	m_boundingBoxNeedsUpdate = true;

	return m_vertices;
}

std::vector<unsigned>& Mesh::GetIndices()
{
	m_d3dBuffersNeedUpdate = true;
	m_boundingBoxNeedsUpdate = true;

	return m_indices;
}

ID3D11Buffer* Mesh::GetVertexBuffer()
{
	if (m_d3dBuffersNeedUpdate)
		UpdateD3DBuffers();

	return m_d3dVertexBuffer.Get();
}

ID3D11Buffer* Mesh::GetIndexBuffer()
{
	if (m_d3dBuffersNeedUpdate)
		UpdateD3DBuffers();

	return m_d3dIndexBuffer.Get();
}

bool Mesh::BoundingBoxNode::TestRayIntersection(const vector<Vertex>& vertices, const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace,
	const XMMATRIX& worldTransform, float &distance, XMVECTOR& normalInLocalSpace,
	float maxDistance, bool returnFurthest)
{
	//	XMVector3Transform()
	BoundingBox boundingBoxInWorldSpace;
	boundingBox.Transform(boundingBoxInWorldSpace, worldTransform);
	if (!boundingBoxInWorldSpace.Intersects(rayOriginInWorldSpace, XMVector3Normalize(rayDirectionInWorldSpace), distance))
		return false;

	bool hit = false;
	float closestDistance = FLT_MAX;
	float furthestDistance = -1.0f;
	XMVECTOR returnedNormal = XMVectorZero();

	if (!children.empty())
	{
		float currentDistance = 0.0f;
		XMVECTOR currentNormal = XMVectorZero();

		for (auto& node : children)
		{
			if (node.TestRayIntersection(vertices, rayOriginInWorldSpace, rayDirectionInWorldSpace, worldTransform, currentDistance, currentNormal, maxDistance, returnFurthest))
			{
				if (!returnFurthest
					&& currentDistance < closestDistance)
				{
					closestDistance = currentDistance;
					returnedNormal = currentNormal;
					hit = true;
				}

				if (returnFurthest
					&& currentDistance > furthestDistance
					&& currentDistance <= maxDistance)
				{
					furthestDistance = currentDistance;
					returnedNormal = currentNormal;
					hit = true;
				}
			}
		}
	}
	else
	{
		float currentDistance = 0.0f;
		XMVECTOR currentNormal = XMVectorZero();

		for (size_t i = 0; i < indicesContained.size(); i += 3)
		{
			XMVECTOR v1 = XMVector3Transform(vertices[indicesContained[i + 0]].position, worldTransform);
			XMVECTOR v2 = XMVector3Transform(vertices[indicesContained[i + 1]].position, worldTransform);
			XMVECTOR v3 = XMVector3Transform(vertices[indicesContained[i + 2]].position, worldTransform);

			if (TriangleTests::Intersects(rayOriginInWorldSpace, rayDirectionInWorldSpace, v1, v2, v3, currentDistance))
			{
				// Calculate normal and reject backfacing triangles (clockwise winding order)
				XMVECTOR ab = v2 - v1;
				XMVECTOR ac = v3 - v1;
				currentNormal = XMVector3Normalize(XMVector3Cross(ac, ab));

				if (XMVectorGetX(XMVector3Dot(XMVectorNegate(rayDirectionInWorldSpace), currentNormal)) < 0)
					continue;

				if (!returnFurthest
					&& currentDistance < closestDistance)
				{
					closestDistance = currentDistance;
					returnedNormal = currentNormal;
					hit = true;
				}

				if (returnFurthest
					&& currentDistance > furthestDistance
					&& currentDistance <= maxDistance)
				{
					furthestDistance = currentDistance;
					returnedNormal = currentNormal;
					hit = true;
				}
			}
		}
	}

	if (hit)
	{
		distance = returnFurthest ? furthestDistance : closestDistance;
		normalInLocalSpace = returnedNormal;
	}

	return hit;
}

bool Mesh::TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, const XMMATRIX& worldTransform, float &distance, XMVECTOR &normal, float maxDistance, bool returnFurthest)
{
	if (IsEmpty())
		return false;

	if (m_boundingBoxNeedsUpdate)
		UpdateBoundingBox();

	if (m_drawStyle != Mesh::DS_TRILIST)
		return false;

	return m_boundingBoxNode.TestRayIntersection(m_vertices, rayOriginInWorldSpace, rayDirectionInWorldSpace, worldTransform, distance, normal, maxDistance, returnFurthest);
}

bool Mesh::TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, const XMMATRIX& worldTransform, float& distance)
{
	XMVECTOR normal = XMVectorZero();
	return TestRayIntersection(rayOriginInWorldSpace, rayDirectionInWorldSpace, worldTransform, distance, normal);
}

bool Mesh::TestPointInside(const XMVECTOR& pointInWorldSpace, const XMMATRIX& worldTransform)
{
	if (IsEmpty())
		return false;

	if (m_boundingBoxNeedsUpdate)
		UpdateBoundingBox();

	BoundingOrientedBox orientedBoundingBox;
	BoundingOrientedBox::CreateFromBoundingBox(orientedBoundingBox, m_boundingBoxNode.boundingBox);	
	orientedBoundingBox.Transform(orientedBoundingBox, worldTransform);

	return orientedBoundingBox.Contains(pointInWorldSpace) == CONTAINS;
}

void Mesh::BoundingBoxNode::GenerateChildNodes(const vector<Mesh::Vertex>& vertices, const vector<unsigned>& indices, unsigned currentDepth)
{
	indicesContained.clear();
	if (currentDepth == 1)
	{
		indicesContained = indices;
	}
	else
	{
		for (size_t i = 0; i < indices.size(); i += 3)
		{
			unsigned indexA = indices[i + 0];
			unsigned indexB = indices[i + 1];
			unsigned indexC = indices[i + 2];

			if (boundingBox.Contains(vertices[indexA].position) == ContainmentType::CONTAINS ||
				boundingBox.Contains(vertices[indexB].position) == ContainmentType::CONTAINS ||
				boundingBox.Contains(vertices[indexC].position) == ContainmentType::CONTAINS)
			{
				indicesContained.push_back(indexA);
				indicesContained.push_back(indexB);
				indicesContained.push_back(indexC);
			}
		}
	}

	if(indices.size() / 3 > targetTriangleCount)
	{
		XMFLOAT3 childExtents;
		childExtents.x = boundingBox.Extents.x / 2.0f;
		childExtents.y = boundingBox.Extents.y / 2.0f;
		childExtents.z = boundingBox.Extents.z / 2.0f;

		float xSigns[8] = { 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f };
		float ySigns[8] = { 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
		float zSigns[8] = { 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f };

		children.resize(8);
		for (unsigned i = 0; i < 8; ++i)
		{
			auto& child = children[i];
			child.boundingBox.Center.x = boundingBox.Center.x + xSigns[i] * childExtents.x;
			child.boundingBox.Center.y = boundingBox.Center.y + ySigns[i] * childExtents.y;
			child.boundingBox.Center.z = boundingBox.Center.z + zSigns[i] * childExtents.z;
			child.boundingBox.Extents = childExtents;
			child.GenerateChildNodes(vertices, indicesContained, currentDepth + 1);
		}
	}
	else
	{
		children.clear();
	}
}

const BoundingBox& Mesh::GetBoundingBox()
{
	if (m_boundingBoxNeedsUpdate)
		UpdateBoundingBox();

	return m_boundingBoxNode.boundingBox;
}

void Mesh::UpdateBoundingBox()
{
	m_boundingBoxNeedsUpdate = false;

	if (IsEmpty())
	{
		m_boundingBoxNode.boundingBox = BoundingBox(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f));
		m_boundingBoxNode.children.clear();
		return;
	}

	float minX, minY, minZ;
	minX = minY = minZ = FLT_MAX;
	float maxX, maxY, maxZ;
	maxX = maxY = maxZ = -FLT_MAX;

	for (unsigned i = 0; i < m_vertices.size(); ++i)
	{
		Vertex& vertex = m_vertices[i];
		float x = XMVectorGetX(vertex.position);
		float y = XMVectorGetY(vertex.position);
		float z = XMVectorGetZ(vertex.position);

		if (x < minX)
			minX = x;
		if (x > maxX)
			maxX = x;
		
		if (y < minY)
			minY = y;
		if (y > maxY)
			maxY = y;
		
		if (z < minZ)
			minZ = z;
		if (z > maxZ)
			maxZ = z;
	}

	m_boundingBoxNode.boundingBox.Extents.x = (maxX - minX) / 2.0f;
	m_boundingBoxNode.boundingBox.Extents.y = (maxY - minY) / 2.0f;
	m_boundingBoxNode.boundingBox.Extents.z = (maxZ - minZ) / 2.0f;

	m_boundingBoxNode.boundingBox.Center.x = minX + m_boundingBoxNode.boundingBox.Extents.x;
	m_boundingBoxNode.boundingBox.Center.y = minY + m_boundingBoxNode.boundingBox.Extents.y;
	m_boundingBoxNode.boundingBox.Center.z = minZ + m_boundingBoxNode.boundingBox.Extents.z;

	m_boundingBoxNode.GenerateChildNodes(m_vertices, m_indices, 1);
}

// Updates the vertex/index buffers if they already exists and is large enough, otherwise recreates them
void Mesh::UpdateD3DBuffers()
{
	m_d3dBuffersNeedUpdate = false;

	if (IsEmpty())
	{
		m_d3dVertexBuffer.Reset();
		m_d3dIndexBuffer.Reset();
		return;
	}

	if (m_d3dVertexBuffer && m_d3dVertexBufferDesc.ByteWidth / m_d3dVertexBufferDesc.StructureByteStride >= m_vertices.size())
	{
		g_d3dContext->UpdateSubresource(m_d3dVertexBuffer.Get(), 0, nullptr, m_vertices.data(), (UINT) m_vertices.size() * sizeof(Vertex), 1);
	}
	else
	{
		memset(&m_d3dVertexBufferDesc, 0, sizeof(D3D11_BUFFER_DESC));
		m_d3dVertexBufferDesc.ByteWidth = (UINT) m_vertices.size() * sizeof(Vertex);
		m_d3dVertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		m_d3dVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		m_d3dVertexBufferDesc.StructureByteStride = sizeof(Vertex);

		D3D11_SUBRESOURCE_DATA data;
		memset(&data, 0, sizeof(data));
		data.pSysMem = m_vertices.data();

		g_d3dDevice->CreateBuffer(&m_d3dVertexBufferDesc, &data, &m_d3dVertexBuffer);
		assert(m_d3dVertexBuffer);
	}

	if (m_d3dIndexBuffer && m_d3dIndexBufferDesc.ByteWidth / m_d3dIndexBufferDesc.StructureByteStride >= m_indices.size())
	{
		g_d3dContext->UpdateSubresource(m_d3dIndexBuffer.Get(), 0, nullptr, m_indices.data(), (UINT) m_indices.size() * sizeof(unsigned), 1);
	}
	else
	{
		memset(&m_d3dIndexBufferDesc, 0, sizeof(D3D11_BUFFER_DESC));
		m_d3dIndexBufferDesc.ByteWidth = (UINT) m_indices.size() * sizeof(unsigned);
		m_d3dIndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		m_d3dIndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		m_d3dIndexBufferDesc.StructureByteStride = sizeof(unsigned);

		D3D11_SUBRESOURCE_DATA data;
		memset(&data, 0, sizeof(data));
		data.pSysMem = m_indices.data();

		g_d3dDevice->CreateBuffer(&m_d3dIndexBufferDesc, &data, &m_d3dIndexBuffer);
		assert(m_d3dIndexBuffer);
	}
}


//
//DrawCall
//
const float DrawCall::DefaultFontSize = 64.0f;

XMMATRIX DrawCall::m_mtxLightViewProj;
XMMATRIX DrawCall::m_mtxCameraView;
DrawCall::Projection DrawCall::m_cameraProj;

XMVECTOR DrawCall::vAmbient;
DrawCall::Light DrawCall::vLights[kMaxLights];
unsigned DrawCall::uLightCount = 0;
unsigned DrawCall::uActiveLightIdx = 0;

bool DrawCall::m_singlePassStereoSupported = false;
bool DrawCall::m_singlePassStereoEnabled = false;

stack<DrawCall::View> DrawCall::m_viewStack;
DrawCall::View DrawCall::m_activeView;

stack<DrawCall::Projection> DrawCall::m_projectionStack;
DrawCall::Projection DrawCall::m_activeProjection;

shared_ptr<Texture2D> DrawCall::m_backBuffer;
std::map<intptr_t, std::shared_ptr<Texture2D>> DrawCall::m_cachedBackBuffers;
stack<vector<shared_ptr<Texture2D>>> DrawCall::m_renderTargetStack;

stack<unsigned> DrawCall::m_renderPassIndexStack;
unsigned DrawCall::m_activeRenderPassIndex = 0;

stack<DrawCall::BlendState> DrawCall::m_sAlphaBlendStates;
stack<bool> DrawCall::m_sDepthTestStates;
stack<bool> DrawCall::m_sBackfaceCullingStates;

stack<bool> DrawCall::m_sRightEyePassStates;
stack<bool> DrawCall::m_sFullscreenPassStates;

vector<DrawCall::RenderPassDesc> DrawCall::m_vGlobalRenderPasses;

map<string, shared_ptr<Shader>> DrawCall::m_shaderStore;

Microsoft::WRL::ComPtr<ID3D11BlendState> DrawCall::m_d3dAlphaBlendEnabledState;
Microsoft::WRL::ComPtr<ID3D11BlendState> DrawCall::m_d3dAdditiveBlendEnabledState;
Microsoft::WRL::ComPtr<ID3D11BlendState> DrawCall::m_d3dColorWriteDisabledState;
Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DrawCall::m_d3dDepthTestDisabledState;
Microsoft::WRL::ComPtr<ID3D11RasterizerState> DrawCall::m_d3dBackfaceCullingDisabledState;

bool DrawCall::Initialize()
{
	//
	// Create device
	//

	unsigned createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;	// Required for D2D to work
#ifdef _DEBUG
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL selectedFeatureLevel;
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, createDeviceFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &g_d3dDevice, &selectedFeatureLevel, nullptr);
	g_d3dDevice->GetImmediateContext(&g_d3dContext);

	// Check for device support for the optional feature that allows setting the render target array index from the vertex shader
	m_singlePassStereoSupported = false;
	m_singlePassStereoEnabled = false;
	D3D11_FEATURE_DATA_D3D11_OPTIONS3 d3dOptions;
	g_d3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &d3dOptions, sizeof(d3dOptions));
	if (d3dOptions.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer)
	{
		m_singlePassStereoSupported = true;
		m_singlePassStereoEnabled = true;
	}

	//
	// Create D2D and DWrite stuff
	//

	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
	// If the project is in a debug build, enable Direct2D debugging via SDK Layers.
	//options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, &g_d2dFactory);
	
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &g_dwriteFactory);

	Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
	g_d3dDevice.As(&dxgiDevice);
	g_d2dFactory->CreateDevice(dxgiDevice.Get(), &g_d2dDevice);
	g_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &g_d2dContext);
	
	g_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	
	g_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &g_whiteBrush);
	g_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gray), &g_grayBrush);
	g_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &g_blackBrush);

	//
	// Setup static objects (and set their values to reasonable defaults)
	//

	m_activeRenderPassIndex = 0;
	SetBackBuffer(make_shared<Texture2D>(512, 512, DXGI_FORMAT_B8G8R8A8_UNORM));

	vAmbient = XMVectorSet(.1f, .1f, .1f, 1.f);

	vLights[0].vLightPosW = XMVectorSet(7.5f, 10.f, -2.5f, 1.f);
	vLights[0].vLightAtW = XMVectorSet(0.f, 0.f, 0.f, 1.f);
	uLightCount = 1;

	DrawCall::PushView(XMVectorSet(5.f, 5.f, 5.f, 1.f), XMVectorSet(0.f, 0.f, 0.f, 1.f), XMVectorSet(0.f, 1.f, 0.f, 0.f));
	DrawCall::PushProj(XM_PIDIV4, m_backBuffer->GetAspect(), 1.0f, 1000.0f);

	m_mtxLightViewProj = XMMatrixIdentity();
	m_mtxCameraView = XMMatrixIdentity();

	// Create the alpha blend state
	D3D11_BLEND_DESC blendDesc;
	memset(&blendDesc, 0, sizeof(blendDesc));
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	g_d3dDevice->CreateBlendState(&blendDesc, &m_d3dAlphaBlendEnabledState);
	DrawCall::PushAlphaBlendState(BLEND_NONE);

	// Create the additive blend state
	memset(&blendDesc, 0, sizeof(blendDesc));
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	g_d3dDevice->CreateBlendState(&blendDesc, &m_d3dAdditiveBlendEnabledState);

	// Create the color write disabled state
	memset(&blendDesc, 0, sizeof(blendDesc));
	g_d3dDevice->CreateBlendState(&blendDesc, &m_d3dColorWriteDisabledState);

	// Create the depth test disabled state
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	memset(&depthStencilDesc, 0, sizeof(depthStencilDesc));
	depthStencilDesc.DepthEnable = FALSE;
	g_d3dDevice->CreateDepthStencilState(&depthStencilDesc, &m_d3dDepthTestDisabledState);
	DrawCall::PushDepthTestState(true);

	// Create the backface culling disabled state
	D3D11_RASTERIZER_DESC rasterizerDesc;
	memset(&rasterizerDesc, 0, sizeof(rasterizerDesc));
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.DepthBias = 0;
	rasterizerDesc.SlopeScaledDepthBias = 0.f;
	rasterizerDesc.DepthBiasClamp = 0.f;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.ScissorEnable = FALSE;
	rasterizerDesc.MultisampleEnable = FALSE;
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	g_d3dDevice->CreateRasterizerState(&rasterizerDesc, &m_d3dBackfaceCullingDisabledState);
	DrawCall::PushBackfaceCullingState(true);

	return true;
}

#ifdef USE_WINRT_D3D
bool DrawCall::InitializeSwapChain(unsigned width, unsigned height, winrt::Windows::UI::Core::CoreWindow const& window)
#else
bool DrawCall::InitializeSwapChain(unsigned width, unsigned height, HWND hWnd, bool fullscreen)
#endif
{
	if (!g_d3dDevice)
		return false;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.Flags = 0;

	Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice;
	g_d3dDevice.As(&dxgiDevice);

	Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
	dxgiDevice->GetAdapter(&dxgiAdapter);

	Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
	dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), &dxgiFactory);

#ifdef USE_WINRT_D3D
	dxgiFactory->CreateSwapChainForCoreWindow(g_d3dDevice.Get(), (IUnknown*) winrt::get_abi(window), &swapChainDesc, nullptr, &g_d3dSwapChain);
#else	
	if (fullscreen)
	{
		Microsoft::WRL::ComPtr<IDXGIOutput> dxgiOutput;
		dxgiAdapter->EnumOutputs(0, &dxgiOutput);

		DXGI_MODE_DESC desiredMode;
		desiredMode.Format = swapChainDesc.Format;
		desiredMode.Height = swapChainDesc.Height;
		desiredMode.Width = swapChainDesc.Width;
		desiredMode.RefreshRate.Numerator = 60000;
		desiredMode.RefreshRate.Denominator = 1001;
		desiredMode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		desiredMode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;

		DXGI_MODE_DESC closestMode;
		dxgiOutput->FindClosestMatchingMode(&desiredMode, &closestMode, nullptr);

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc;
		swapChainDesc.Format = closestMode.Format;
		swapChainDesc.Width = closestMode.Width;
		swapChainDesc.Height = closestMode.Height;
		fullscreenDesc.RefreshRate = closestMode.RefreshRate;
		fullscreenDesc.ScanlineOrdering = closestMode.ScanlineOrdering;
		fullscreenDesc.Scaling = closestMode.Scaling;
		fullscreenDesc.Windowed = FALSE;
		dxgiFactory->CreateSwapChainForHwnd(g_d3dDevice.Get(), (HWND)hWnd, &swapChainDesc, &fullscreenDesc, nullptr, &g_d3dSwapChain);
	}
	else
	{
		dxgiFactory->CreateSwapChainForHwnd(g_d3dDevice.Get(), (HWND)hWnd, &swapChainDesc, nullptr, nullptr, &g_d3dSwapChain);
	}
#endif

	SetBackBuffer(g_d3dSwapChain.Get());

	return true;
}

bool DrawCall::ResizeSwapChain(unsigned newWidth, unsigned newHeight)
{
	if (!g_d3dSwapChain || !m_backBuffer || !g_d3dContext)
		return false;

	m_backBuffer->Reset();
	g_d3dContext->OMSetRenderTargets(0, nullptr, nullptr);
	g_d2dContext->SetTarget(nullptr);
	
	g_d3dSwapChain->ResizeBuffers(0, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, 0);
	
	Microsoft::WRL::ComPtr<ID3D11Texture2D> d3dBackBuffer = nullptr;
	g_d3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&d3dBackBuffer);
	m_backBuffer->SetD3DTexture(d3dBackBuffer.Get());

	return true;
}

void DrawCall::Uninitialize()
{
	m_shaderStore.clear();

	m_backBuffer.reset();
	m_cachedBackBuffers.clear();

	m_d3dAlphaBlendEnabledState.Reset();
	m_d3dDepthTestDisabledState.Reset();
	m_d3dAdditiveBlendEnabledState.Reset();
	m_d3dColorWriteDisabledState.Reset();
	m_d3dBackfaceCullingDisabledState.Reset();

	g_d2dFactory.Reset();
	g_d2dDevice.Reset();
	g_d2dContext.Reset();
	g_dwriteFactory.Reset();

	g_textFormats.clear();
	g_whiteBrush.Reset();

	g_d3dContext.Reset();
	g_d3dDevice.Reset();
	g_d3dSwapChain.Reset();
}

ID3D11Device* DrawCall::GetD3DDevice()
{
	return g_d3dDevice.Get();
}

ID3D11DeviceContext* DrawCall::GetD3DDeviceContext()
{
	return g_d3dContext.Get();
}

IDXGISwapChain1* DrawCall::GetD3DSwapChain()
{
	return g_d3dSwapChain.Get();
}

bool DrawCall::IsSinglePassSteroSupported()
{
	return m_singlePassStereoSupported;
}

bool DrawCall::IsSinglePassSteroEnabled()
{
	return m_singlePassStereoEnabled;
}

void DrawCall::EnableSinglePassStereo(bool enabled)
{
	if (IsSinglePassSteroSupported())
		m_singlePassStereoEnabled = enabled;
}

void DrawCall::PushView(const XMMATRIX& mtxLeft, const XMMATRIX& mtxRight)
{
	m_activeView.mtx = mtxLeft;
	m_activeView.mtxRight = mtxRight;
	m_viewStack.push(m_activeView);
}

void DrawCall::PushView(const XMVECTOR& vEye, const XMVECTOR vAt, const XMVECTOR &vUp)
{
	m_activeView.mtx = m_activeView.mtxRight = XMMatrixLookAtRH(vEye, vAt, vUp);
	m_viewStack.push(m_activeView);
}

void DrawCall::PopView()
{
	// Don't pop the last view matrix
	if(m_viewStack.size() <=1)
		return;

	m_viewStack.pop();
	m_activeView = m_viewStack.top();
}

XMMATRIX DrawCall::GetView()
{
	return m_activeView.mtx;
}

void DrawCall::PushProj(const XMMATRIX& mtxLeft, const XMMATRIX& mtxRight)
{
	memset(&m_activeProjection, 0, sizeof(m_activeProjection));

	m_activeProjection.mtx = mtxLeft;
	m_activeProjection.mtxRight = mtxRight;

	m_projectionStack.push(m_activeProjection);
}

void DrawCall::PushProj(float fFOV, float fAspect, float fNear, float fFar)
{
	m_activeProjection.mtx = XMMatrixPerspectiveFovRH(fFOV, fAspect, fNear, fFar);
	m_activeProjection.fNearPlaneHeight = tan(fFOV * .5f) * 2.f * fNear;
	m_activeProjection.fNearPlaneWidth = m_activeProjection.fNearPlaneHeight * fAspect;
	m_activeProjection.fNear = fNear;
	m_activeProjection.fFar = fFar;
	m_activeProjection.fRange = fFar / (fFar-fNear);
	
	m_projectionStack.push(m_activeProjection);
}

void DrawCall::PushProjOrtho(float fWidth, float fHeight, float fNear, float fFar)
{
	m_activeProjection.mtx = XMMatrixOrthographicRH(fWidth, fHeight, fNear, fFar);
	m_activeProjection.fNearPlaneHeight = fHeight;
	m_activeProjection.fNearPlaneWidth = fWidth;
	m_activeProjection.fNear = fNear;
	m_activeProjection.fFar = fFar;
	m_activeProjection.fRange = fFar / (fFar-fNear);

	m_projectionStack.push(m_activeProjection);
}

void DrawCall::PopProj()
{
	// Don't pop the last proj matrix
	if (m_viewStack.size() <= 1)
		return;

	m_projectionStack.pop();
	m_activeProjection = m_projectionStack.top();
}

XMMATRIX DrawCall::GetProj()
{
	return m_activeProjection.mtx;
}

void DrawCall::StoreCurrentViewProjAsLightViewProj()
{
	m_mtxLightViewProj = XMMatrixMultiply(m_activeView.mtx, m_activeProjection.mtx);
}

void DrawCall::StoreCurrentViewAsCameraView()
{
	m_mtxCameraView = m_activeView.mtx;
	m_cameraProj = m_activeProjection;
}

shared_ptr<Texture2D> DrawCall::GetBackBuffer()
{
	return m_backBuffer;
}

void DrawCall::SetBackBuffer(shared_ptr<Texture2D> backBuffer)
{
	if (m_renderPassIndexStack.empty())
		m_activeRenderPassIndex = 0;

	m_backBuffer = backBuffer;
	if (m_renderTargetStack.empty())
		SetCurrentRenderTargetsOnD3DDevice();
}

void DrawCall::SetBackBuffer(ID3D11Texture2D* pD3DBackBuffer, const D3D11_VIEWPORT& viewport)
{
	if(!pD3DBackBuffer)
		return;

	std::shared_ptr<Texture2D> backBuffer;

	auto it = m_cachedBackBuffers.find((intptr_t)pD3DBackBuffer);
	if (it == m_cachedBackBuffers.end())
	{
		backBuffer = make_shared<Texture2D>(pD3DBackBuffer);
		m_cachedBackBuffers.insert({ (intptr_t)pD3DBackBuffer, backBuffer });
	}
	else
	{
		backBuffer = it->second;
	}

	backBuffer->SetViewport(viewport);
	SetBackBuffer(backBuffer);
}

void DrawCall::SetBackBuffer(ID3D11Texture2D* pD3DBackBuffer)
{
	if (!pD3DBackBuffer)
		return;

	D3D11_TEXTURE2D_DESC desc;
	pD3DBackBuffer->GetDesc(&desc);
	CD3D11_VIEWPORT viewport(0.0f, 0.0f, (float)desc.Width, (float)desc.Height);

	SetBackBuffer(pD3DBackBuffer, viewport);
}

void DrawCall::SetBackBuffer(IDXGISwapChain1* pSwapChain)
{
	if (!pSwapChain)
		return;

	// Create a texture for the back buffer of the swap chain
	Microsoft::WRL::ComPtr<ID3D11Texture2D> d3dBackBuffer = nullptr;
	pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&d3dBackBuffer);
	assert(d3dBackBuffer);

	SetBackBuffer(d3dBackBuffer.Get());

	return;
}

shared_ptr<Texture2D> DrawCall::GetCurrentRenderTarget()
{
	if (m_renderTargetStack.empty())
		return m_backBuffer;
	else
		return m_renderTargetStack.top().front();
}

void DrawCall::PushRenderPass(unsigned renderPassIndex, shared_ptr<Texture2D> renderTarget)
{
	if (!renderTarget)
		renderTarget = GetCurrentRenderTarget();

	vector<shared_ptr<Texture2D>> renderTargets	{ renderTarget };
	return PushRenderPass(renderPassIndex, renderTargets);
}

void DrawCall::PushRenderPass(unsigned renderPassIndex, vector<shared_ptr<Texture2D>> renderTargets)
{
	m_renderPassIndexStack.push(renderPassIndex);
	m_activeRenderPassIndex = renderPassIndex;

	if (renderTargets.empty())
		renderTargets.push_back(GetCurrentRenderTarget());

	m_renderTargetStack.push(renderTargets);
	SetCurrentRenderTargetsOnD3DDevice();
}

void DrawCall::PopRenderPass()
{
	m_renderPassIndexStack.pop();
	if (m_renderPassIndexStack.empty())
		m_activeRenderPassIndex = 0;
	else
		m_activeRenderPassIndex = m_renderPassIndexStack.top();

	m_renderTargetStack.pop();
	SetCurrentRenderTargetsOnD3DDevice();

	// Clear the shader resource views
	ID3D11ShaderResourceView* vNullShaderResourceViews[16];
	memset(vNullShaderResourceViews, 0, sizeof(vNullShaderResourceViews));
	g_d3dContext->VSSetShaderResources(0, 16, vNullShaderResourceViews);
	g_d3dContext->PSSetShaderResources(0, 16, vNullShaderResourceViews);
}

void DrawCall::SetCurrentRenderTargetsOnD3DDevice()
{
	vector<shared_ptr<Texture2D>> renderTargets;

	if (m_renderTargetStack.empty())
		renderTargets.push_back(m_backBuffer);
	else
		renderTargets = m_renderTargetStack.top();

	vector<ID3D11RenderTargetView*> renderTargetViews;
	for (auto &texture : renderTargets)
	{
		ID3D11RenderTargetView* pRenderTargetView = texture->GetRenderTargetView();
		if (texture->IsStereo() && IsRightEyePassActive())
			pRenderTargetView = texture->GetRenderTargetViewRight();

		renderTargetViews.push_back(pRenderTargetView);
	}

	ID3D11DepthStencilView* pDepthStencilView = renderTargets[0]->GetDepthStencilView();
	if (renderTargets[0]->IsStereo() && IsRightEyePassActive())
		pDepthStencilView = renderTargets[0]->GetDepthStencilViewRight();

	g_d3dContext->OMSetRenderTargets((UINT) renderTargetViews.size(), renderTargetViews.data(), pDepthStencilView);
	g_d3dContext->RSSetViewports(1, &renderTargets[0]->GetViewport());
	g_d2dContext->SetTarget(renderTargets[0]->GetD2DTargetBitmap());
}

bool DrawCall::IsRightEyePassActive()
{
	return !m_sRightEyePassStates.empty() && m_sRightEyePassStates.top();
}

void DrawCall::PushRightEyePass(unsigned renderPassIndex, shared_ptr<Texture2D> renderTarget)
{
	m_sRightEyePassStates.push(true);
	PushRenderPass(renderPassIndex, renderTarget);
}

void DrawCall::PopRightEyePass()
{
	m_sRightEyePassStates.pop();
	PopRenderPass();
}


void DrawCall::PushShadowPass(shared_ptr<Texture2D> depthMap, unsigned uRenderPassIdx, unsigned uLightIdx)
{
	assert(depthMap);

	DrawCall::PushRenderPass(uRenderPassIdx, depthMap);

	DrawCall::PushView(DrawCall::vLights[uLightIdx].vLightPosW, DrawCall::vLights[uLightIdx].vLightAtW, XMVectorSet(0.f, 1.f, 0.f, 0.f));
	DrawCall::PushProj(XM_PIDIV4, 1.f, 1.f, 250.f);
	DrawCall::StoreCurrentViewProjAsLightViewProj();
}

void DrawCall::PopShadowPass()
{
	DrawCall::PopView();
	DrawCall::PopProj();

	DrawCall::PopRenderPass();
}

void DrawCall::PushUIPass(shared_ptr<Texture2D> renderTarget)
{
	DrawCall::PushRenderPass(0, renderTarget);

	DrawCall::PushView(XMVectorSet(0.f, 0.f, 1.f, 1.f),
						XMVectorSet(0.f, 0.f, 0.f, 1.f), 
						XMVectorSet(0.f, 1.f, 0.f, 0.f));

	DrawCall::PushProjOrtho(GetCurrentRenderTarget()->GetWidth()/(float)GetCurrentRenderTarget()->GetHeight(), 1.f, 1.f, 1000.f);

	DrawCall::PushDepthTestState(false);
}

void DrawCall::PopUIPass()
{
	DrawCall::PopView();
	DrawCall::PopProj();

	DrawCall::PopDepthTestState();

	DrawCall::PopRenderPass();
}

void DrawCall::PushFullscreenPass(shared_ptr<Texture2D> renderTarget)
{
	PushUIPass(renderTarget);
	m_sFullscreenPassStates.push(true);
}

void DrawCall::PopFullscreenPass()
{
	PopUIPass();
	m_sFullscreenPassStates.pop();
}

void DrawCall::PushAlphaBlendState(BlendState blendState)
{
	m_sAlphaBlendStates.push(blendState);

	if (m_sAlphaBlendStates.top() == BLEND_ALPHA)
		g_d3dContext->OMSetBlendState(m_d3dAlphaBlendEnabledState.Get(), nullptr, 0xffffffff);
	else if (m_sAlphaBlendStates.top() == BLEND_ADDITIVE)
		g_d3dContext->OMSetBlendState(m_d3dAdditiveBlendEnabledState.Get(), nullptr, 0xffffffff);
	else if(m_sAlphaBlendStates.top() == BLEND_COLOR_DISABLED)
		g_d3dContext->OMSetBlendState(m_d3dColorWriteDisabledState.Get(), nullptr, 0xffffffff);
	else
		g_d3dContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void DrawCall::PopAlphaBlendState()
{
	if (m_sAlphaBlendStates.size() <= 1)
		return;

	m_sAlphaBlendStates.pop();

	if (m_sAlphaBlendStates.top() == BLEND_ALPHA)
		g_d3dContext->OMSetBlendState(m_d3dAlphaBlendEnabledState.Get(), nullptr, 0xffffffff);
	else if (m_sAlphaBlendStates.top() == BLEND_ADDITIVE)
		g_d3dContext->OMSetBlendState(m_d3dAdditiveBlendEnabledState.Get(), nullptr, 0xffffffff);
	else if (m_sAlphaBlendStates.top() == BLEND_COLOR_DISABLED)
		g_d3dContext->OMSetBlendState(m_d3dColorWriteDisabledState.Get(), nullptr, 0xffffffff);
	else
		g_d3dContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void DrawCall::PushDepthTestState(bool bEnabled)
{
	m_sDepthTestStates.push(bEnabled);

	if (!m_sDepthTestStates.top())
		g_d3dContext->OMSetDepthStencilState(m_d3dDepthTestDisabledState.Get(), 0);
	else
		g_d3dContext->OMSetDepthStencilState(nullptr, 0);
}

void DrawCall::PopDepthTestState()
{
	if (m_sDepthTestStates.size() <= 1)
		return;

	m_sDepthTestStates.pop();

	if (!m_sDepthTestStates.top())
		g_d3dContext->OMSetDepthStencilState(m_d3dDepthTestDisabledState.Get(), 0);
	else
		g_d3dContext->OMSetDepthStencilState(nullptr, 0);
}

void DrawCall::PushBackfaceCullingState(bool bEnabled)
{
	m_sBackfaceCullingStates.push(bEnabled);

	if (!m_sBackfaceCullingStates.top())
		g_d3dContext->RSSetState(m_d3dBackfaceCullingDisabledState.Get());
	else
		g_d3dContext->RSSetState(nullptr);
}

void DrawCall::PopBackfaceCullingState()
{
	if (m_sBackfaceCullingStates.size() <= 1)
		return;

	m_sBackfaceCullingStates.pop();

	if (!m_sBackfaceCullingStates.top())
		g_d3dContext->RSSetState(m_d3dBackfaceCullingDisabledState.Get());
	else
		g_d3dContext->RSSetState(nullptr);
}

void DrawCall::AddGlobalRenderPass(const string& vertexShaderFilename, const string& pixelShaderFilename, const unsigned renderPassIndex)
{
	RenderPassDesc renderPass;
	renderPass.vertexShaderFilename = vertexShaderFilename;
	renderPass.pixelShaderFilename = pixelShaderFilename;
	renderPass.renderPassIndex = renderPassIndex;
	m_vGlobalRenderPasses.push_back(renderPass);
}

shared_ptr<Shader> DrawCall::LoadShaderUsingShaderStore(Shader::ShaderType type, const std::string& filename)
{
	shared_ptr<Shader> shader;

	auto it = m_shaderStore.find(filename);
	if (it == m_shaderStore.end())
	{
		shader = make_shared<Shader>(type, filename);
		m_shaderStore[filename] = shader;
	}
	else
	{
		shader = it->second;
	}

	return shader;
}

void DrawCall::Present()
{
	if (g_d3dSwapChain)
		g_d3dSwapChain->Present(0, 0);
}

void DrawCall::DrawText(const std::string& text, const D2D_RECT_F& layoutRect, float fontSize, HorizontalAlignment horizontalAlignment, VerticalAlignment verticalAlignment, TextColor textColor)
{
	g_d2dContext->BeginDraw();

	wstring wideText = StringToWideString(text);

	Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat;
	const auto& textFormatRecord = g_textFormats.find(fontSize);
	if (textFormatRecord != g_textFormats.end())
	{
		textFormat = textFormatRecord->second;
	}
	else
	{
		g_dwriteFactory->CreateTextFormat(
			L"Segoe UI",
			nullptr,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			fontSize,
			L"en-US",
			&textFormat);

		switch (horizontalAlignment)
		{
		case HorizontalAlignment::Left:
			textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
			break;
		case HorizontalAlignment::Center:
			textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
			break;
		case HorizontalAlignment::Right:
			textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
			break;
		}

		switch (verticalAlignment)
		{
		case VerticalAlignment::Top:
			textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
			break;
		case VerticalAlignment::Middle:
			textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
			break;
		case VerticalAlignment::Bottom:
			textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR);
			break;
		}
	}

	ID2D1SolidColorBrush* pBrush = g_whiteBrush.Get();
	if (textColor == TextColor::Gray)
		pBrush = g_grayBrush.Get();
	else if (textColor == TextColor::Black)
		pBrush = g_blackBrush.Get();

	g_d2dContext->DrawText(wideText.c_str(), (UINT32) text.size(), textFormat.Get(), layoutRect, pBrush);

	g_d2dContext->EndDraw();
}

void DrawCall::DrawText(const std::string &text, float x, float y, float fontSize, TextColor textColor)
{
	D2D1_RECT_F rect;
	rect.left = x;
	rect.top = y;
	rect.right = FLT_MAX;
	rect.bottom = FLT_MAX;

	DrawText(text, rect, fontSize, HorizontalAlignment::Left, VerticalAlignment::Top, textColor);
}

XMMATRIX DrawCall::CalculateWorldTransformForLine(const XMVECTOR& startPosition, const XMVECTOR& endPosition, const float radius, const XMVECTOR& rightDirection)
{
	if (XMVector3Equal(startPosition, endPosition))
		return XMMatrixIdentity();

	XMVECTOR vectorToTarget = endPosition - startPosition;
	XMMATRIX translation = XMMatrixTranslationFromVector(startPosition + (vectorToTarget / 2.0f));

	float length = XMVectorGetX(XMVector3Length(vectorToTarget));
	XMMATRIX scale = XMMatrixScaling(radius * 2.0f, length, radius * 2.0f);

	XMMATRIX rotationToZAxisAligned = XMMatrixRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XM_PIDIV2);

	XMMATRIX rotationToTargetAligned = XMMatrixLookToRH(
		XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
		XMVector3Normalize(vectorToTarget),
		rightDirection);
	rotationToTargetAligned = XMMatrixTranspose(rotationToTargetAligned);

	return scale * rotationToZAxisAligned * rotationToTargetAligned * translation;
}

XMMATRIX DrawCall::CalculateWorldTransformForLine(const XMVECTOR& startPosition, const XMVECTOR& endPosition, const float radius)
{
	return CalculateWorldTransformForLine(startPosition, endPosition, radius, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}


DrawCall::DrawCall(const string& vertexShaderFilename, const string& pixelShaderFilename, shared_ptr<Mesh> mesh, const string& geometryShaderFilename)
{
	assert(g_d3dDevice);

	AddRenderPass(vertexShaderFilename, pixelShaderFilename, 0, geometryShaderFilename);

	SetInstanceCapacity(1, false);
	m_instanceBufferNeedsUpdate = true;

	allInstanceWorldTransform = XMMatrixIdentity();

	m_mesh = mesh;
	if (!m_mesh)
		m_mesh = make_shared<Mesh>();

	for (auto renderPass : m_vGlobalRenderPasses)
		AddRenderPass(renderPass.vertexShaderFilename, renderPass.pixelShaderFilename, renderPass.renderPassIndex, renderPass.geometryShaderFilename);
};

DrawCall::DrawCall(const std::string& vertexShaderFilename, const std::string& pixelShaderFilename, Mesh::MeshType meshType, const std::string& geometryShaderFilename):
	DrawCall(vertexShaderFilename, pixelShaderFilename, make_shared<Mesh>(meshType), geometryShaderFilename)
{}

DrawCall::DrawCall(const std::string& vertexShaderFilename, const std::string& pixelShaderFilename, const std::string& modelFilename, const std::string& geometryShaderFilename):
	DrawCall(vertexShaderFilename, pixelShaderFilename, make_shared<Mesh>(modelFilename), geometryShaderFilename)
{}

void DrawCall::AddRenderPass(const string& vertexShaderFilename, const string& pixelShaderFilename, unsigned renderPassIndex, const string& geometryShaderFilename)
{
	ShaderSet shaderSet;

	shaderSet.vertexShader = LoadShaderUsingShaderStore(Shader::ST_VERTEX, vertexShaderFilename);
	shaderSet.pixelShader = LoadShaderUsingShaderStore(Shader::ST_PIXEL, pixelShaderFilename);

	if (!geometryShaderFilename.empty())
		shaderSet.geometryShader = LoadShaderUsingShaderStore(Shader::ST_GEOMETRY, geometryShaderFilename);

	if (DrawCall::IsSinglePassSteroSupported())
	{
		string extension = GetFilenameExtension(vertexShaderFilename);
		string vertexShaderSPSFilename = RemoveFilenameExtension(vertexShaderFilename);
		vertexShaderSPSFilename = vertexShaderSPSFilename + "_SPS." + extension;

		if (FileExists(vertexShaderSPSFilename))
			shaderSet.vertexShaderSPS = LoadShaderUsingShaderStore(Shader::ST_VERTEX_SPS, vertexShaderSPSFilename);
	}

	m_shaderSets[renderPassIndex] = shaderSet;
}

unsigned DrawCall::GetInstanceCapacity()
{
	return (unsigned) m_instances.size();
}

void DrawCall::SetInstanceCapacity(unsigned instanceCapacity, bool enableParticleInstancing)
{
	if (enableParticleInstancing != m_particleInstancingEnabled)
	{
		m_instancingLayout.Reset();
		m_instancingLayoutSPS.Reset();

		m_instances.clear();
		m_particleInstances.clear();
	}

	vector<D3D11_INPUT_ELEMENT_DESC> elements = g_instancedElements;
	unsigned instanceSize = sizeof(Instance);
	m_instances.resize(instanceCapacity);
	if (enableParticleInstancing)
	{
		elements = g_instancedParticleElements;
		instanceSize = sizeof(ParticleInstance);
		m_particleInstances.resize(instanceCapacity);
	}

	if (!m_instancingLayout && GetVertexShader())
	{
		g_d3dDevice->CreateInputLayout(elements.data(), (UINT) elements.size(), GetVertexShader()->GetBytecode(), GetVertexShader()->GetBytecodeSize(), &m_instancingLayout);
	}
	if (!m_instancingLayoutSPS && GetVertexShaderSPS())
	{
		for (auto &element : elements)
		{
			if (element.InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA)
				element.InstanceDataStepRate = 2;
		}

		g_d3dDevice->CreateInputLayout(elements.data(), (UINT) elements.size(), GetVertexShaderSPS()->GetBytecode(), GetVertexShaderSPS()->GetBytecodeSize(), &m_instancingLayoutSPS);
	}

	D3D11_BUFFER_DESC d3dBufferDesc;
	memset(&d3dBufferDesc, 0, sizeof(D3D11_BUFFER_DESC));
	d3dBufferDesc.ByteWidth = instanceCapacity * instanceSize;

	d3dBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	d3dBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	d3dBufferDesc.StructureByteStride = instanceSize;

	g_d3dDevice->CreateBuffer(&d3dBufferDesc, nullptr, &m_instanceBuffer);
	assert(m_instanceBuffer);

	m_instanceBufferNeedsUpdate = true;
	m_particleInstancingEnabled = enableParticleInstancing;
}

DrawCall::Instance* DrawCall::GetInstanceBuffer()
{
	if (m_particleInstancingEnabled)
		return nullptr;

	m_instanceBufferNeedsUpdate = true;
	return m_instances.data();
}

DrawCall::ParticleInstance* DrawCall::GetParticleInstanceBuffer()
{
	if (!m_particleInstancingEnabled)
		return nullptr;

	m_instanceBufferNeedsUpdate = true;
	return m_particleInstances.data();
}

void DrawCall::SetWorldTransform(const XMMATRIX& worldTransform, unsigned instanceIndex)
{
	if (instanceIndex < m_instances.size())
		m_instances[instanceIndex].worldTransform = worldTransform;

	m_instanceBufferNeedsUpdate = true;
}

XMMATRIX DrawCall::GetWorldTransform(unsigned instanceIndex) const
{
	if (instanceIndex >= m_instances.size())
		return XMMatrixIdentity();

	return m_instances[instanceIndex].worldTransform;
}

void DrawCall::SetColor(const XMVECTOR& color, unsigned instanceIndex)
{
	if (m_particleInstancingEnabled == false)
	{
		if (instanceIndex < m_instances.size())
			m_instances[instanceIndex].color = color;
	}
	else
	{
		if (instanceIndex < m_particleInstances.size())
			m_particleInstances[instanceIndex].color = color;
	}
	m_instanceBufferNeedsUpdate = true;
}

bool DrawCall::TestPointInside(const XMVECTOR& pointInWorldSpace, const unsigned instanceIndex)
{
	if (instanceIndex < m_instances.size())
		return m_mesh->TestPointInside(pointInWorldSpace, m_instances[instanceIndex].worldTransform);
	else
		return false;
}

bool DrawCall::TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, float &distance, XMVECTOR &normal, const unsigned instanceIndex)
{
	if (instanceIndex < m_instances.size())
		return m_mesh->TestRayIntersection(rayOriginInWorldSpace, rayDirectionInWorldSpace, m_instances[instanceIndex].worldTransform, distance, normal);
	else
		return false;
}

bool DrawCall::TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, float& distance, const unsigned instanceIndex)
{
	XMVECTOR normal = XMVectorZero();
	return TestRayIntersection(rayOriginInWorldSpace, rayDirectionInWorldSpace, distance, normal, instanceIndex);
}


shared_ptr<Shader> DrawCall::GetVertexShader(unsigned renderPassIndex)
{
	return m_shaderSets[renderPassIndex].vertexShader;
}

shared_ptr<Shader> DrawCall::GetPixelShader(unsigned renderPassIndex)
{
	return m_shaderSets[renderPassIndex].pixelShader;
}

shared_ptr<Shader> DrawCall::GetGeometryShader(unsigned renderPassIndex)
{
	return m_shaderSets[renderPassIndex].geometryShader;
}

shared_ptr<Shader> DrawCall::GetVertexShaderSPS(unsigned renderPassIndex)
{
	return m_shaderSets[renderPassIndex].vertexShaderSPS;
}

void DrawCall::Draw(unsigned instancesToDraw)
{
	SetupDraw(instancesToDraw);

	if (GetCurrentRenderTarget()->IsStereo() && IsSinglePassSteroEnabled())
		instancesToDraw *= 2;

	g_d3dContext->DrawIndexedInstanced(m_mesh->GetIndexCount(), instancesToDraw, 0, 0, 0);
}

void DrawCall::SetupDraw(unsigned instancesToDraw)
{
	if (GetCurrentRenderTarget()->IsStereo() && IsSinglePassSteroEnabled())
		g_d3dContext->IASetInputLayout(m_instancingLayoutSPS.Get());
	else
		g_d3dContext->IASetInputLayout(m_instancingLayout.Get());

	// Auto-scale quad to fullscreen if in fullscreen pass
	if(!m_sFullscreenPassStates.empty() && m_sFullscreenPassStates.top())
	{
		float fAspect = GetCurrentRenderTarget()->GetWidth() / (float) GetCurrentRenderTarget()->GetHeight();
		SetWorldTransform(XMMatrixScaling(fAspect, 1.f, 1.f));
	}

	ShaderSet& shaderSet = m_shaderSets[m_activeRenderPassIndex];
	assert(shaderSet.vertexShader && shaderSet.pixelShader);
	if (GetCurrentRenderTarget()->IsStereo() && IsSinglePassSteroEnabled())
	{
		shaderSet.vertexShaderSPS->Bind();
		UpdateShaderConstants(shaderSet.vertexShaderSPS);
	}
	else
	{
		shaderSet.vertexShader->Bind();
		UpdateShaderConstants(shaderSet.vertexShader);
	}

	shaderSet.pixelShader->Bind();
	UpdateShaderConstants(shaderSet.pixelShader);

	if (shaderSet.geometryShader)
	{
		shaderSet.geometryShader->Bind();
		UpdateShaderConstants(shaderSet.geometryShader);
	}

	unsigned instanceSize = sizeof(Instance);
	unsigned instanceCapacity = (unsigned) m_instances.size();
	void* instanceData = m_instances.data();
	if (m_particleInstancingEnabled)
	{
		instanceSize = sizeof(ParticleInstance);
		instanceCapacity = (unsigned) m_particleInstances.size();
		instanceData = m_particleInstances.data();
	}

	if (m_instanceBufferNeedsUpdate)
	{
		if (instancesToDraw > instanceCapacity)
			instancesToDraw = instanceCapacity;

		g_d3dContext->UpdateSubresource(m_instanceBuffer.Get(), 0, nullptr, instanceData, instancesToDraw * instanceSize, 0);
		m_instanceBufferNeedsUpdate = false;
	}

	ID3D11Buffer* buffers[2];
	buffers[0] = m_mesh->GetVertexBuffer();
	buffers[1] = m_instanceBuffer.Get();

	UINT strides[2];
	strides[0] = sizeof(Mesh::Vertex);
	strides[1] = instanceSize;

	UINT offsets[2];
	offsets[0] = 0;
	offsets[1] = 0;

	g_d3dContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	g_d3dContext->IASetIndexBuffer(m_mesh->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);

	if (m_mesh->GetDrawStyle() == Mesh::DS_LINELIST)
		g_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	else
		g_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void DrawCall::UpdateShaderConstants(shared_ptr<Shader> shader)
{
	for(unsigned i=0; i<shader->GetContantBufferCount(); ++i)
	{
		ConstantBuffer& buffer = shader->GetConstantBuffer(i);

		// Grab the camera view and proj settings (which may be different from active view and proj if this is a fullscreen pass)
		XMMATRIX mtxCameraView = m_activeView.mtx;
		Projection cameraProj = m_activeProjection;
		if(!m_sFullscreenPassStates.empty() && m_sFullscreenPassStates.top() == true)	// For fullscreen pass, use the camera view matrix instead of the view matrix
		{
			mtxCameraView = m_mtxCameraView;
			cameraProj = m_cameraProj;
		}

		for(unsigned j=0; j<buffer.constants.size(); ++j)
		{
			Constant& constant = buffer.constants[j];

			XMMATRIX mtx;
			XMVECTOR v;

			switch(constant.id)
			{
			case CONST_WORLD_MATRIX:
				mtx = XMMatrixTranspose(allInstanceWorldTransform);
				memcpy(buffer.staging.get() + constant.startOffset, &mtx, constant.size);
				break;
			case CONST_WORLDVIEWPROJ_MATRIX:
				if (constant.elementCount == 0)
				{
					XMMATRIX view = m_activeView.mtx;
					XMMATRIX proj = m_activeProjection.mtx;
					if (IsRightEyePassActive())
					{
						view = m_activeView.mtxRight;
						proj = m_activeProjection.mtxRight;
					}

					mtx = XMMatrixTranspose(XMMatrixMultiply(allInstanceWorldTransform, XMMatrixMultiply(view, proj)));
					memcpy(buffer.staging.get() + constant.startOffset, &mtx, constant.size);
				}
				else if (constant.elementCount == 2)
				{
					mtx = XMMatrixTranspose(XMMatrixMultiply(allInstanceWorldTransform, XMMatrixMultiply(m_activeView.mtx, m_activeProjection.mtx)));
					memcpy(buffer.staging.get() + constant.startOffset, &mtx, constant.size / 2);

					mtx = XMMatrixTranspose(XMMatrixMultiply(allInstanceWorldTransform, XMMatrixMultiply(m_activeView.mtxRight, m_activeProjection.mtxRight)));
					memcpy(buffer.staging.get() + constant.startOffset + constant.size / 2, &mtx, constant.size / 2);
				}
				break;
			case CONST_VIEWPROJ_MATRIX:
				if (constant.elementCount == 0)
				{
					XMMATRIX view = m_activeView.mtx;
					XMMATRIX proj = m_activeProjection.mtx;
					if (IsRightEyePassActive())
					{
						view = m_activeView.mtxRight;
						proj = m_activeProjection.mtxRight;
					}

					mtx = XMMatrixTranspose(XMMatrixMultiply(view, proj));
					memcpy(buffer.staging.get() + constant.startOffset, &mtx, constant.size);
				}
				else if (constant.elementCount == 2)
				{
					mtx = XMMatrixTranspose(XMMatrixMultiply(m_activeView.mtx, m_activeProjection.mtx));
					memcpy(buffer.staging.get() + constant.startOffset, &mtx, constant.size/2);

					mtx = XMMatrixTranspose(XMMatrixMultiply(m_activeView.mtxRight, m_activeProjection.mtxRight));
					memcpy(buffer.staging.get() + constant.startOffset + constant.size / 2, &mtx, constant.size / 2);
				}
				break;
			case CONST_VIEW_MATRIX:
				if (constant.elementCount == 0)
				{
					XMMATRIX view = m_activeView.mtx;
					XMMATRIX proj = m_activeProjection.mtx;
					if (IsRightEyePassActive())
					{
						view = m_activeView.mtxRight;
						proj = m_activeProjection.mtxRight;
					}

					mtx = XMMatrixTranspose(view);
					memcpy(buffer.staging.get() + constant.startOffset, &mtx, constant.size);
				}
				else if (constant.elementCount == 2)
				{
					mtx = XMMatrixTranspose(m_activeView.mtx);
					memcpy(buffer.staging.get() + constant.startOffset, &mtx, constant.size/2);

					mtx = XMMatrixTranspose(m_activeView.mtxRight);
					memcpy(buffer.staging.get() + constant.startOffset + constant.size / 2, &mtx, constant.size / 2);
				}
				break;
			case CONST_INVVIEW_MATRIX:
				if (constant.elementCount == 0)
				{
					XMMATRIX view = XMMatrixInverse(nullptr, m_activeView.mtx);
					if (IsRightEyePassActive())
						view = XMMatrixInverse(nullptr, m_activeView.mtxRight);

					mtx = XMMatrixTranspose(view);
					memcpy(buffer.staging.get() + constant.startOffset, &mtx, constant.size);
				}
				else if (constant.elementCount == 2)
				{
					mtx = XMMatrixTranspose(XMMatrixInverse(nullptr, m_activeView.mtx));
					memcpy(buffer.staging.get() + constant.startOffset, &mtx, constant.size / 2);

					mtx = XMMatrixTranspose(XMMatrixInverse(nullptr, m_activeView.mtxRight));
					memcpy(buffer.staging.get() + constant.startOffset + constant.size / 2, &mtx, constant.size / 2);
				}
				break;
			case CONST_LIGHTVIEWPROJ_MATRIX:
				mtx = XMMatrixTranspose(m_mtxLightViewProj);
				memcpy(buffer.staging.get() + constant.startOffset, &mtx, constant.size);
				break;
			case CONST_LIGHTPOSV:
				v = XMVector4Transform(vLights[uActiveLightIdx].vLightPosW, mtxCameraView);
				memcpy(buffer.staging.get() + constant.startOffset, &v, constant.size);
				break;
			case CONST_INVVIEWLIGHTVIEWPROJ_MATRIX:
				mtx = XMMatrixTranspose(XMMatrixMultiply(XMMatrixInverse(&v, mtxCameraView), m_mtxLightViewProj));
				memcpy(buffer.staging.get() + constant.startOffset, &mtx, constant.size);
				break;
			case CONST_LIGHT_AMBIENT:
				memcpy(buffer.staging.get() + constant.startOffset, &vAmbient, constant.size);
				break;
			case CONST_NEARPLANEHEIGHT:
				memcpy(buffer.staging.get() + constant.startOffset, &cameraProj.fNearPlaneHeight, constant.size);
				break;
			case CONST_NEARPLANEWIDTH:
				memcpy(buffer.staging.get() + constant.startOffset, &cameraProj.fNearPlaneWidth, constant.size);
				break;
			case CONST_NEARPLANEDIST:
				memcpy(buffer.staging.get() + constant.startOffset, &cameraProj.fNear, constant.size);
				break;
			case CONST_FARPLANEDIST:
				memcpy(buffer.staging.get() + constant.startOffset, &cameraProj.fFar, constant.size);
				break;
			case CONST_PROJECTIONRANGE:
				memcpy(buffer.staging.get() + constant.startOffset, &cameraProj.fRange, constant.size);
				break;
			}
		}

		D3D11_MAPPED_SUBRESOURCE mapped;
		g_d3dContext->Map(buffer.d3dBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, buffer.staging.get(), mapped.RowPitch);
		g_d3dContext->Unmap(buffer.d3dBuffer.Get(), 0);

		if(shader->GetType() == Shader::ST_VERTEX || shader->GetType() == Shader::ST_VERTEX_SPS)
			g_d3dContext->VSSetConstantBuffers(buffer.slot, 1, buffer.d3dBuffer.GetAddressOf());
		else if (shader->GetType() == Shader::ST_PIXEL)
			g_d3dContext->PSSetConstantBuffers(buffer.slot, 1, buffer.d3dBuffer.GetAddressOf());
		else if (shader->GetType() == Shader::ST_GEOMETRY)
			g_d3dContext->GSSetConstantBuffers(buffer.slot, 1, buffer.d3dBuffer.GetAddressOf());
	}
}
