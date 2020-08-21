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

#pragma once

#include <DirectXMath.h>
using namespace DirectX;

// Interface used by systems that contain intersectable geometry. Helps maintain code seperation for systems that test for intersections.
class Intersectable
{
public:

	virtual bool TestRayIntersection(XMVECTOR rayOrigin, XMVECTOR rayDirection, float& distance, XMVECTOR& normal) = 0;

};