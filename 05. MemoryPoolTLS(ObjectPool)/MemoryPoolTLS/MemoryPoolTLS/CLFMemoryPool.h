#ifndef __PROCADEMY_LFMEMORY_POOL__
#define __PROCADEMY_LFMEMORY_POOL__

#include <iostream>
#include <Windows.h>
#include <tchar.h>

#include "CCrashDump.h"

#define dfSTATE_ALLOC     (0xffffffffffffffff)
#define dfSTATE_RELEASE   (0xdddddddddddddddd)
#define dfMEMORY_LOG_CNT  (10000)

namespace procademy
{
    template <typename T>
    class CLFMemoryPool
    {
    public:
        CLFMemoryPool(bool usePlacement = false);
        virtual ~CLFMemoryPool();

        T* Alloc();
        bool Free(volatile T* data);

        int GetCapacity() { return m_lCapacity; }
        int GetUseCount() { return m_lUseCount; }
    private:
        void WriteLog(const WCHAR* log) const;

        struct st_NODE
        {
            UINT64                  _uiID;
            st_NODE*                _pNext;
#ifdef __DEBUG
            UINT64                  _uiFreeFlag;
            CLFMemoryPool*          _pCheckStart;
#endif
            T                       _tData;
#ifdef __DEBUG
            CLFMemoryPool*          _pCheckEnd;
#endif
        };

        struct st_CMP_NODE
        {
            st_NODE*                _pNode;
            UINT64                  _uiID;
        };

#ifdef __DEBUG
        enum class e_TYPE
        {
            TYPE_PUSH_TRY = 0,
            TYPE_PUSH_DONE,
            TYPE_POP_TRY,
            TYPE_POP_DONE
        };


        struct st_DEBUG
        {
            DWORD           _dwThreadID;
            e_TYPE          _eType;
        };

        st_DEBUG* m_pMemoryLog;

        UINT64                  m_lDebugIndex;
#endif
        alignas(16) st_CMP_NODE             m_pTop;
        // UINT64                  m_uiID;

        alignas(64) long                    m_lCapacity;
        alignas(64) long                    m_lUseCount;

        alignas(64) bool                    m_bUsePlacement;
        CCrashDump*                         g_Dump;
    };

    template <typename T>
    CLFMemoryPool<T>::CLFMemoryPool(bool bUsePlacement)
        : m_pTop{ nullptr, 0 }
        //, m_uiID(0)
#ifdef __DEBUG
        , m_pMemoryLog(new st_DEBUG[dfMEMORY_LOG_CNT])
        , m_lDebugIndex(-1)
#endif
        , m_lCapacity(0)
        , m_lUseCount(0)
        , m_bUsePlacement(bUsePlacement)
        , g_Dump(new CCrashDump)
    {
    }

    template <typename T>
    CLFMemoryPool<T>::~CLFMemoryPool()
    {
        // 현재까지 free인 데이터들만 해제하고 나머지는 버림
        while (m_pTop._pNode != nullptr)
        {
            st_NODE* pTemp = m_pTop._pNode;
            m_pTop._pNode = m_pTop._pNode->_pNext;
            delete pTemp;
        }

        delete g_Dump;
    }

    template <typename T>
    T* CLFMemoryPool<T>::Alloc()
    {
        // 메모리 풀 입장에서 Alloc은 pop과 동일
        st_CMP_NODE stCmpNode;
        st_NODE* pCopyNode;
        T* pData;

#ifdef __DEBUG
        // 메모리 로깅 관련 인덱스, 카운트 설정
        ULONG64 iIndex;

        iIndex = InterlockedIncrement(&m_lDebugIndex);
        iIndex %= dfMEMORY_LOG_CNT;
        st_DEBUG* stDebug = &m_pMemoryLog[iIndex];
#endif

        UINT64 uiID = 0;

        do
        {
            stCmpNode._pNode = m_pTop._pNode;
            stCmpNode._uiID = m_pTop._uiID;

            pCopyNode = stCmpNode._pNode;

            if (pCopyNode == nullptr)
            {
                pCopyNode = new st_NODE;

                pCopyNode->_uiID = 0;
                pCopyNode->_pNext = nullptr;
#ifdef __DEBUG
                pCopyNode->_uiFreeFlag = dfSTATE_ALLOC;
                pCopyNode->_pCheckStart = this;
                pCopyNode->_pCheckEnd = this;

                //InterlockedIncrement(&m_lCapacity);

                // 메모리 로깅..
                stDebug->_dwThreadID = GetCurrentThreadId();
                stDebug->_eType = e_TYPE::TYPE_POP_TRY;
#endif
                break;
            }

#ifdef __DEBUG
            // 메모리 로깅..
            stDebug->_dwThreadID = GetCurrentThreadId();
            stDebug->_eType = e_TYPE::TYPE_POP_TRY;

            pCopyNode->_uiFreeFlag = dfSTATE_ALLOC;
#endif

            if (pCopyNode->_pNext != nullptr)
                uiID = pCopyNode->_pNext->_uiID;

        } while (!InterlockedCompareExchange128((LONG64*)&m_pTop, uiID, (LONG64)pCopyNode->_pNext, (LONG64*)&stCmpNode));

        //InterlockedExchange(&pCopyNode->_uiFreeFlag, dfSTATE_ALLOC);
        

        pData = &(pCopyNode->_tData);
        if (m_bUsePlacement)
            new(pData) T;

        //InterlockedIncrement(&m_lUseCount);

#ifdef __DEBUG
        // 메모리 로깅..
        iIndex = InterlockedIncrement(&m_lDebugIndex);
        iIndex %= dfMEMORY_LOG_CNT;
        stDebug = &m_pMemoryLog[iIndex];

        stDebug->_dwThreadID = GetCurrentThreadId();
        stDebug->_eType = e_TYPE::TYPE_POP_DONE;
#endif

        return pData;
    }

    template <typename T>
    bool CLFMemoryPool<T>::Free(volatile T* data)
    {
        // 메모리 풀 입장에서 Free는 push과 동일
        st_CMP_NODE stCmpNode;
#ifdef __DEBUG
        st_NODE* pPushNode = (st_NODE*)(((char*)data) - sizeof(CLFMemoryPool*) - sizeof(UINT64) - sizeof(UINT64) - sizeof(st_NODE*));
#else
        //st_NODE* pPushNode = (st_NODE*)(((char*)data) - sizeof(UINT64) - sizeof(st_NODE*));
        st_NODE* pPushNode = (st_NODE*)((char*)data - (sizeof(st_NODE) - sizeof(T)));
#endif

#ifdef __DEBUG
        // 메모리 로깅 관련 인덱스, 카운트 설정
        ULONG64 iIndex;

        iIndex = InterlockedIncrement(&m_lDebugIndex);
        iIndex %= dfMEMORY_LOG_CNT;
        st_DEBUG* stDebug = &m_pMemoryLog[iIndex];


        // this로 체크섬 확인 (이 풀에서 나간 오브젝트가 맞는지)
        // (+ 언더 플로우 체크)
        if (pPushNode->_pCheckStart != this)
        {
            WCHAR buf[128];
            wsprintf(buf, L"Free(T* data), not this pool member or check underflow");
            WriteLog(buf);

            g_Dump->Crash();
        }

        // (+ 오버 플로우 체크)
        if (pPushNode->_pCheckEnd != this)
        {
            WCHAR buf[128];
            wsprintf(buf, L"Free(T* data), not this pool member or check overflow");
            WriteLog(buf);

            g_Dump->Crash();
        }

        // free flag 확인 (이미 free 된 녀석은 아닌지..)
        // 이미 반납된 녀석이라면..
        if (pPushNode->_uiFreeFlag == dfSTATE_RELEASE)
        {
            WCHAR buf[128];

            wsprintf(buf, L"Free(T* data), this parameter is already free");
            WriteLog(buf);

            // 일단 로그는 썼고 free한 데이터 또 free 하려고 하면 크래쉬 내줄지 정해야함....
            // crash();

            g_Dump->Crash();
        }
        else
        {
            if (pPushNode->_uiFreeFlag != dfSTATE_ALLOC)
            {
                WCHAR buf[128];

                wsprintf(buf, L"Free(T* data), not this pool member or check underflow");
                WriteLog(buf);
                g_Dump->Crash();
            }
        }

        pPushNode->_uiFreeFlag = dfSTATE_RELEASE;
#endif
        pPushNode->_uiID++;
        
        do
        {
            stCmpNode._pNode = m_pTop._pNode;
            stCmpNode._uiID = m_pTop._uiID;

            pPushNode->_pNext = stCmpNode._pNode;

#ifdef __DEBUG
            // 메모리 로깅..
            stDebug->_dwThreadID = GetCurrentThreadId();
            stDebug->_eType = e_TYPE::TYPE_PUSH_TRY;
#endif

        } while (!InterlockedCompareExchange128((LONG64*)&m_pTop, (LONG64)pPushNode->_uiID, (LONG64)pPushNode, (LONG64*)&stCmpNode));

        if (m_bUsePlacement)
            pPushNode->_tData.~T();

        //InterlockedDecrement(&m_lUseCount);

#ifdef __DEBUG
        // 메모리 로깅..
        iIndex = InterlockedIncrement(&m_lDebugIndex);
        iIndex %= dfMEMORY_LOG_CNT;
        stDebug = &m_pMemoryLog[iIndex];

        stDebug->_dwThreadID = GetCurrentThreadId();
        stDebug->_eType = e_TYPE::TYPE_PUSH_DONE;
#endif

        return true;
    }

    template <typename T>
    void CLFMemoryPool<T>::WriteLog(const WCHAR* log) const
    {
        FILE* stream;
        _wfopen_s(&stream, L"MemoryPool Log.txt", L"a");

        if (stream == 0)
        {
            return;
        }

        fwprintf_s(stream, L"%s | %s\n", log, _T(__TIME__));

        fclose(stream);
    }
}

#endif