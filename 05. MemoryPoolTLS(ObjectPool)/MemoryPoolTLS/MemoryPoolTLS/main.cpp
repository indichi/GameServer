#include <thread>
#include <Windows.h>

#include "CMemoryPoolTLS.h"
#include "Profiler.h"
//#include "TLSProfiling.h"

class CTest
{
public:
    CTest()
        : a(0)
        , b(0)
        , c(0)
        , d(0)
    {}
    ~CTest() {}
private:
    int     a;
    int     b;
    int     c;
    int     d;
};

CMemoryPoolTLS<CTest> g_Pool;

void __stdcall NewDelete(LPVOID lpEvent)
{
    HANDLE hEvent = (HANDLE)lpEvent;

    WaitForSingleObject(lpEvent, INFINITE);

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

void __stdcall PoolAllocFree(LPVOID lpEvent)
{
    HANDLE hEvent = (HANDLE)lpEvent;

    WaitForSingleObject(lpEvent, INFINITE);

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

int main(void)
{
    const int iThreadCount = 4;
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (hEvent == 0)
        return -1;

    HANDLE hThreads[iThreadCount];

    InitProfiler();

    ////////////////////////////////////////////////////////////
    //// new delete
    ////////////////////////////////////////////////////////////

    hThreads[0] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)NewDelete, &hEvent, NULL, NULL);
    hThreads[1] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)NewDelete, &hEvent, NULL, NULL);
    hThreads[2] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)NewDelete, &hEvent, NULL, NULL);
    hThreads[3] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)NewDelete, &hEvent, NULL, NULL);
    /*hThreads[4] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)NewDelete, &hEvent, NULL, NULL);
    hThreads[5] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)NewDelete, &hEvent, NULL, NULL);*/

    SetEvent(hEvent);

    WaitForMultipleObjects(iThreadCount, hThreads, TRUE, INFINITE);

    ////////////////////////////////////////////////////////////
    //// Alloc Free
    ////////////////////////////////////////////////////////////

    ResetEvent(hEvent);

    hThreads[0] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)PoolAllocFree, &hEvent, NULL, NULL);
    hThreads[1] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)PoolAllocFree, &hEvent, NULL, NULL);
    hThreads[2] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)PoolAllocFree, &hEvent, NULL, NULL);
    hThreads[3] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)PoolAllocFree, &hEvent, NULL, NULL);
    /*hThreads[4] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)PoolAllocFree, &hEvent, NULL, NULL);
    hThreads[5] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)PoolAllocFree, &hEvent, NULL, NULL);*/

    SetEvent(hEvent);

    WaitForMultipleObjects(iThreadCount, hThreads, TRUE, INFINITE);

    CloseHandle(hEvent);

    ProfilePrint();

    return 0;
}