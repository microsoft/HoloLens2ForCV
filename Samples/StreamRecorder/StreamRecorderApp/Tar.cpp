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

#include <cassert>
#include <chrono>
#include <cstdint>
#include <ios>
#include <string>

#include "StringHelpers.h"
#include "Tar.h"
#include "TimeConverter.h"

namespace Io
{    
    //
    // TAR (Tape Archive) header.
    //
    // See https://en.wikipedia.org/wiki/Tar_(computing) for details.
    //
#pragma pack (push, 1)
    struct TarHeader
    {
        TarHeader()
            : FileName()
            , FileMode()
            , OwnerId()
            , GroupId()
            , FileSize()
            , LastModificationTime()
            , Checksum()
            , Type('0' /* Normal file */)
            , LinkedFileName()
            , UStarIndicator()
            , UStarVersion()
            , OwnerUserName()
            , OwnerGroupName()
            , DeviceMajorNumber()
            , DeviceMinorNumber()
            , FileNamePrefix()
            , Padding()
        {
            //
            // S_IFREG: regular file.
            //
            FileMode[0] = '0'; FileMode[1] = '1'; FileMode[2] = '0'; FileMode[3] = '0';

            //
            // S_IRWXU+S_IRWXG+S_IRWXO: User/Group/Other can Read/Write/Execute.
            //
            FileMode[4] = '7'; FileMode[5] = '7'; FileMode[6] = '7'; FileMode[7] = '\0';

            //
            // Ignore group and owner ids.
            //
            for (int32_t i = 0; i < 7; ++i)
            {
                OwnerId[i] = '0';
                GroupId[i] = '0';
            }

            //
            // Note that the Checksum field needs to be set to space characters before
            // we calculate the final checksum of the header.
            //
            for (int32_t i = 0; i < 7; ++i)
            {
                Checksum[i] = ' ';
            }

            ChecksumSpace = ' ';

            //
            // UStar magic number and version.
            //
            UStarIndicator[0] = 'u'; UStarIndicator[1] = 's'; UStarIndicator[2] = 't';
            UStarIndicator[3] = 'a'; UStarIndicator[4] = 'r'; UStarIndicator[5] = '\0';

            UStarVersion[0] = '0'; UStarVersion[1] = '0';
        }

        char FileName[100];                 // 0
        char FileMode[8];                   // 100
        char OwnerId[8];                    // 108
        char GroupId[8];                    // 116
        char FileSize[12];                  // 124
        char LastModificationTime[12];      // 136
        char Checksum[7];                   // 148
        char ChecksumSpace;                 // 155
        char Type;                          // 156
        char LinkedFileName[100];           // 157
        char UStarIndicator[6];             // 257
        char UStarVersion[2];               // 263
        char OwnerUserName[32];             // 265
        char OwnerGroupName[32];            // 297
        char DeviceMajorNumber[8];          // 329
        char DeviceMinorNumber[8];          // 337
        char FileNamePrefix[155];           // 345
        char Padding[12];                   // 500
                                            // 512
    };
#pragma pack (pop)

    template <size_t N>
    void CopyStringToTarHeader(const std::string& input, char output[N])
    {
        assert(input.size() < N);

        for (size_t i = 0; i < input.size(); ++i)
        {
            output[i] = input[i];
        }

        for (size_t i = input.size(); i < N; ++i)
        {
            output[i] = '\0';
        }
    }

    template <size_t N>
    void CopyUInt64ToTarHeaderAsOctets(const uint64_t input, char output[N])
    {
        size_t numberOfOctets = 0;

        if (input > 0)
        {
            char buffer[32] = {};

            numberOfOctets = sprintf_s(
                buffer,
                "%0*llo",
                static_cast<int>(N - 1),
                input);

            assert(numberOfOctets <= N - 1);

            for (size_t i = 0; i < numberOfOctets; ++i)
            {
                output[i] = buffer[i];
            }
        }

        for (size_t i = numberOfOctets; i < N; ++i)
        {
            output[i] = '\0';
        }
    }

    Tarball::Tarball(const std::wstring& tarballFileName) {
        m_tarballFile.open(tarballFileName, std::ios::binary);
        assert(m_tarballFile.is_open());
    }

    Tarball::~Tarball() {
        Close();
    }

    void Tarball::Close() {
        if (m_tarballFile.is_open()) {
            // The tarball always ends with two 512 byte blocks of zeros.
            std::vector<char> alignmentData(512, 0);
            m_tarballFile.write(alignmentData.data(), alignmentData.size());
            m_tarballFile.write(alignmentData.data(), alignmentData.size());
            m_tarballFile.close();
        }
    }

    void Tarball::AddFile(
            const std::wstring& fileName,
            const uint8_t* fileData,
            const size_t fileSize)
    {
        assert(m_tarballFile.is_open());

        static_assert(
            sizeof(TarHeader) == 512,
            "Size of the TarHeader structure must be equal to 512 bytes.");

        // Construct the file header.

        TarHeader header;

        CopyStringToTarHeader<100>(Utf16ToUtf8(fileName), header.FileName);
        CopyUInt64ToTarHeaderAsOctets<12>(fileSize, header.FileSize);
        CopyUInt64ToTarHeaderAsOctets<12>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count(),
            header.LastModificationTime);

        uint64_t headerChecksum = 0;
        for (size_t i = 0; i < sizeof(header); ++i)
        {
            headerChecksum += reinterpret_cast<uint8_t*>(&header)[i];
        }

        CopyUInt64ToTarHeaderAsOctets<7>(headerChecksum, header.Checksum);

        // Write the header and the data to the tarball.

        m_tarballFile.write(
            reinterpret_cast<const char*>(&header), sizeof(header));
        m_tarballFile.write(
            reinterpret_cast<const char*>(fileData), fileSize);

        // Make sure the file is aligned to 512 byes, otherwise
        // pad the file with zeros.

        const size_t lastBlockSize = fileSize % 512;
        if (lastBlockSize != 0)
        {
            const size_t lastBlockPadding = 512 - lastBlockSize;
            assert(lastBlockPadding < 512);

            m_tarballFile.write(
                std::vector<char>(lastBlockPadding, 0).data(), lastBlockPadding);
        }
    }
}
