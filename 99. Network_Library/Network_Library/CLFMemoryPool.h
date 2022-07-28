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

        struct st_NODE
        {
            UINT64                  _uiID;
            st_NODE* _pNext;
            UINT64                  _uiFreeFlag;
            CLFMemoryPool* _pCheckStart;
            T                       _tData;
            CLFMemoryPool* _pCheckEnd;
        };

        struct st_CMP_NODE
        {
            st_NODE* _pNode;
            UINT64                  _uiID;
        };

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

        alignas(16) st_CMP_NODE m_pTop;
        UINT64                  m_uiID;
        st_DEBUG* m_pMemoryLog;

        UINT64                  m_lDebugIndex;
        long                    m_lCapacity;
        long                    m_lUseCount;

        bool                    m_bUsePlacement;

        CCrashDump g_Dump;
    };

    template <typename T>
    CLFMemoryPool<T>::CLFMemoryPool(bool bUsePlacement)
        : m_pTop{ nullptr, 0 }
        , m_uiID(0)
        , m_pMemoryLog(new st_DEBUG[dfMEMORY_LOG_CNT])
        , m_lDebugIndex(-1)
        , m_lCapacity(0)
        , m_lUseCount(0)
        , m_bUsePlacement(bUsePlacement)
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

        // �޸� �α� ���� �ε���, ī��Ʈ ����
        ULONG64 iIndex;

        iIndex = InterlockedIncrement(&m_lDebugIndex);
        iIndex %= dfMEMORY_LOG_CNT;
        st_DEBUG* stDebug = &m_pMemoryLog[iIndex];
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
                pCopyNode->_uiFreeFlag = dfSTATE_ALLOC;
                pCopyNode->_pCheckStart = this;
                pCopyNode->_pCheckEnd = this;
                pCopyNode->_pNext = nullptr;

                InterlockedIncrement(&m_lCapacity);

#ifdef _DEBUG
                // �޸� �α�..
                stDebug->_dwThreadID = GetCurrentThreadId();
                stDebug->_eType = e_TYPE::TYPE_POP_TRY;
#endif

                break;
            }

#ifdef _DEBUG
            // �޸� �α�..
            stDebug->_dwThreadID = GetCurrentThreadId();
            stDebug->_eType = e_TYPE::TYPE_POP_TRY;
#endif

            if (pCopyNode->_pNext != nullptr)
                uiID = pCopyNode->_pNext->_uiID;

        } while (!InterlockedCompareExchange128((LONG64*)&m_pTop, uiID, (LONG64)pCopyNode->_pNext, (LONG64*)&stCmpNode));

        //InterlockedExchange(&pCopyNode->_uiFreeFlag, dfSTATE_ALLOC);
        pCopyNode->_uiFreeFlag = dfSTATE_ALLOC;

        pData = &(pCopyNode->_tData);
        if (m_bUsePlacement)
            new(pData) T;

        InterlockedIncrement(&m_lUseCount);

#ifdef _DEBUG
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
        st_NODE* pPushNode = (st_NODE*)(((char*)data) - sizeof(CLFMemoryPool*) - sizeof(UINT64) - sizeof(UINT64) - sizeof(st_NODE*));

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

        pPushNode->_uiID++;
        pPushNode->_uiFreeFlag = dfSTATE_RELEASE;

        do
        {
            stCmpNode._pNode = m_pTop._pNode;
            stCmpNode._uiID = m_pTop._uiID;

            pPushNode->_pNext = stCmpNode._pNode;

#ifdef _DEBUG
            // �޸� �α�..
            stDebug->_dwThreadID = GetCurrentThreadId();
            stDebug->_eType = e_TYPE::TYPE_PUSH_TRY;
#endif

        } while (!InterlockedCompareExchange128((LONG64*)&m_pTop, (LONG64)pPushNode->_uiID, (LONG64)pPushNode, (LONG64*)&stCmpNode));

        if (m_bUsePlacement)
            pPushNode->_tData.~T();

        InterlockedDecrement(&m_lUseCount);

#ifdef _DEBUG
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
