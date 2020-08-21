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

#ifndef _DRAWCALL
#define _DRAWCALL

#include <winapifamily.h>
#include <wrl/client.h>

#if !defined WINAPI_FAMILY || WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP
#define USE_DESKTOP_D3D				// Desktop app
#elif WINAPI_FAMILY == WINAPI_FAMILY_PC_APP
#define USE_WINRT_D3D				// Windows Store app
#include <intrin.h>
#include <winrt/Windows.UI.Core.h>
#endif

#include <d3d11_2.h>
#include <D3Dcompiler.h>
#include <d3d11shader.h>
#include <d2d1_2.h>
#include <dwrite_2.h>
#include <dxgi1_3.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <DirectXMath.h>
#include <DirectXCollision.h>
using namespace DirectX;

#include <vector>
#include <string>
#include <stack>
#include <map>
#include <memory>

#define RENDER_TARGET_COUNT 8

enum RenderPass
{
	RENDERPASS_COLOR = 0,
	RENDERPASS_SHADOW = 1,
	RENDERPASS_DEPTH = 2,
	RENDERPASS_NORMALDEPTH = 3,
	RENDERPASS_DEFERRED = 4,

	RENDERPASS_CUSTOM0 = 100,
	RENDERPASS_CUSTOM1 = 101,
	RENDERPASS_CUSTOM2 = 102,
	RENDERPASS_CUSTOM3 = 103,
	RENDERPASS_CUSTOM4 = 104,
};

enum class HorizontalAlignment
{
	Left,
	Center,
	Right
};

enum class VerticalAlignment
{
	Top,
	Middle,
	Bottom
};

enum class TextColor
{
	White,
	Gray,
	Black
};

enum class MapType
{
	Read,
	Write,
	ReadWrite
};

struct Image
{
	std::vector<std::unique_ptr<unsigned char>> mips;
	unsigned width;
	unsigned height;
	unsigned bytesPerPixel;
	unsigned rowPitch;			// Size of each row in bytes
	unsigned mipCount;
	DXGI_FORMAT format;

	Image()
	{
		width = height = bytesPerPixel = rowPitch = 0;
		mipCount = 1;
		format = DXGI_FORMAT_UNKNOWN;
	}
};

Image CreateImageFromFile(std::string filename);

class Texture2D
{
public:

	// Creates a texture that can be used as a rendertarget
	Texture2D(unsigned width = 512, unsigned height = 512, DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM);

	// Create a texture using the given D3D texture
	Texture2D(ID3D11Texture2D* pD3DTexture);

	// Creates a static texture from file
	Texture2D(std::string filename);

	// Re-initializes the texture arond the given D3D texture
	void SetD3DTexture(ID3D11Texture2D* pD3DTexture);

	~Texture2D();
	void Reset();

	bool UploadData(unsigned char* pData, unsigned size);
	void SaveToFile(std::string filename);
	
	void BindAsVertexShaderResource(unsigned slot);
	void BindAsPixelShaderResource(unsigned slot);

	void Clear(float r, float g, float b, float a);
	void Clear(const XMVECTOR& color);

	void* Map(MapType mapType);
	void Unmap();

	void* GetDataPtr();		// When mapped, will return the data pointer.
	unsigned GetPitch();	// When mapped, will be equal to DX row pitch.	When not mapped, will be 0.

	unsigned GetWidth() { return m_width; }
	unsigned GetHeight() { return m_height; }
	float GetAspect() { return m_width / (float)m_height; }

	const D3D11_VIEWPORT& GetViewport();
	void SetViewport(D3D11_VIEWPORT viewport);
	void SetViewport(unsigned width, unsigned height, float minDepth = 0.f, float maxDepth = 1.f, float topLeftX = 0.f, float topLeftY = 0.f);

	void EnableComparisonSampling(bool enabled);

	// Get whether this is a stereo render target (array texture with one texture per eye)
	bool IsStereo() { return m_isStereo; }

	// Get whether this texture can be used as a render target
	bool IsRenderTarget() { return m_isRenderTarget; }

	ID3D11Texture2D* GetD3DTexture() { return m_texture.Get(); }
	ID3D11Texture2D* GetD3DDepthStencilTexture(){return m_depthStencilTexture.Get();}
	ID3D11ShaderResourceView* GetShaderResourceView() { return m_shaderResourceView.Get(); }
	ID3D11RenderTargetView* GetRenderTargetView() { return m_renderTargetView.Get(); }
	ID3D11RenderTargetView* GetRenderTargetViewRight() { return m_renderTargetViewRight.Get(); }
	ID3D11DepthStencilView* GetDepthStencilView() { return m_depthStencilView.Get(); }
	ID3D11DepthStencilView* GetDepthStencilViewRight() { return m_depthStencilViewRight.Get(); }

	// Returns a bitmap that D2D can use to draw to this texture.  It is only created if the texture is a rendertarget
	//	and is a supported pixel format.
	ID2D1Bitmap1*			GetD2DTargetBitmap() { return m_d2dTargetBitmap.Get(); }

private:
	
	unsigned m_width, m_height;
	DXGI_FORMAT m_format;

	D3D11_VIEWPORT m_viewport;

	bool m_comparisonSamplingEnabled;
	bool m_isStereo;
	bool m_isRenderTarget;

	int m_mappedCount;
	MapType m_currentMapType;
	D3D11_MAPPED_SUBRESOURCE m_mapped;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetViewRight;	// For rendering the right eye during multi-pass stereo rendering (not used for single-pass stereo)

	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthStencilTexture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilViewRight;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stagingTexture;

	Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_d2dTargetBitmap;

	void InitStagingTexture();
};

enum ConstantID
{
	CONST_WORLD_MATRIX,
	CONST_WORLDVIEWPROJ_MATRIX,
	CONST_VIEWPROJ_MATRIX,
	CONST_VIEW_MATRIX,
	CONST_INVVIEW_MATRIX,
	CONST_LIGHTVIEWPROJ_MATRIX,
	CONST_LIGHTPOSV,
	CONST_INVVIEWLIGHTVIEWPROJ_MATRIX,
	CONST_LIGHT_AMBIENT,
	CONST_NEARPLANEHEIGHT,
	CONST_NEARPLANEWIDTH,
	CONST_NEARPLANEDIST,
	CONST_FARPLANEDIST,
	CONST_PROJECTIONRANGE,
	CONST_COUNT,
};

static const char* g_vContantIDStrings[] = 
{
	"mtxWorld",
	"mtxWorldViewProj",
	"mtxViewProj",
	"mtxView",
	"mtxInvView",
	"mtxLightViewProj",
	"vLightPosV",
	"mtxInvViewLightViewProj",
	"vAmbient",
	"fNearPlaneHeight",
	"fNearPlaneWidth",
	"fNear",
	"fFar",
	"fRange",
};

struct Constant
{
	unsigned id;			// Unique identifier so we know what to set it to
	unsigned startOffset;
	unsigned size;
	unsigned elementCount;	// Number of elements if array variable. 0 if not an array.
};

class ConstantBuffer
{
public:

	unsigned slot;
	unsigned size;

	std::unique_ptr<unsigned char> staging;
	Microsoft::WRL::ComPtr<ID3D11Buffer> d3dBuffer;

	std::vector<Constant> constants;
};

class Shader
{
public:

	enum ShaderType
	{
		ST_VERTEX,
		ST_PIXEL,
		ST_GEOMETRY,
		ST_VERTEX_SPS,	// Vertex shader varient for single-pass stereo rendering
	};

	Shader(ShaderType type, std::string filename);

	void Bind();

	ShaderType GetType(){return m_type;}

	void* GetBytecode();
	unsigned long GetBytecodeSize();

	unsigned GetContantBufferCount();
	ConstantBuffer& GetConstantBuffer(unsigned uIdx);

private:

	ShaderType m_type;

	std::unique_ptr<unsigned char> m_shaderBlob;
	unsigned long m_shaderBlobSize;

	// Only one of these shader pointers will be non-null
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> m_geometryShader;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShaderSPS;

	std::vector<ConstantBuffer> m_vConstantBuffers;
};

class Mesh
{
public:

	enum MeshType
	{
		MT_EMPTY,
		MT_PLANE,       // -0.5..0.5 in {x,z}; normal points to +y
		MT_UIPLANE,     // -0.5..0.5 in {x,y}; normal points to +z
		MT_ZERO_ONE_PLANE_XY_NEGATIVE_Z_NORMAL,  // 0..1 in {x,y}; normal points to -z; vertex coords == tex coords.
		MT_BOX,
		MT_CYLINDER,
		MT_ROUNDEDBOX,
		MT_SPHERE
	};

	enum DrawStyle
	{
		DS_TRILIST,
		DS_LINELIST,
	};

	struct Vertex
	{
		XMVECTOR position;
		XMVECTOR normal;
		XMFLOAT2 texcoord;

		bool operator==(const Vertex& b) const
		{
			uint32_t result;
			XMVectorEqualR(&result, position, b.position);
			bool positionEqual = XMComparisonAllTrue(result);
			XMVectorEqualR(&result, normal, b.normal);
			bool normalEqual = XMComparisonAllTrue(result);

			if(positionEqual && normalEqual && texcoord.x == b.texcoord.x && texcoord.y == b.texcoord.y)
				return true;
			else
				return false;
		}
	};

	struct BoundingBoxNode
	{
		BoundingBox boundingBox;

		// Indices representing all the triangles that intersect this bounding box
		//	These index directly into Mesh::m_vertices
		std::vector<unsigned> indicesContained;

		std::vector<BoundingBoxNode> children;

		static const unsigned targetTriangleCount = 250;

		void GenerateChildNodes(const std::vector<Mesh::Vertex>& vertices, const std::vector<unsigned>& indices, unsigned currentDepth);
		bool TestRayIntersection(const std::vector<Vertex>& vertices, const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, const XMMATRIX& worldTransform, float &distance, XMVECTOR& normalInLocalSpace, float maxDistance = std::numeric_limits<float>::max(), bool returnFurthest = false);
	};

	struct Disc
	{
		XMVECTOR center;		// Position of the center of the disc
		XMVECTOR upDir;			// Direction that defines local Y axis (the extrusion axis)
		XMVECTOR rightDir;		// Direction that defines local X axis
		float radiusRight;		// Radius in the X axis
		float radiusBack;		// Radius in the Z axis
	};

	enum class DiscMode
	{
		Circle,
		Square,
		RoundedSquare
	};

	Mesh(MeshType type = MT_EMPTY);							// Creates a procedural mesh, empty by default
	Mesh(Mesh::Vertex* pVertices, unsigned vertexCount);	// Creates a mesh out of a list of vertices (nullptrs will cause empty mesh)
	Mesh(Mesh::Vertex* pVertices, unsigned vertexCount, unsigned* pIndices, unsigned indexCount);	// Creates a mesh out of a list of vertices (nullptrs will result in empty mesh)
	Mesh(std::string filename);								// Creates a mesh by loading from file

	void LoadBox(const float width, const float height, const float depth);
	void LoadPlane(MeshType type);	// Takes MT_PLANE or MT_UIPLANE or MT_ZERO_ONE_PLANE_XY
	void LoadCylinder(const float radius, const float height);
	void LoadRoundedBox(const float width, const float height, const float depth);
	void LoadSphere();

	// Generates extruded geometry along a list of discs
	//  If the first disc has 0 radius, the geometry will be capped on both sides
	//  If the first disc has the same center point as the second disc, the caps will not be smoothed
	void AppendGeometryForDiscs(const std::vector<Disc>& discs, const unsigned segmentCount, const DiscMode discMode = DiscMode::Circle);

	bool TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, const XMMATRIX& worldTransform, float &distance, XMVECTOR &normal, float maxDistance = std::numeric_limits<float>::max(), bool returnFurthest = false);
	bool TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, const XMMATRIX& worldTransform, float& distance);
	bool TestPointInside(const XMVECTOR& pointInWorldSpace, const XMMATRIX& worldTransform);	// This currently only tests against the bounding box
	const BoundingBox& GetBoundingBox();

	void SetDrawStyle(DrawStyle drawStyle){m_drawStyle = drawStyle;}
	DrawStyle GetDrawStyle() { return m_drawStyle; }
	
	// Calling these will trigger a d3d buffer update the next time GetVertexBuffer/GetIndexBuffer is called
	void Clear();
	void BakeTransform(const XMMATRIX& transform);
	void GenerateSmoothNormals();
	void UpdateVertices(Vertex* pVertices, unsigned vertexCount);
	void UpdateVertices(Vertex* pVertices, unsigned vertexCount, unsigned* pIndices, unsigned indexCount);
	std::vector<Vertex>& GetVertices();
	std::vector<unsigned>& GetIndices();

	unsigned GetVertexCount() { return (unsigned) m_vertices.size(); }
	unsigned GetIndexCount() { return (unsigned) m_indices.size(); }
	bool IsEmpty() { return m_indices.empty() || m_vertices.empty(); }

	ID3D11Buffer* GetVertexBuffer();
	ID3D11Buffer* GetIndexBuffer();

	void UpdateBoundingBox();

	bool SaveToFile(const std::string& filename);

private:

	DrawStyle m_drawStyle;

	BoundingBoxNode m_boundingBoxNode;

	std::vector<Vertex> m_vertices;
	std::vector<unsigned> m_indices;

	bool m_d3dBuffersNeedUpdate;
	bool m_boundingBoxNeedsUpdate;

	D3D11_BUFFER_DESC m_d3dVertexBufferDesc;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_d3dVertexBuffer;

	D3D11_BUFFER_DESC m_d3dIndexBufferDesc;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_d3dIndexBuffer;
	
	void UpdateD3DBuffers();
};

class DrawCall
{

public:
	static const float DefaultFontSize;

	struct Instance
	{
		XMMATRIX worldTransform;
		XMVECTOR color;

		Instance()
		{
			worldTransform = XMMatrixIdentity();
			color = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
		}
	};

	struct ParticleInstance
	{
		XMVECTOR translationScale;	// XYZ is position, W is scale
		XMVECTOR color;

		ParticleInstance()
		{
			translationScale = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
			color = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
		}
	};

	struct RenderPassDesc
	{
		std::string vertexShaderFilename;
		std::string pixelShaderFilename;
		std::string geometryShaderFilename;	// Can be empty string
		unsigned renderPassIndex;
	};
 
	struct ShaderSet
	{
		std::shared_ptr<Shader> vertexShader;
		std::shared_ptr<Shader> pixelShader;
		std::shared_ptr<Shader> geometryShader;
		std::shared_ptr<Shader> vertexShaderSPS;
	};

	struct Light
	{
		XMMATRIX mtxViewProj;
		XMVECTOR vLightPosW;
		XMVECTOR vLightAtW;
	};

	struct View
	{
		XMMATRIX mtx;
		XMMATRIX mtxRight;	// Used for right eye during stereo rendering
	};

	struct Projection
	{
		XMMATRIX mtx;
		XMMATRIX mtxRight;	// Used for right eye during stereo rendering
		float fNearPlaneHeight;
		float fNearPlaneWidth;
		float fNear;
		float fFar;
		float fRange;			// zfar / (zfar-znear);
	};

	enum BlendState
	{
		BLEND_NONE,
		BLEND_ALPHA,
		BLEND_ADDITIVE,
		BLEND_COLOR_DISABLED,
	};

	//
	// Global stuff that affects all draw calls
	//


public:		

	static XMVECTOR vAmbient;
	static const unsigned kMaxLights = 32;
	static Light vLights[kMaxLights];
	static unsigned uLightCount;
	static unsigned uActiveLightIdx;

	// Initializes DrawCall for use on an offscreen render target.
	// You can set a swap chain later with one of two options:
	//	-Calling InitializeSwapChain
	//	-Creating it elsewhere and then setting it via SetBackBuffer
	static bool Initialize();

#ifdef USE_WINRT_D3D
	static bool InitializeSwapChain(unsigned width, unsigned height, winrt::Windows::UI::Core::CoreWindow const& window);
#else
	static bool InitializeSwapChain(unsigned width, unsigned height, HWND hWnd, bool fullscreen = false);
#endif

	static bool ResizeSwapChain(unsigned newWidth, unsigned newHeight);

	static void Uninitialize();

	static ID3D11Device* GetD3DDevice();
	static ID3D11DeviceContext* GetD3DDeviceContext();
	static IDXGISwapChain1* GetD3DSwapChain();

	static bool IsSinglePassSteroSupported();			// Whether or not the hardware supports single pass stereo
	static bool IsSinglePassSteroEnabled();				// Whether or not single pass stero is currently enabled
	static void EnableSinglePassStereo(bool enabled);

	static void PushView(const XMMATRIX& mtxLeft, const XMMATRIX& mtxRight);
	static void PushView(const XMVECTOR& vEye, const XMVECTOR vAt, const XMVECTOR &vUp);
	static void PopView();
	static XMMATRIX GetView();

	static void PushProj(const XMMATRIX& mtxLeft, const XMMATRIX& mtxRight);
	static void PushProj(float fFOV, float fAspect, float fNear, float fFar);
	static void PushProjOrtho(float fFOV, float fAspect, float fNear, float fFar);
	static void PopProj();
	static XMMATRIX GetProj();

	static void StoreCurrentViewProjAsLightViewProj();	// Stores the current view and proj as the light's view proj
	static void StoreCurrentViewAsCameraView();			// Stores the current camera view matrix so it can be used in fullscreen passes that need stuff in view space (such as view-space light positions)

	static std::shared_ptr<Texture2D> GetBackBuffer();
	static void SetBackBuffer(std::shared_ptr<Texture2D> backBuffer);
	
	// These variants of SetBackBuffer automatically generate a Texture2D wrapper and cache as needed
	static void SetBackBuffer(ID3D11Texture2D* pD3DBackBuffer, const D3D11_VIEWPORT& viewport);
	static void SetBackBuffer(ID3D11Texture2D* pD3DBackBuffer);
	static void SetBackBuffer(IDXGISwapChain1* pSwapChain);
	
	static std::shared_ptr<Texture2D> GetCurrentRenderTarget();
	static void PushRenderPass(unsigned renderPassIndex, std::shared_ptr<Texture2D> renderTarget);
	static void PushRenderPass(unsigned renderPassIndex, std::vector<std::shared_ptr<Texture2D>> renderTargets);
	static void PopRenderPass();

	static void PushRightEyePass(unsigned renderPassIndex, std::shared_ptr<Texture2D> renderTarget);
	static void PopRightEyePass();

	static void PushShadowPass(std::shared_ptr<Texture2D> depthMap, unsigned renderPassIndex, unsigned uLightIdx=0);
	static void PopShadowPass();

	static void PushUIPass(std::shared_ptr<Texture2D> renderTarget = nullptr);
	static void PopUIPass();

	static void PushFullscreenPass(std::shared_ptr<Texture2D> renderTarget = nullptr);
	static void PopFullscreenPass();

	static void PushAlphaBlendState(BlendState blendState);
	static void PopAlphaBlendState();

	static void PushDepthTestState(bool bEnabled);
	static void PopDepthTestState();

	static void PushBackfaceCullingState(bool bEnabled);
	static void PopBackfaceCullingState();

	static void AddGlobalRenderPass(const std::string& vertexShaderFilename, const std::string& pixelShaderFilename, const unsigned renderPassIndex);
	static std::shared_ptr<Shader> LoadShaderUsingShaderStore(Shader::ShaderType type, const std::string& filename);

	static void Present();

	// Draw text using the current redertarget
	static void DrawText(const std::string& text, const D2D_RECT_F& layoutRect, float fontSize, HorizontalAlignment horizontalAlignment, VerticalAlignment verticalAlignment, TextColor textColor = TextColor::White);
	static void DrawText(const std::string &text, float x, float y, float fontSize = DefaultFontSize, TextColor textColor = TextColor::White);	// Coordinates specify the top-left corner of the text

	// These functions build a transform that orients the mesh such that the Y axis points along the line, and the X axis points in the right direction
	static XMMATRIX DrawCall::CalculateWorldTransformForLine(const XMVECTOR& startPosition, const XMVECTOR& endPosition, const float radius, const XMVECTOR& rightDirection);
	static XMMATRIX DrawCall::CalculateWorldTransformForLine(const XMVECTOR& startPosition, const XMVECTOR& endPosition, const float radius);

private:
	static XMMATRIX m_mtxLightViewProj;
	static XMMATRIX m_mtxCameraView;	// Used during fullscreen passes when the original camera view is needed
	static Projection m_cameraProj;		// Used during fullscreen passes when the original camera projection is needed

	static bool m_singlePassStereoSupported;
	static bool m_singlePassStereoEnabled;

	static std::stack<View> m_viewStack;
	static View m_activeView;

	static std::stack<Projection> m_projectionStack;
	static Projection m_activeProjection;

	static std::shared_ptr<Texture2D> m_backBuffer;
	static std::map<intptr_t, std::shared_ptr<Texture2D>> m_cachedBackBuffers;
	static std::stack<std::vector<std::shared_ptr<Texture2D>>> m_renderTargetStack;

	static std::stack<unsigned> m_renderPassIndexStack;
	static unsigned m_activeRenderPassIndex;

	static std::stack<BlendState> m_sAlphaBlendStates;
	static std::stack<bool> m_sDepthTestStates;
	static std::stack<bool> m_sBackfaceCullingStates;

	static std::stack<bool> m_sRightEyePassStates;
	static std::stack<bool> m_sFullscreenPassStates;

	// Global render passes, which are automatically added to every draw call at creation time
	static std::vector<RenderPassDesc> m_vGlobalRenderPasses;

	// Global store of all shaders, so we don't load a shader more than once.
	static std::map<std::string, std::shared_ptr<Shader>> m_shaderStore;

	// Pre-made states for various tasks
	static Microsoft::WRL::ComPtr<ID3D11BlendState> m_d3dAlphaBlendEnabledState;
	static Microsoft::WRL::ComPtr<ID3D11BlendState> m_d3dAdditiveBlendEnabledState;
	static Microsoft::WRL::ComPtr<ID3D11BlendState> m_d3dColorWriteDisabledState;
	static Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_d3dDepthTestDisabledState;
	static Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_d3dBackfaceCullingDisabledState;

	static void SetCurrentRenderTargetsOnD3DDevice();
	static bool IsRightEyePassActive();

	//
	// Local stuff that affects only this draw call
	//

public:

	DrawCall(const std::string& vertexShaderFilename, const std::string& pixelShaderFilename, std::shared_ptr<Mesh> mesh = nullptr, const std::string& geometryShaderFilename = "");
	DrawCall(const std::string& vertexShaderFilename, const std::string& pixelShaderFilename, Mesh::MeshType meshType, const std::string& geometryShaderFilename = "");
	DrawCall(const std::string& vertexShaderFilename, const std::string& pixelShaderFilename, const std::string& modelFilename, const std::string& geometryShaderFilename = "");

	void AddRenderPass(const std::string& vertexShaderFilename, const std::string& pixelShaderFilename, unsigned renderPassIndex, const std::string& geometryShaderFilename = "");

	unsigned GetInstanceCapacity();
	void SetInstanceCapacity(unsigned instanceCapacity, bool enableParticleInstancing = false);
	Instance* GetInstanceBuffer();
	ParticleInstance* GetParticleInstanceBuffer();
	void SetWorldTransform(const XMMATRIX& worldTransform, unsigned instanceIndex = 0);
	XMMATRIX GetWorldTransform(unsigned instanceIndex = 0) const;
	void SetColor(const XMVECTOR& color, unsigned instanceIndex = 0);

	bool TestPointInside(const XMVECTOR& pointInWorldSpace, const unsigned instanceIndex = 0);
	bool TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, float &distance, XMVECTOR &normal, const unsigned instanceIndex = 0);
	bool TestRayIntersection(const XMVECTOR& rayOriginInWorldSpace, const XMVECTOR& rayDirectionInWorldSpace, float& distance, const unsigned instanceIndex = 0);

	std::shared_ptr<Shader> GetVertexShader(unsigned renderPassIndex = 0);
	std::shared_ptr<Shader> GetPixelShader(unsigned renderPassIndex = 0);
	std::shared_ptr<Shader> GetGeometryShader(unsigned renderPassIndex = 0);
	std::shared_ptr<Shader> GetVertexShaderSPS(unsigned renderPassIndex = 0);

	std::shared_ptr<Mesh> GetMesh() { return m_mesh; }
	void SetMesh(std::shared_ptr<Mesh> mesh) { m_mesh = mesh; }

	void Draw(unsigned instancesToDraw = 1);

	XMMATRIX allInstanceWorldTransform;	// Global world transform that will be applied to all instances

private:

	void SetupDraw(unsigned instancesToDraw);
	void UpdateShaderConstants(std::shared_ptr<Shader> pShader);

	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_instancingLayout;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_instancingLayoutSPS;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_instanceBuffer;
	bool m_instanceBufferNeedsUpdate;
	bool m_particleInstancingEnabled;

	std::vector<Instance> m_instances;
	std::vector<ParticleInstance> m_particleInstances;

	std::shared_ptr<Mesh> m_mesh;
	std::map<unsigned, ShaderSet> m_shaderSets;
};

#endif
