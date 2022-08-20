#include <thread>
#include <Windows.h>

#include "CMemoryPoolTLS.h"
#include "Profiler.h"
//#include "TLSProfiling.h"

HANDLE hNewDeleteEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
HANDLE hPoolEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
const int iThreadCount = 4;

class CTest
{
public:
    CTest() {}
    ~CTest() {}
private:
    int     a[350];
};

CMemoryPoolTLS<CTest> g_Pool;

void __stdcall NewDelete()
{
    WaitForSingleObject(hNewDeleteEvent, INFINITE);

    for (int i = 0; i < 10000; ++i)
    {
        PROFILE_BEGIN(L"new/delete");
        for (int j = 0; j < 10000; j++)
        {
            CTest* t = new CTest;
            delete t;
        }
        PROFILE_END(L"new/delete");
    }

}

void __stdcall PoolAllocFree()
{
    WaitForSingleObject(hPoolEvent, INFINITE);

    for (int i = 0; i < 10000; ++i)
    {
        PROFILE_BEGIN(L"Alloc/Free");
        for (int j = 0; j < 10000; j++)
        {
            CTest* t = g_Pool.Alloc();
            g_Pool.Free(t);
        }
        PROFILE_END(L"Alloc/Free");
    }
}

void SetPool()
{
    CTest** arrPool = new CTest * [10000];

    for (int i = 0; i < 10000; ++i)
    {
        arrPool[i] = g_Pool.Alloc();
    }

    for (int i = 0; i < 10000; ++i)
    {
        g_Pool.Free(arrPool[i]);
    }

    delete[] arrPool;
}

int main(void)
{
    if (hNewDeleteEvent == 0 || hPoolEvent == 0)
        return -1;

    HANDLE hThreads[iThreadCount];

    InitProfiler();

    ////////////////////////////////////////////////////////////
    //// new delete
    ////////////////////////////////////////////////////////////

    hThreads[0] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)NewDelete, NULL, NULL, NULL);
    hThreads[1] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)NewDelete, NULL, NULL, NULL);
    hThreads[2] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)NewDelete, NULL, NULL, NULL);
    hThreads[3] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)NewDelete, NULL, NULL, NULL);

    SetEvent(hNewDeleteEvent);

    WaitForMultipleObjects(iThreadCount, hThreads, TRUE, INFINITE);
    
    for (int i = 0; i < iThreadCount; ++i)
    {
        if (hThreads[i] == 0)
            continue;

        CloseHandle(hThreads[i]);
    }
    
    ////////////////////////////////////////////////////////////
    //// Alloc Free
    ////////////////////////////////////////////////////////////

    hThreads[0] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)PoolAllocFree, NULL, NULL, NULL);
    hThreads[1] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)PoolAllocFree, NULL, NULL, NULL);
    hThreads[2] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)PoolAllocFree, NULL, NULL, NULL);
    hThreads[3] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)PoolAllocFree, NULL, NULL, NULL);

    SetPool();

    SetEvent(hPoolEvent);

    WaitForMultipleObjects(iThreadCount, hThreads, TRUE, INFINITE);

    ProfilePrint();

    CloseHandle(hNewDeleteEvent);
    CloseHandle(hPoolEvent);

    for (int i = 0; i < iThreadCount; ++i)
    {
        if (hThreads[i] == 0)
            continue;

        CloseHandle(hThreads[i]);
    }

    return 0;
}