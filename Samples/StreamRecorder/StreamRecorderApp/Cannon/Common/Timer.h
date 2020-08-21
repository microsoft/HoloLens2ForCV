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

#ifndef TIMER_
#define TIMER_

#include <wrl.h>
#include <iostream>
#include <cassert>

class Timer
{
public:
	Timer()
	{
		QueryPerformanceFrequency(&m_freq);
		QueryPerformanceCounter(&m_start);
	}

	~Timer()
	{
	}

	void Reset()
	{	
		QueryPerformanceCounter(&m_start);
	}

	float GetTime()
	{
		QueryPerformanceCounter(&m_stop);
		return (m_stop.QuadPart - m_start.QuadPart) / (float) m_freq.QuadPart;
	}

	void PrintTime(char* szText)
	{
		float fTime = GetTime();
		std::cout << szText << fTime << std::endl;
	}

    void PrintTimeOutputString(char* szText)
    {
        char outputString[1024];
        sprintf_s(outputString, 1024, "%s %f\n", szText, GetTime());
        OutputDebugStringA(outputString);
    }

	// Returns the total time from QueryPerformanceCounter in 100's of nanoseconds
	static unsigned long long GetSystemRelativeTime()
	{
		assert(sizeof(unsigned long long) == sizeof(LARGE_INTEGER::QuadPart));

		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);

		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);

		return (time.QuadPart * 10000000 / freq.QuadPart);
	}

	static double GetSystemRelativeTimeInSeconds()
	{
		assert(sizeof(long long) == sizeof(LARGE_INTEGER::QuadPart));

		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);

		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);

		return time.QuadPart / (double) freq.QuadPart;
	}

	static unsigned long long GetFileTime()
	{
		FILETIME fileTimeStruct;
		GetSystemTimePreciseAsFileTime(&fileTimeStruct);

		ULARGE_INTEGER fileTime;
		fileTime.HighPart = fileTimeStruct.dwHighDateTime;
		fileTime.LowPart = fileTimeStruct.dwLowDateTime;
		return fileTime.QuadPart;
	}

	static unsigned long long ConvertFileTimeToQPCTime(unsigned long long fileTime)
	{
		unsigned long long offset = GetFileTime() - GetSystemRelativeTime();
		return fileTime - offset;
	}

private:

	LARGE_INTEGER m_start;
	LARGE_INTEGER m_stop;
	LARGE_INTEGER m_freq;
};


#endif