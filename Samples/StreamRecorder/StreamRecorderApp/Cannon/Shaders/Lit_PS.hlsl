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

cbuffer g_cbLightData
{
	float4 vAmbient;
};

float4 main(LitRasterData input) : SV_TARGET
{
	float4 albedo = input.color;
	float intensity = vAmbient.x + CalculateDiffuseIntensity(input.viewSpaceNormal, input.viewSpaceLightDir);

	float4 outputColor;
	outputColor.rgb = albedo.rgb * intensity;
	outputColor.a = albedo.a;
	return outputColor;
}
