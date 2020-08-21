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

struct InstancedVertex
{
	float4 position			: POSITION;
	float3 normal			: NORMAL;
	float2 texcoord			: TEXCOORD0;

	float4 worldMatrixRow0	: WORLDMATRIX_ROW0;
	float4 worldMatrixRow1	: WORLDMATRIX_ROW1;
	float4 worldMatrixRow2	: WORLDMATRIX_ROW2;
	float4 worldMatrixRow3	: WORLDMATRIX_ROW3;
	float4 color			: COLOR;

	uint   instId		: SV_InstanceID;
};

struct InstancedParticleVertex
{
	float4 position			: POSITION;
	float3 normal			: NORMAL;
	float2 texcoord			: TEXCOORD0;

	float4 translationScale	: TRANSLATIONSCALE;		// x,y,z are the translation, w is the scale
	float4 color			: COLOR;

	uint   instId			: SV_InstanceID;
};

struct LitRasterData
{
	float4 projectedPosition	: SV_POSITION;
	float3 viewSpaceNormal		: TEXCOORD0;
	float3 viewSpaceLightDir	: TEXCOORD1;
	float3 viewSpaceViewDir		: TEXCOORD2;
	float4 color				: TEXCOORD6;
	float2 texcoord				: TEXCOORD7;
};

struct LitRasterDataSPS
{
	float4 projectedPosition	: SV_POSITION;
	float3 viewSpaceNormal		: TEXCOORD0;
	float3 viewSpaceLightDir	: TEXCOORD1;
	float3 viewSpaceViewDir		: TEXCOORD2;
	float4 color				: TEXCOORD6;
	float2 texcoord				: TEXCOORD7;

	uint   rtvId         : SV_RenderTargetArrayIndex;
};

struct UnlitRasterData
{
	float4 projectedPosition	: SV_POSITION;
	float4 color 				: TEXCOORD6;
	float2 texcoord				: TEXCOORD7;
};

struct UnlitRasterDataSPS
{
	float4 projectedPosition	: SV_POSITION;
	float4 color 				: TEXCOORD6;
	float2 texcoord				: TEXCOORD7;

	uint   rtvId         : SV_RenderTargetArrayIndex;
};

void CalculateWorldPositionAndNormal(
	InstancedVertex vertex,
	float4x4 worldTransform,
	inout float4 worldPosition,
	inout float3 worldNormal)
{
	float4x4 mtxWorldFinal;
	mtxWorldFinal[0] = vertex.worldMatrixRow0;
	mtxWorldFinal[1] = vertex.worldMatrixRow1;
	mtxWorldFinal[2] = vertex.worldMatrixRow2;
	mtxWorldFinal[3] = vertex.worldMatrixRow3;
	mtxWorldFinal = mul(mtxWorldFinal, worldTransform);
	worldPosition = mul(vertex.position, mtxWorldFinal);
	worldNormal = mul(vertex.normal, (float3x3)mtxWorldFinal);
}

float4 CalculateWorldPosition_Particle(
	float3 localVertexPosition,
	float4 particleTranslationScale,
	float4x4 worldTransform,
	float3 worldCameraPosition)
{
	float3 worldParticlePosition = (float3) mul(float4(particleTranslationScale.xyz, 1.0f), worldTransform);
	float3 worldDirectionToCamera = normalize(worldCameraPosition - worldParticlePosition);

	float3x3 worldRotation;
	worldRotation[0] = normalize(cross(float3(0.0f, 1.0f, 0.0f), worldDirectionToCamera));
	worldRotation[1] = normalize(cross(worldDirectionToCamera, (float3)worldRotation[0]));
	worldRotation[2] = worldDirectionToCamera;

	float4 worldVertexPosition;
	worldVertexPosition.xyz = mul(localVertexPosition * particleTranslationScale.w, worldRotation) + worldParticlePosition;
	worldVertexPosition.w = 1.0f;
	return worldVertexPosition;
}

float3 CalculateNormalFromDepthImage(
	Texture2D<uint> depthTexture,
	uint pixelIndex,
	float physicalPixelWidth)
{
	uint width = 0, height = 0;
	depthTexture.GetDimensions(width, height);
	uint row = pixelIndex / width;
	uint col = pixelIndex % width;

	float depth = depthTexture[uint2(col, row)] / 1000.0f;
	float depthX1 = depthTexture[uint2(col - 1, row)] / 1000.0f;
	float depthX2 = depthTexture[uint2(col + 1, row)] / 1000.0f;
	float depthY1 = depthTexture[uint2(col, row - 1)] / 1000.0f;
	float depthY2 = depthTexture[uint2(col, row + 1)] / 1000.0f;
	float lateralDistance = physicalPixelWidth * 2.0f;

	if (depthX1 == 0.0f)
		depthX1 = depth;
	if (depthX2 == 0.0f)
		depthX2 = depth;
	if (depthY1 == 0.0f)
		depthY1 = depth;
	if (depthY2 == 0.0f)
		depthY2 = depth;

	float3 xDirection = normalize(float3(lateralDistance, 0.0f, depthX2 - depthX1));
	float3 yDirection = normalize(float3(0.0f, -lateralDistance, depthY2 - depthY1));
	return normalize(cross(xDirection, yDirection));
}

void CalculateViewAndLightDir_ViewSpace(
	float4 worldPosition,
	float3 worldNormal,
	float4 viewSpaceLightPosition,
	float4x4 viewTransform,
	inout float3 viewSpaceNormal,
	inout float3 viewSpaceViewDir,
	inout float3 viewSpaceLightDir)
{
	float4 viewSpacePosition = mul(worldPosition, viewTransform);

	viewSpaceNormal = mul(worldNormal, (float3x3)viewTransform);
	viewSpaceViewDir = -normalize(viewSpacePosition.xyz);
	viewSpaceLightDir = (viewSpaceLightPosition - viewSpacePosition).xyz;
}

float CalculateDiffuseIntensity(
	float3 normal,
	float3 lightDir)
{
	return saturate(dot(normalize(normal), normalize(lightDir)));
}
