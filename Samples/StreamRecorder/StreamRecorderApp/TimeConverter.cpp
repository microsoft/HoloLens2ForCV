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

#include "TimeConverter.h"

static constexpr UINT64 kMaxLongLong = static_cast<UINT64>(std::numeric_limits<long long>::max());

long long checkAndConvertUnsigned(UINT64 val)
{
    assert(val <= kMaxLongLong);
    return static_cast<long long>(val);
}

HundredsOfNanoseconds UniversalToUnixTime(const FILETIME fileTime)
{
    const std::chrono::seconds c_unix_epoch(11'644'473'600);

    return HundredsOfNanoseconds(
        fileTime.dwLowDateTime + (static_cast<uint64_t>(fileTime.dwHighDateTime) << 32)) - c_unix_epoch;
}