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


#include "Shared.hlsl"

cbuffer g_cbMatrices
{
	float4x4 mtxWorld;
	float4x4 mtxViewProj;
};

cbuffer g_cbLight
{
	float4 vLightPosV;
};

UnlitRasterData main(InstancedVertex vertex)
{
	UnlitRasterData output = (UnlitRasterData)0;

	float4 worldPosition;
	float3 worldNormal;
	CalculateWorldPositionAndNormal(
		vertex,
		mtxWorld,
		worldPosition,
		worldNormal);
	output.projectedPosition = mul(worldPosition, mtxViewProj);

	output.color = vertex.color;
	output.texcoord = vertex.texcoord;

	return output;
}
