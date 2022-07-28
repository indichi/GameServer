#pragma once

#include "CLFMemoryPool.h"
#include "CCrashDump.h"

using namespace procademy;

//#define __DEBUG

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

        st_NODE*        _pOriginTop;
        st_NODE*        _pOriginNext;
        UINT64          _uiOriginID;

        st_NODE*        _pCopyTop;
        st_NODE*        _pCopyNext;
        UINT64          _uiCopyID;

        st_NODE*        _pChangeNode;
        UINT64          _uiChangeID;
    };
#endif
public:
    CLFStack()
        : m_Pool()
        , m_pTop{ nullptr, 0 }
        , m_ulSize(0)
#ifdef __DEBUG
        , m_pMemoryLog(new st_DEBUG[dfMEMORY_LOG_CNT])
        , m_lDebugIndex(-1)
#endif
    {}

    ~CLFStack() {}

    ULONG64 GetSize() const { return m_ulSize; }

    void Push(T data)
    {
#ifdef __DEBUG
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

        do
        {
            stCmpNode._pNode = m_pTop._pNode;
            stCmpNode._uiID = m_pTop._uiID;

            pNewNode->_pNext = m_pTop._pNode;

#ifdef __DEBUG
            // 메모리 로깅..
            stDebug->_dwThreadID = GetCurrentThreadId();
            stDebug->_eType = e_TYPE::TYPE_PUSH_TRY;

            stDebug->_pOriginTop = m_pTop._pNode;
            stDebug->_pOriginNext = stDebug->_pOriginTop != nullptr ? stDebug->_pOriginTop->_pNext : nullptr;

            stDebug->_pCopyTop = stCmpNode._pNode;
            stDebug->_pCopyNext = stDebug->_pCopyTop != nullptr ? stDebug->_pCopyTop->_pNext : nullptr;

            stDebug->_pChangeNode = pNewNode;
#endif
        } while (InterlockedCompareExchange128((LONG64*)&m_pTop, m_pTop._uiID + 1, (LONG64)pNewNode, (LONG64*)&stCmpNode) != TRUE);


        InterlockedIncrement(&m_ulSize);
    }

    void Pop(volatile T* pOut)
    {
#ifdef __DEBUG
        // 메모리 로깅 관련 인덱스, 카운트 설정
        ULONG64 iIndex;

        iIndex = InterlockedIncrement(&m_lDebugIndex);
        iIndex %= dfMEMORY_LOG_CNT;
        st_DEBUG* stDebug = &m_pMemoryLog[iIndex];
#endif

        // 
        st_CMP_NODE stCmpNode;

        do
        {
            stCmpNode._pNode = m_pTop._pNode;
            stCmpNode._uiID = m_pTop._uiID;

            if (stCmpNode._pNode == nullptr)
            {
                g_Dump.Crash();
                return;
            }

#ifdef __DEBUG
            // 메모리 로깅..
            stDebug->_dwThreadID = GetCurrentThreadId();
            stDebug->_eType = e_TYPE::TYPE_POP_TRY;

            stDebug->_pOriginTop = m_pTop._pNode;
            stDebug->_pOriginNext = stDebug->_pOriginTop != nullptr ? stDebug->_pOriginTop->_pNext : nullptr;

            stDebug->_pCopyTop = stCmpNode._pNode;
            stDebug->_pCopyNext = stDebug->_pCopyTop != nullptr ? stDebug->_pCopyTop->_pNext : nullptr;

            stDebug->_pChangeNode = stCmpNode._pNode->_pNext;
#endif
        } while (!InterlockedCompareExchange128((LONG64*)&m_pTop, m_pTop._uiID + 1, (LONG64)m_pTop._pNode->_pNext, (LONG64*)&stCmpNode));

        *pOut = stCmpNode._pNode->_tData;

        m_Pool.Free(stCmpNode._pNode);
        InterlockedDecrement(&m_ulSize);
    }

private:


    CLFMemoryPool<st_NODE>      m_Pool;
    alignas(16) st_CMP_NODE     m_pTop;

    ULONG64                     m_ulSize;

#ifdef __DEBUG
    st_DEBUG* m_pMemoryLog;
    ULONG64                     m_lDebugIndex;
#endif
};

