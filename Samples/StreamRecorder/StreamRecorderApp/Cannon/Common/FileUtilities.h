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

#include <wrl/client.h>

#include <cstdio>
#include <string>
#include <memory>
#include <vector>

inline std::string GetFilenameExtension(const std::string& filename)
{
	auto pos = filename.find_last_of('.');
	if (pos == std::string::npos || pos + 1 == filename.size())
		return std::string();
	else
		return filename.substr(pos + 1, filename.size() - pos - 1);
}

inline std::string RemoveFilenameExtension(const std::string& filename)
{
	auto pos = filename.find_last_of('.');
	if (pos == std::string::npos)
		return filename;
	else
		return filename.substr(0, pos);
}

inline std::string GetFilenamePath(const std::string& filename)
{
	auto pos = filename.find_last_of('\\');
	if (pos == std::string::npos)
		pos = filename.find_last_of('/');

	if (pos == std::string::npos)
		return std::string();
	else
		return filename.substr(0, pos);
}

inline std::wstring StringToWideString(const std::string& sourceString)
{
	size_t destinationSizeInWords = sourceString.length() + 1;
	std::unique_ptr<wchar_t> wideString(new wchar_t[destinationSizeInWords]);
	size_t nConverted = 0;
	mbstowcs_s(&nConverted, wideString.get(), destinationSizeInWords, sourceString.data(), _TRUNCATE);
	return std::wstring(wideString.get());
}

inline std::string WideStringToString(const std::wstring& sourceString)
{
	size_t destinationSizeInBytes = (sourceString.length() + 1) * 4;
	std::unique_ptr<char> narrowString(new char[destinationSizeInBytes]);
	size_t nConverted = 0;
	wcstombs_s(&nConverted, narrowString.get(), destinationSizeInBytes, sourceString.data(), _TRUNCATE);
	return std::string(narrowString.get());
}

inline std::string GetExecutablePath()
{
	wchar_t wideExecutableFilename[MAX_PATH];
	GetModuleFileNameW(nullptr, wideExecutableFilename, MAX_PATH);
	return GetFilenamePath(WideStringToString(wideExecutableFilename));
}

inline FILE* OpenFile(const std::string& filename, const char* mode)
{
	std::wstring wideFilename = StringToWideString(filename);
	std::wstring wideMode = StringToWideString(std::string(mode));

	FILE* pFile = nullptr;

	// First try the filename as-is
	_wfopen_s(&pFile, wideFilename.c_str(), wideMode.c_str());
	if (pFile)
		return pFile;

	// Then try the filename relative to the executable location
	std::wstring exePath = StringToWideString(GetExecutablePath());
	_wfopen_s(&pFile, (exePath + L"/" + wideFilename).c_str(), wideMode.c_str());
	if (pFile)
		return pFile;
	
	return nullptr;
}

inline bool FileExists(const std::string& filename)
{
	FILE* pFile = OpenFile(filename, "rb");

	if (pFile)
	{
		fclose(pFile);
		return true;
	}
	else
	{
		return false;
	}
}

template<typename T>
inline void WriteValueToBuffer(unsigned char** pWritePtr, const T& value)
{
	memcpy(*pWritePtr, &value, sizeof(value));
	*pWritePtr += sizeof(value);
}

template<typename T>
inline void ReadValueFromBuffer(const unsigned char** pReadPtr, T& value)
{
	memcpy(&value, *pReadPtr, sizeof(value));
	*pReadPtr += sizeof(value);
}

template<typename T>
inline unsigned GetSerializedVectorSize(const std::vector<T>& value)
{
	return sizeof(unsigned) + sizeof(T) * (unsigned)value.size();
}

template<typename T>
inline void WriteVectorToBuffer(unsigned char** pWritePtr, const std::vector<T>& value)
{
	unsigned size = (unsigned)value.size() * sizeof(T);
	WriteValueToBuffer(pWritePtr, size);

	memcpy(*pWritePtr, value.data(), size);
	*pWritePtr += size;
}

template<typename T>
inline void ReadVectorFromBuffer(const unsigned char** pReadPtr, std::vector<T>& value)
{
	unsigned size = 0;
	ReadValueFromBuffer(pReadPtr, size);

	value.resize(size / sizeof(T));
	memcpy(value.data(), *pReadPtr, size);
	*pReadPtr += size;
}

inline unsigned GetSerializedStringSize(const std::string& value)
{
	return sizeof(unsigned) + (unsigned)value.size();
}

inline void WriteStringToBuffer(unsigned char** pWritePtr, const std::string& value)
{
	unsigned size = (unsigned)value.size();
	WriteValueToBuffer(pWritePtr, size);

	memcpy(*pWritePtr, value.data(), size);
	*pWritePtr += size;
}

inline void ReadStringFromBuffer(const unsigned char** pReadPtr, std::string& value)
{
	unsigned size = 0;
	ReadValueFromBuffer(pReadPtr, size);

	value.resize(size);
	memcpy(value.data(), *pReadPtr, size);
	*pReadPtr += size;
}
