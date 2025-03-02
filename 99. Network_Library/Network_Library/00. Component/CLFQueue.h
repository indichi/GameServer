#pragma once

#include <Windows.h>
#include "CLFMemoryPool.h"

using namespace procademy;

#define DEBUG_CNT   (10000)

template <typename T>
class CLFQueue
{
private:
    struct st_NODE
    {
        st_NODE*    _pNext;
        T           _tData;
    };

    struct st_CMP
    {
        st_NODE*    _pNode;
        DWORD64     _dwID;
    };

    
    alignas(16) st_NODE*        m_pHead;
    DWORD64                     m_dwHeadID;
    
    alignas(64) st_NODE*        m_pTail;
    DWORD64                     m_dwTailID;

    alignas(64) DWORD64         m_dwHeadCheckID;
    alignas(64) DWORD64         m_dwTailCheckID;

    
    alignas(64) CLFMemoryPool<st_NODE>*     m_Pool;
    alignas(64) int                         m_iSize;

#ifdef __LFQUEUE_DEBUG
public:
    enum class e_TYPE
    {
        ENQUEUE     =       0,
        DEQUEUE              ,
        DEQUEUE_SIZE_CHECK
    };

    struct st_DEBUG
    {
        DWORD       dwThreadID;
        UINT64      uiDebugCnt;
        e_TYPE      eType;
        st_NODE*    pOrigin;
        st_NODE*    pOriginNext;
        st_NODE*    pCopy;
        st_NODE*    pCopyNext;
        st_NODE*    pChangeNode;
        st_NODE*    pChangeNodeNext;
        int         iSize;
    };

    UINT64      m_uiDebugIndex;
    UINT64      m_uiDebugCnt;
    st_DEBUG*   m_pDebug;
#endif
    
public:
    CLFQueue()
        : m_pHead()
        , m_dwHeadID(0)
        , m_pTail()
        , m_dwTailID(0)
        , m_Pool(new CLFMemoryPool<st_NODE>)
        , m_iSize(0)
        , m_dwHeadCheckID(0)
        , m_dwTailCheckID(0)
    {
        m_pHead = m_Pool->Alloc();
        m_pHead->_pNext = nullptr;

        m_pTail = m_pHead;

#ifdef __LFQUEUE_DEBUG
        m_uiDebugIndex = -1;
        m_uiDebugCnt = 0;
        m_pDebug = new st_DEBUG[DEBUG_CNT];
#endif
    }

    ~CLFQueue()
    {}

    int GetSize() const { return m_iSize; }
    int GetPoolAllocSize() const { return m_Pool->GetCapacity(); }

    void Enqueue(T tData)
    {
        st_CMP stCmp;

        st_NODE* pNewNode = m_Pool->Alloc();
        pNewNode->_tData = tData;
        pNewNode->_pNext = nullptr;

#ifdef __LFQUEUE_DEBUG
        UINT64 uiIndex = InterlockedIncrement(&m_uiDebugIndex);
        uiIndex %= DEBUG_CNT;

        st_DEBUG* pDebug = &m_pDebug[uiIndex];
        pDebug->uiDebugCnt = InterlockedIncrement(&m_uiDebugCnt);
        pDebug->pChangeNode = pNewNode;
        pDebug->pChangeNode = pNewNode->_pNext;
#endif

        DWORD64 dwCheckID = InterlockedIncrement64((LONG64*)&m_dwTailCheckID);

        while (1)
        {
            // 현재 Tail 복사
            stCmp._pNode = m_pTail;
            stCmp._dwID = m_dwTailID;

            // next가 nullptr이 아니란 것은 다른 스레드에서 Enqueue 중 Tail에 노드 삽입 성공했다는 의미
            // tail을 한칸 옮겨주고 다시 시도
            if (stCmp._pNode->_pNext != nullptr)
            {
                InterlockedCompareExchange128((LONG64*)&m_pTail, dwCheckID, (LONG64)stCmp._pNode->_pNext, (LONG64*)&stCmp);
                dwCheckID = InterlockedIncrement64((LONG64*)&m_dwTailCheckID);
                continue;
            }

#ifdef __LFQUEUE_DEBUG
            pDebug->dwThreadID = GetCurrentThreadId();
            pDebug->eType = e_TYPE::ENQUEUE;
            pDebug->pOrigin = m_pTail;
            pDebug->pOriginNext = m_pTail->_pNext;
            pDebug->pCopy = stCmp._pNode;
            pDebug->pCopy = stCmp._pNode->_pNext;
#endif
            // Enqueue 시도
            // 1st CAS (새로운 노드를 tail복사본->next로 변경)
            if (InterlockedCompareExchangePointer((PVOID*)&stCmp._pNode->_pNext, pNewNode, nullptr) == nullptr)
            {
                // 2nd CAS (tail의 복사본이 현재 tail이 맞으면 tail복사본->next[새로운 노드]로 변경)
                InterlockedCompareExchange128((LONG64*)&m_pTail, dwCheckID, (LONG64)stCmp._pNode->_pNext, (LONG64*)&stCmp);
                break;
            }
        }

        int iSize = InterlockedIncrement((long*)&m_iSize);

#ifdef __LFQUEUE_DEBUG
        pDebug->iSize = iSize;
#endif
    }

    bool Dequeue(volatile T* pOut)
    {
#ifdef __LFQUEUE_DEBUG
        UINT64 uiIndex = InterlockedIncrement(&m_uiDebugIndex);
        uiIndex %= DEBUG_CNT;

        st_DEBUG* pDebug = &m_pDebug[uiIndex];
        pDebug->uiDebugCnt = InterlockedIncrement(&m_uiDebugCnt);
#endif
        // size check
        if (InterlockedDecrement((long*)&m_iSize) < 0)
        {
            InterlockedIncrement((long*)&m_iSize);
#ifdef __LFQUEUE_DEBUG
            pDebug->dwThreadID = GetCurrentThreadId();
            pDebug->eType = e_TYPE::DEQUEUE_SIZE_CHECK;
            pDebug->pOrigin = nullptr;
            pDebug->pOriginNext = nullptr;
            pDebug->pCopy = nullptr;
            pDebug->pCopyNext = nullptr;
            pDebug->iSize = iSize;
#endif
            return false;
        }

        st_CMP stCmpTail;
        st_CMP stCmpHead;
        st_NODE* pHeadNext;

        T tData;

        DWORD64 dwCheckID = InterlockedIncrement64((LONG64*)&m_dwHeadCheckID);

        while (1)
        {
#ifdef __LFQUEUE_DEBUG
            st_CMP stCmp;

            stCmp._pNode = m_pHead;
            stCmp._uiID = m_dwHeadID;
#endif
            // 현재 Tail 복사
            stCmpTail._pNode = m_pTail;
            stCmpTail._dwID = m_dwTailID;

            // 현재 Head 복사
            stCmpHead._pNode = m_pHead;
            stCmpHead._dwID = m_dwHeadID;

            pHeadNext = stCmpHead._pNode->_pNext;

            // head와 tail이 같은 노드를 가르키고 있음 (비어있는 상황)
            // 지금 tail의 next가 null이 아니다 -> 현재 다른 쓰레드에서 Enqueue하면서 1st CAS만 성공했음..
            // 그럴 경우 아무 데이터도 없었을 때 Enqueue 1st CAS만 성공했으면 head->next 는 null이 아닐것임 (2nd CAS실패하면서 size로 체크해서 나타 날 수 있는 문제) 
            // 이 상태에서 Dequeue가 완료되고 1st CAS 진행한 쓰레드에서 2nd CAS를 하려고 하면 tail은 이미 메모리 풀로 free 된 노드임
            // 이후에 다른 쓰레드에서 Enqueue할 때 free된 노드(현재 tail이 가르키고 있는)가 재할당되고(alloc) 노드의 next를 null로 밀면 그 뒤에 달려있던 노드는 분실됨
            // 이어서 1st CAS하면 tail->next = tail이 되는 기이한 현상이 일어남
            
            // tail 한번 뒤로 옮겨주고 다시 진행....
            if (stCmpTail._pNode->_pNext != nullptr)
            {
                DWORD64 dwTailCheckID = InterlockedIncrement64((LONG64*)&m_dwTailCheckID);
                InterlockedCompareExchange128((LONG64*)&m_pTail, (LONG64)dwTailCheckID, (LONG64)stCmpTail._pNode->_pNext, (LONG64*)&stCmpTail);

                continue;
            }
            
            // next가 null이 아닐때 진행
            if (pHeadNext == nullptr)
                continue;

            tData = pHeadNext->_tData;

#ifdef __LFQUEUE_DEBUG
            pDebug->dwThreadID = GetCurrentThreadId();
            pDebug->eType = e_TYPE::DEQUEUE;
            pDebug->pOrigin = m_pHead;
            pDebug->pOriginNext = m_pHead->_pNext;
            pDebug->pCopy = stCmp._pNode;
            pDebug->pCopyNext = stCmp._pNode->_pNext;
#endif
            
            if (InterlockedCompareExchange128((LONG64*)&m_pHead, (LONG64)dwCheckID, (LONG64)stCmpHead._pNode->_pNext, (LONG64*)&stCmpHead))
            {
#ifdef __LFQUEUE_DEBUG
                pDebug->iSize = iSize;
                pDebug->pChangeNode = pDebug->pOrigin->_pNext;
                pDebug->pChangeNodeNext = pDebug->pChangeNode->_pNext;
#endif
                *pOut = tData;
                break;
            }
        }

        m_Pool->Free(stCmpHead._pNode);     // 메모리 풀에 반환
        return true;
    }
};