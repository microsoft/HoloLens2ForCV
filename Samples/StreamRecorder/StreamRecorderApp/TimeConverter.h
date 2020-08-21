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

#include <cassert>
#include <chrono>
#include <cstdint>
#include <wrl.h>

typedef std::chrono::duration<int64_t, std::ratio<1, 10'000'000>> HundredsOfNanoseconds;

HundredsOfNanoseconds UniversalToUnixTime(const FILETIME fileTime);
long long checkAndConvertUnsigned(UINT64 val);

class TimeConverter
{
public:
	TimeConverter()
	{
		QueryPerformanceFrequency(&m_qpf);
		m_qpc2ft = CalculateRelativeToAbsoluteTicksOffset();
	}

	~TimeConverter()
	{
	}

	HundredsOfNanoseconds TimeConverter::RelativeTicksToAbsoluteTicks(const HundredsOfNanoseconds ticks) const
	{
		return m_qpc2ft + ticks;
	}

private:

	HundredsOfNanoseconds TimeConverter::UnsignedQpcToRelativeTicks(const uint64_t qpc) const
	{
		static const std::uint64_t c_ticksPerSecond = 10'000'000;

		const std::uint64_t q = qpc / m_qpf.QuadPart;
		const std::uint64_t r = qpc % m_qpf.QuadPart;

		return HundredsOfNanoseconds(
			q * c_ticksPerSecond + (r * c_ticksPerSecond) / m_qpf.QuadPart);
	}

	HundredsOfNanoseconds TimeConverter::QpcToRelativeTicks(const int64_t qpc) const
	{
		if (qpc < 0)
		{
			return -UnsignedQpcToRelativeTicks(
				static_cast<uint64_t>(-qpc));
		}
		else
		{
			return UnsignedQpcToRelativeTicks(
				static_cast<uint64_t>(qpc));
		}
	}

	HundredsOfNanoseconds QpcToRelativeTicks(const LARGE_INTEGER qpc) const
	{
		return QpcToRelativeTicks(qpc.QuadPart);
	}

	HundredsOfNanoseconds TimeConverter::FileTimeToAbsoluteTicks(const FILETIME ft) const
	{
		ULARGE_INTEGER ft_uli;

		ft_uli.HighPart = ft.dwHighDateTime;
		ft_uli.LowPart = ft.dwLowDateTime;

		return HundredsOfNanoseconds(ft_uli.QuadPart);
	}

	HundredsOfNanoseconds CalculateRelativeToAbsoluteTicksOffset() const
	{
		LARGE_INTEGER qpc_now;
		FILETIME ft_now;

		assert(QueryPerformanceCounter(&qpc_now));
		GetSystemTimePreciseAsFileTime(&ft_now);

		const HundredsOfNanoseconds qpc_now_in_ticks = QpcToRelativeTicks(qpc_now);
		const HundredsOfNanoseconds ft_now_in_ticks = FileTimeToAbsoluteTicks(ft_now);

		assert(ft_now_in_ticks > qpc_now_in_ticks);

		return ft_now_in_ticks - qpc_now_in_ticks;
	}

	LARGE_INTEGER m_qpf;
	HundredsOfNanoseconds m_qpc2ft;
};