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

#pragma once

#include <intrin.h>
#include <winrt/Windows.Storage.h>
#include <fstream>
#include <vector>

namespace Io
{	
	// Class to create tarball, which allows for incremental
	// streaming of files into the archive
	class Tarball
	{
	public:
		Tarball(const std::wstring& tarballFileName);
		~Tarball();

		// Close the tarball
		void Close();

		// Add a file to the tarball
		void AddFile(const std::wstring& fileName, const uint8_t* fileData, const size_t fileSize);

	private:
		// The file handler to the tarball
		std::ofstream m_tarballFile;
	};
}