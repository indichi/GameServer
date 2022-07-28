#pragma once

#include <Windows.h>

#define dfMAX_NAME_LEN      (64)
#define dfMAX_DATA_COUNT    (100)
#define dfMAX_THREAD_COUNT  (50)

struct st_PROFILE_DATA
{
    BOOL            _bIsUse;
    WCHAR           _szName[dfMAX_NAME_LEN];    // ���� �̸�

    LARGE_INTEGER   _lStartTime;            // �������Ϸ� ���۽ð�

    UINT64          _iTotalTime;            // ��ü ��� �ð�
    UINT64          _iMin[2];               // �ּ� ���ð� 2��
    UINT64          _iMax[2];               // �ִ� ���ð� 2��

    UINT            _iCall;                 // ���� ȣ�� Ƚ��
};

struct st_THREAD_DATA
{
    DWORD               _dwThreadID;
    //st_PROFILE_DATA     _stProfileData[dfMAX_THREAD_COUNT];
    st_PROFILE_DATA*    _stProfileData;
};

void InitProfiler();

bool ProfilingBegin(const WCHAR* szName);
bool ProfilingEnd(const WCHAR* szName);
bool GetProfileData(const WCHAR* szName, st_PROFILE_DATA** pOut);

bool ProfilePrint();

#define PROFILE_BEGIN(x)    ProfilingBegin(x)
#define PROFILE_END(x)      ProfilingEnd(x)