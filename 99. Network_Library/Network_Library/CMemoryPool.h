#pragma once

#include <iostream>
#include <Windows.h>
#include <tchar.h>

#define STATE_ALLOC     (0xffffffffffffffff)
#define STATE_RELEASE   (0xdddddddddddddddd)

namespace procademy
{
    template <typename T>
    class CMemoryPool
    {
    public:
        CMemoryPool(bool usePlacement = false);
        virtual ~CMemoryPool();

        T* Alloc();
        bool Free(T* data);

        int GetCapacity()        
        {
            int iRet;
            AcquireSRWLockShared(&mLocker);
            iRet = mCapacity; 
            ReleaseSRWLockShared(&mLocker);

            return iRet;
        }

        int GetUseCount()        
        {
            int iRet;
            AcquireSRWLockShared(&mLocker);
            iRet = mUseCount;
            ReleaseSRWLockShared(&mLocker);

            return iRet;
        }
    private:
        struct st_NODE {
            st_NODE*            next;
            UINT64              freeFlag;
            CMemoryPool*        checkStart;
            T                   data;
            CMemoryPool*        checkEnd;
        };

        void WriteLog(const WCHAR* log) const;

        int                 mCapacity;
        int                 mUseCount;
        st_NODE*            mFreeNode;
        bool                mUsePlacement;
        SRWLOCK             mLocker;
    };

    template <typename T>
    CMemoryPool<T>::CMemoryPool(bool usePlacement)
        : mCapacity(0)
        , mUseCount(0)
        , mFreeNode(nullptr)
        , mUsePlacement(usePlacement)
    {
        InitializeSRWLock(&mLocker);
    }

    template <typename T>
    CMemoryPool<T>::~CMemoryPool()
    {
        // 현재까지 free인 데이터들만 해제하고 나머지는 버림
        st_NODE* next;

        // Placement new 사용 시 메모리 공간만 해제 (Free할 때 소멸자 호출 시킴)
        if (mUsePlacement)
        {
            while (mFreeNode != nullptr)
            {
                next = mFreeNode->next;
                free(mFreeNode);
                mFreeNode = next;
            }
        }
        else
        {
            while (mFreeNode != nullptr)
            {
                next = mFreeNode->next;
                delete mFreeNode;
                mFreeNode = next;
            }
        }
    }

    template <typename T>
    T* CMemoryPool<T>::Alloc()
    {
        AcquireSRWLockExclusive(&mLocker);

        // placement new 내용 추가 및 free함수, 소멸자도 추가
        // free node 가 없을 때.. (새로 할당 받아야 할 때 -> 프리리스트 구조)
        if (mFreeNode == nullptr)
        {
            mFreeNode = new st_NODE;
            mFreeNode->next = nullptr;
            ++mCapacity;
        }

        st_NODE* pNode = mFreeNode;
        T* pData = &pNode->data;

        pNode->checkStart = this;
        pNode->checkEnd = this;
        pNode->freeFlag = STATE_ALLOC;  // free falg false 세팅

        mFreeNode = pNode->next;

        if (mUsePlacement)
        {
            new(pData) T;
        }

        ++mUseCount;

        ReleaseSRWLockExclusive(&mLocker);
        return pData;
    }

    template <typename T>
    bool CMemoryPool<T>::Free(T* data)
    {
        AcquireSRWLockExclusive(&mLocker);
        if (nullptr == data)
        {
            WCHAR buf[128];
            wsprintf(buf, L"Free(T* data), parameter is nullptr");
            WriteLog(buf);
            return false;
        }

        // st_NODE* 시작 주소 = T* data의 주소 - checkStart 사이즈 - freeflag 사이즈 - st_NODE* 사이즈
        st_NODE* node = (st_NODE*)(((char*)data) - sizeof(CMemoryPool*) - sizeof(UINT64) - sizeof(st_NODE*));
        
        // this로 체크섬 확인 (이 풀에서 나간 오브젝트가 맞는지)
        // (+ 언더 플로우 체크)
        if (node->checkStart != this)
        {
            WCHAR buf[128];
            wsprintf(buf, L"Free(T* data), not this pool member or check underflow");
            WriteLog(buf);
            ReleaseSRWLockExclusive(&mLocker);
            return false;
        }
        
        // (+ 오버 플로우 체크)
        if (node->checkEnd != this)
        {
            WCHAR buf[128];
            wsprintf(buf, L"Free(T* data), not this pool member or check overflow");
            WriteLog(buf);
            ReleaseSRWLockExclusive(&mLocker);
            return false;
        }

        // free flag 확인 (이미 free 된 녀석은 아닌지..)
        // 이미 반납된 녀석이라면..
        if (node->freeFlag == STATE_RELEASE)
        {
            WCHAR buf[128];

            wsprintf(buf, L"Free(T* data), this parameter is already free");
            WriteLog(buf);

            // 일단 로그는 썼고 free한 데이터 또 free 하려고 하면 크래쉬 내줄지 정해야함....
            // crash();
            ReleaseSRWLockExclusive(&mLocker);
            return false;
        }
        else
        {
            if (node->freeFlag != STATE_ALLOC)
            {
                WCHAR buf[128];

                wsprintf(buf, L"Free(T* data), not this pool member or check underflow");
                WriteLog(buf);
                ReleaseSRWLockExclusive(&mLocker);
                return false;
            }
        }

        // free true로 세팅 (반납 되었음을 의미)
        node->freeFlag = STATE_RELEASE;

        node->next = mFreeNode;
        mFreeNode = node;

        // Placement new 사용 시 수동으로 소멸자 호출시켜줌
        if (mUsePlacement)
        {
            data->~T();
        }

        --mUseCount;

        ReleaseSRWLockExclusive(&mLocker);
        return true;
    }

    template <typename T>
    void CMemoryPool<T>::WriteLog(const WCHAR* log) const
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
