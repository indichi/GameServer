#include <iostream>
#include "Profiler.h"

DWORD dwTLSIndex;
st_THREAD_DATA ProfileArray[dfMAX_THREAD_COUNT];
long lEmptyIndex = -1;
LARGE_INTEGER lFreq;

void InitProfiler()
{
    QueryPerformanceFrequency(&lFreq);

    dwTLSIndex = TlsAlloc();
    if (dwTLSIndex == TLS_OUT_OF_INDEXES)
        exit(-1);

    for (int i = 0; i < dfMAX_THREAD_COUNT; ++i)
    {
        ProfileArray[i]._dwThreadID = -1;
        ProfileArray[i]._stProfileData = nullptr;
    }
}

bool ProfilingBegin(const WCHAR* szName)
{
    st_PROFILE_DATA* pData = nullptr;
    GetProfileData(szName, &pData);

    if (pData == nullptr)
        return false;

    // 중복 호출 됐다면 그냥 return..
    if (pData->_lStartTime.QuadPart != 0)
        return false;

    QueryPerformanceCounter(&pData->_lStartTime);

    return true;
}

bool ProfilingEnd(const WCHAR* szName)
{
    LARGE_INTEGER lEndTime;
    UINT64 iProfilingTime;
    st_PROFILE_DATA* pData = nullptr;

    GetProfileData(szName, &pData);
    if (pData == nullptr)
        return false;

    QueryPerformanceCounter(&lEndTime);
    iProfilingTime = (lEndTime.QuadPart - pData->_lStartTime.QuadPart);

    if (pData->_iMax[0] == 0)
        pData->_iMax[0] = iProfilingTime;
    else if (pData->_iMax[1] == 0)
        pData->_iMax[1] = iProfilingTime;
    else
    {
        if (pData->_iMax[0] < iProfilingTime)
            pData->_iMax[0] = iProfilingTime;
        else if (pData->_iMax[1] < iProfilingTime)
            pData->_iMax[1] = iProfilingTime;
    }

    if (pData->_iMin[0] == _UI64_MAX)
        pData->_iMin[0] = iProfilingTime;
    else if (pData->_iMin[1] == _UI64_MAX)
        pData->_iMin[1] = iProfilingTime;
    else
    {
        if (pData->_iMin[0] > iProfilingTime)
            pData->_iMin[0] = iProfilingTime;
        else if (pData->_iMin[1] > iProfilingTime)
            pData->_iMin[1] = iProfilingTime;
    }
    
    pData->_iTotalTime += iProfilingTime;
    ++pData->_iCall;

    pData->_lStartTime.QuadPart = 0;

    return true;
}

bool GetProfileData(const WCHAR* szName, st_PROFILE_DATA** pOut)
{
    st_PROFILE_DATA* pData = (st_PROFILE_DATA*)TlsGetValue(dwTLSIndex);

    // 처음 사용하는 거라면..
    if (pData == nullptr)
    {
        pData = new st_PROFILE_DATA[dfMAX_DATA_COUNT];

        memset(pData, 0, sizeof(st_PROFILE_DATA) * dfMAX_DATA_COUNT);

        if (!TlsSetValue(dwTLSIndex, pData))
            return false;

        // thread sample array 설정
        long lindex = InterlockedIncrement(&lEmptyIndex);

        if (lindex >= dfMAX_THREAD_COUNT)
            return false;

        ProfileArray[lindex]._dwThreadID = GetCurrentThreadId();
        ProfileArray[lindex]._stProfileData = pData;

        *pOut = pData;
    }
    // 세팅 된 값이 있다면
    else
    {
        for (int i = 0; i < dfMAX_DATA_COUNT; ++i)
        {
            // 해당 tag이름으로 Data가 이미 존재
            if (pData[i]._bIsUse == TRUE && (wcscmp(pData[i]._szName, szName) == 0))
            {
                *pOut = pData;
                break;
            }

            // 사용중이지도 않고 tag도 ""일 때 (비어있는 Data)
            if (pData[i]._bIsUse == FALSE && (wcscmp(pData[i]._szName, L"") == 0))
            {
                pData[i]._bIsUse = TRUE;
                pData[i]._iCall = 0;
                pData[i]._iMax[0] = 0;
                pData[i]._iMax[1] = 0;
                pData[i]._iMin[0] = _UI64_MAX;
                pData[i]._iMin[1] = _UI64_MAX;
                pData[i]._iTotalTime = 0;
                wcscpy_s(pData[i]._szName, szName);

                *pOut = pData;
                break;
            }
        }
    }

    return true;
}

bool ProfilePrint()
{
    SYSTEMTIME stTime;
    WCHAR szFileName[256];
    FILE* pStream;

    GetLocalTime(&stTime);
    wsprintf(szFileName, L"%4d-%02d-%02d %02d.%02d.%02d.txt", stTime.wYear, stTime.wMonth, stTime.wDay, stTime.wHour, stTime.wMinute, stTime.wSecond);

    _wfopen_s(&pStream, szFileName, L"w+");

    if (pStream == 0)
        return false;

    for (int i = 0; i < dfMAX_THREAD_COUNT; ++i)
    {
        if (ProfileArray[i]._dwThreadID == -1 || ProfileArray[i]._stProfileData == nullptr)
            continue;

        fwprintf_s(pStream, L"////////////////////////////////////\n");
        fwprintf_s(pStream, L"Thread ID:%d\n", ProfileArray[i]._dwThreadID);

        fwprintf_s(pStream, L"--------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
        fwprintf_s(pStream, L"%64s  |  %20s  |  %20s  |  %20s  |  %10s  |\n", L"Name", L"Average", L"Min", L"Max", L"Call");
        for (int j = 0; j < dfMAX_DATA_COUNT; ++j)
        {
            st_PROFILE_DATA* pData = &(ProfileArray[i]._stProfileData[j]);

            if (pData->_bIsUse == FALSE || (wcscmp(pData->_szName, L"") == 0))
                continue;

            //TODO print 마무리..
            double dAvg;
            double dMin = pData->_iMin[0] > pData->_iMin[1] ? pData->_iMin[1] : pData->_iMin[0];
            double dMax = pData->_iMax[0] < pData->_iMax[1] ? pData->_iMax[1] : pData->_iMax[0];

            UINT64 iTotal = pData->_iTotalTime;
            int iCall = pData->_iCall;
            int iDivCall = iCall;

            if (iCall > 4)
            {
                iTotal -= pData->_iMin[0];
                iTotal -= pData->_iMin[1];

                iTotal -= pData->_iMax[0];
                iTotal -= pData->_iMax[1];
                
                iDivCall -= 4;
            }

            dAvg = iTotal / lFreq.QuadPart * 1000000.0 / iDivCall;
            dMin = dMin / lFreq.QuadPart * 1000000.0;
            dMax = dMax / lFreq.QuadPart * 1000000.0;

            fwprintf_s(pStream, L"%64s  |  %20.8f  |  %20.4f  |  %20.4f  |  %10d  |\n", pData->_szName, dAvg, dMin, dMax, iDivCall);
        }

        fwprintf_s(pStream, L"--------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    }

    fclose(pStream);

    return true;
}
