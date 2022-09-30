#pragma once

#include "CLFMemoryPool.h"
#include "CCrashDump.h"

using namespace procademy;

//#define __LFSTACK_DEBUG

template <typename T>
class CLFStack
{
private:

    struct st_NODE
    {
        T               _tData;
        st_NODE*        _pNext;
        UINT64          _uiID;
    };

    struct st_CMP_NODE
    {
        st_NODE*    _pNode;
        UINT64      _uiID;
    };

    CCrashDump      g_Dump;

#ifdef __LFSTACK_DEBUG

    enum class e_TYPE
    {
        TYPE_PUSH_TRY = 0,
        TYPE_PUSH_DONE,
        TYPE_POP_TRY,
        TYPE_POP_DONE
    };

    struct st_DEBUG
    {
        DWORD           _dwThreadID;            // 작업 Thread ID
        e_TYPE          _eType;                 // 작업 Type

        st_NODE*        _pOriginTop;            // 원본 Top Node
        st_NODE*        _pOriginNext;           // 원본 TopNode->Next
        UINT64          _uiOriginID;            // 원본 Unique ID

        st_NODE*        _pCopyTop;              // 사본 TopNode
        st_NODE*        _pCopyNext;             // 사본 TopNode->Next
        UINT64          _uiCopyID;              // 사본 Unique ID

        st_NODE*        _pChangeNode;           // 교체 Node
        UINT64          _uiChangeID;            // 교체 Unique ID
    };

#endif

private:

    CLFMemoryPool<st_NODE>                  m_Pool;
    alignas(16) st_CMP_NODE                 m_pTop;
    alignas(64) ULONG64                     m_ulSize;
    alignas(64) DWORD64                     m_dwCheckID;

public:
    CLFStack()
        : m_Pool()
        , m_pTop{ nullptr, 0 }
        , m_ulSize(0)
        , m_dwCheckID(0)
#ifdef __LFSTACK_DEBUG
        , m_pMemoryLog(new st_DEBUG[dfMEMORY_LOG_CNT])
        , m_lDebugIndex(-1)
#endif
    {}

    ~CLFStack() {}

    ULONG64 GetSize() const { return m_ulSize; }

    void Push(T data)
    {
#ifdef __LFSTACK_DEBUG
        // 메모리 로깅 관련 인덱스, 카운트 설정
        ULONG64 iIndex;

        iIndex = InterlockedIncrement(&m_lDebugIndex);
        iIndex %= dfMEMORY_LOG_CNT;
        st_DEBUG* stDebug = &m_pMemoryLog[iIndex];
#endif

        //
        st_CMP_NODE stCmpNode;

        st_NODE* pNewNode = m_Pool.Alloc();
        pNewNode->_tData = data;
        pNewNode->_pNext = nullptr;

        DWORD64 dwID = InterlockedIncrement(&m_dwCheckID);      // 고유 ID 값 증가

        do
        {
            // 현재 Top 복사
            stCmpNode._pNode = m_pTop._pNode;
            stCmpNode._uiID = m_pTop._uiID;

            // 새로운 노드의 next 설정
            pNewNode->_pNext = m_pTop._pNode;

#ifdef __LFSTACK_DEBUG
            // 메모리 로깅..
            stDebug->_dwThreadID = GetCurrentThreadId();
            stDebug->_eType = e_TYPE::TYPE_PUSH_TRY;

            stDebug->_pOriginTop = m_pTop._pNode;
            stDebug->_pOriginNext = stDebug->_pOriginTop != nullptr ? stDebug->_pOriginTop->_pNext : nullptr;

            stDebug->_pCopyTop = stCmpNode._pNode;
            stDebug->_pCopyNext = stDebug->_pCopyTop != nullptr ? stDebug->_pCopyTop->_pNext : nullptr;

            stDebug->_pChangeNode = pNewNode;
#endif
        } while (InterlockedCompareExchange128((LONG64*)&m_pTop, dwID, (LONG64)pNewNode, (LONG64*)&stCmpNode) != TRUE);     // Top의 주소 값 + 고유 ID 값 같이 비교/변경


        InterlockedIncrement(&m_ulSize);
    }

    bool Pop(volatile T* pOut)
    {
#ifdef __LFSTACK_DEBUG
        // 메모리 로깅 관련 인덱스, 카운트 설정
        ULONG64 iIndex;

        iIndex = InterlockedIncrement(&m_lDebugIndex);
        iIndex %= dfMEMORY_LOG_CNT;
        st_DEBUG* stDebug = &m_pMemoryLog[iIndex];
#endif

        // 
        st_CMP_NODE stCmpNode;

        DWORD64 dwID = InterlockedIncrement(&m_dwCheckID);      // 고유 ID 값 증가

        do
        {
            // 현재 Top 복사
            stCmpNode._pNode = m_pTop._pNode;
            stCmpNode._uiID = m_pTop._uiID;

            if (stCmpNode._pNode == nullptr)
            {
                //g_Dump.Crash();
                return false;
            }

#ifdef __LFSTACK_DEBUG
            // 메모리 로깅..
            stDebug->_dwThreadID = GetCurrentThreadId();
            stDebug->_eType = e_TYPE::TYPE_POP_TRY;

            stDebug->_pOriginTop = m_pTop._pNode;
            stDebug->_pOriginNext = stDebug->_pOriginTop != nullptr ? stDebug->_pOriginTop->_pNext : nullptr;

            stDebug->_pCopyTop = stCmpNode._pNode;
            stDebug->_pCopyNext = stDebug->_pCopyTop != nullptr ? stDebug->_pCopyTop->_pNext : nullptr;

            stDebug->_pChangeNode = stCmpNode._pNode->_pNext;
#endif
        } while (!InterlockedCompareExchange128((LONG64*)&m_pTop, dwID, (LONG64)stCmpNode._pNode->_pNext, (LONG64*)&stCmpNode));        // Top의 주소 값 + 고유 ID 값 같이 비교/변경

        *pOut = stCmpNode._pNode->_tData;       // Out Parameter에 데이터 복사
        m_Pool.Free(stCmpNode._pNode);          // 메모리 풀에 반환

        InterlockedDecrement(&m_ulSize);

        return true;
    }



#ifdef __LFSTACK_DEBUG
    st_DEBUG*                   m_pMemoryLog;
    ULONG64                     m_lDebugIndex;
#endif
};
