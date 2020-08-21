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


#include "StringHelpers.h"

std::string Utf16ToUtf8(const wchar_t* text)
{
    char buffer[1024];

    sprintf_s(
        buffer,
        "%S",
        text);

    return std::string(buffer);
}

std::string Utf16ToUtf8(const std::wstring& text)
{
    return Utf16ToUtf8(
        text.c_str());
}