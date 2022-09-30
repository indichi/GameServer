#pragma once

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

#ifdef __LFMEM_DEBUG
        struct st_NODE
        {
            UINT64                  _uiID;
            st_NODE*                _pNext;
            UINT64                  _uiFreeFlag;
            CLFMemoryPool*          _pCheckStart;
            T                       _tData;
            CLFMemoryPool*          _pCheckEnd;
        };
#else
        struct st_NODE
        {
            UINT64                  _uiID;
            st_NODE*                _pNext;
            T                       _tData;
        };
#endif

        struct st_CMP_NODE
        {
            st_NODE*                _pNode;
            UINT64                  _uiID;
        };

#ifdef __LFMEM_DEBUG
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

        st_DEBUG*               m_pMemoryLog;
        UINT64                  m_lDebugIndex;
#endif

        alignas(16) st_CMP_NODE m_pTop;
        alignas(64) DWORD64     m_dwCheckID;

        long                    m_lCapacity;
        long                    m_lUseCount;

        bool                    m_bUsePlacement;
    };

    template <typename T>
    CLFMemoryPool<T>::CLFMemoryPool(bool bUsePlacement)
        : m_pTop{ nullptr, 0 }
        , m_dwCheckID(0)
        , m_lCapacity(0)
        , m_lUseCount(0)
        , m_bUsePlacement(bUsePlacement)
#ifdef __LFMEM_DEBUG
        , m_pMemoryLog(new st_DEBUG[dfMEMORY_LOG_CNT])
        , m_lDebugIndex(-1)
#endif
    {
    }

    template <typename T>
    CLFMemoryPool<T>::~CLFMemoryPool()
    {
        // ������� free�� �����͵鸸 �����ϰ� �������� ����
        while (m_pTop._pNode != nullptr)
        {
            st_NODE* pTemp = m_pTop._pNode;
            m_pTop._pNode = m_pTop._pNode->_pNext;
            delete pTemp;
        }
    }

    template <typename T>
    T* CLFMemoryPool<T>::Alloc()
    {
        // �޸� Ǯ ���忡�� Alloc�� pop�� ����
        st_CMP_NODE stCmpNode;
        st_NODE* pCopyNode;
        T* pData;

#ifdef __LFMEM_DEBUG
        // �޸� �α� ���� �ε���, ī��Ʈ ����
        ULONG64 iIndex;

        iIndex = InterlockedIncrement(&m_lDebugIndex);
        iIndex %= dfMEMORY_LOG_CNT;
        st_DEBUG* stDebug = &m_pMemoryLog[iIndex];
#endif

        DWORD64 dwID = InterlockedIncrement(&m_dwCheckID);

        do
        {
            stCmpNode._pNode = m_pTop._pNode;
            stCmpNode._uiID = m_pTop._uiID;

            pCopyNode = stCmpNode._pNode;

            if (pCopyNode == nullptr)
            {
                pCopyNode = new st_NODE;
                pCopyNode->_uiID = dwID;
                pCopyNode->_pNext = nullptr;

                InterlockedIncrement(&m_lCapacity);

#ifdef __LFMEM_DEBUG
                pCopyNode->_uiFreeFlag = dfSTATE_ALLOC;
                pCopyNode->_pCheckStart = this;
                pCopyNode->_pCheckEnd = this;

                // �޸� �α�..
                stDebug->_dwThreadID = GetCurrentThreadId();
                stDebug->_eType = e_TYPE::TYPE_POP_TRY;
#endif

                break;
            }

#ifdef __LFMEM_DEBUG
            // �޸� �α�..
            stDebug->_dwThreadID = GetCurrentThreadId();
            stDebug->_eType = e_TYPE::TYPE_POP_TRY;
#endif

            /*if (pCopyNode->_pNext != nullptr)
                uiID = pCopyNode->_pNext->_uiID;*/

        } while (!InterlockedCompareExchange128((LONG64*)&m_pTop, dwID, (LONG64)pCopyNode->_pNext, (LONG64*)&stCmpNode));

        pData = &(pCopyNode->_tData);
        if (m_bUsePlacement)
            new(pData) T;

        InterlockedIncrement(&m_lUseCount);

#ifdef __LFMEM_DEBUG
        pCopyNode->_uiFreeFlag = dfSTATE_ALLOC;

        // �޸� �α�..
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
        // �޸� Ǯ ���忡�� Free�� push�� ����
        st_CMP_NODE stCmpNode;
        st_NODE* pPushNode = (st_NODE*)((char*)data - (sizeof(st_NODE) - sizeof(T)));

#ifdef __LFMEM_DEBUG
        pPushNode = (st_NODE*)(((char*)data) - sizeof(CLFMemoryPool*) - sizeof(UINT64) - sizeof(UINT64) - sizeof(st_NODE*));
        // �޸� �α� ���� �ε���, ī��Ʈ ����
        ULONG64 iIndex;

        iIndex = InterlockedIncrement(&m_lDebugIndex);
        iIndex %= dfMEMORY_LOG_CNT;
        st_DEBUG* stDebug = &m_pMemoryLog[iIndex];

        // this�� üũ�� Ȯ�� (�� Ǯ���� ���� ������Ʈ�� �´���)
        // (+ ��� �÷ο� üũ)
        if (pPushNode->_pCheckStart != this)
        {
            WCHAR buf[128];
            wsprintf(buf, L"Free(T* data), not this pool member or check underflow");
            WriteLog(buf);

            g_Dump.Crash();
        }

        // (+ ���� �÷ο� üũ)
        if (pPushNode->_pCheckEnd != this)
        {
            WCHAR buf[128];
            wsprintf(buf, L"Free(T* data), not this pool member or check overflow");
            WriteLog(buf);

            g_Dump.Crash();
        }

        // free flag Ȯ�� (�̹� free �� �༮�� �ƴ���..)
        // �̹� �ݳ��� �༮�̶��..
        if (pPushNode->_uiFreeFlag == dfSTATE_RELEASE)
        {
            WCHAR buf[128];

            wsprintf(buf, L"Free(T* data), this parameter is already free");
            WriteLog(buf);

            // �ϴ� �α״� ��� free�� ������ �� free �Ϸ��� �ϸ� ũ���� ������ ���ؾ���....
            // crash();

            g_Dump.Crash();
        }
        else
        {
            if (pPushNode->_uiFreeFlag != dfSTATE_ALLOC)
            {
                WCHAR buf[128];

                wsprintf(buf, L"Free(T* data), not this pool member or check underflow");
                WriteLog(buf);
                g_Dump.Crash();
            }
        }

        pPushNode->_uiFreeFlag = dfSTATE_RELEASE;
#endif
        DWORD64 dwID = InterlockedIncrement(&m_dwCheckID);

        do
        {
            stCmpNode._pNode = m_pTop._pNode;
            stCmpNode._uiID = m_pTop._uiID;

            //pPushNode->_pNext = stCmpNode._pNode;
            pPushNode->_pNext = m_pTop._pNode;

#ifdef __LFMEM_DEBUG
            // �޸� �α�..
            stDebug->_dwThreadID = GetCurrentThreadId();
            stDebug->_eType = e_TYPE::TYPE_PUSH_TRY;
#endif

        } while (!InterlockedCompareExchange128((LONG64*)&m_pTop, dwID, (LONG64)pPushNode, (LONG64*)&stCmpNode));

        if (m_bUsePlacement)
            pPushNode->_tData.~T();

        InterlockedDecrement(&m_lUseCount);

#ifdef __LFMEM_DEBUG
        // �޸� �α�..
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