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


cbuffer g_cbLightData
{
	float4 vAmbient;
};

Texture2D g_tx: register(t0);
SamplerState samplerState: register(s0);

struct PS_INPUT
{
   float4 vProjPos  : SV_POSITION;
   float3 vNormalV	: TEXCOORD0;
   float3 vLightDirV: TEXCOORD1;
   float3 vViewV	: TEXCOORD2;
   float4 color	    : TEXCOORD6;
   float2 vTexCoord : TEXCOORD7;
};

struct PS_OUTPUT
{
   float4 vColor: SV_TARGET;
};

PS_OUTPUT main(PS_INPUT input)
{
   PS_OUTPUT output = (PS_OUTPUT)0;

   float4 textureSample = g_tx.Sample(samplerState, input.vTexCoord);

   float4 vAlbedo = input.color * (1.0f - textureSample.a) + textureSample;
   float4 vFinalAmbient = vAlbedo * vAmbient;
   
   output.vColor = vFinalAmbient + saturate(dot(normalize(input.vLightDirV), normalize(input.vNormalV))) * vAlbedo;
   
   return output;
}