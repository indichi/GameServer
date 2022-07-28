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
        // ������� free�� �����͵鸸 �����ϰ� �������� ����
        st_NODE* next;

        // Placement new ��� �� �޸� ������ ���� (Free�� �� �Ҹ��� ȣ�� ��Ŵ)
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

        // placement new ���� �߰� �� free�Լ�, �Ҹ��ڵ� �߰�
        // free node �� ���� ��.. (���� �Ҵ� �޾ƾ� �� �� -> ��������Ʈ ����)
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
        pNode->freeFlag = STATE_ALLOC;  // free falg false ����

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

        // st_NODE* ���� �ּ� = T* data�� �ּ� - checkStart ������ - freeflag ������ - st_NODE* ������
        st_NODE* node = (st_NODE*)(((char*)data) - sizeof(CMemoryPool*) - sizeof(UINT64) - sizeof(st_NODE*));
        
        // this�� üũ�� Ȯ�� (�� Ǯ���� ���� ������Ʈ�� �´���)
        // (+ ��� �÷ο� üũ)
        if (node->checkStart != this)
        {
            WCHAR buf[128];
            wsprintf(buf, L"Free(T* data), not this pool member or check underflow");
            WriteLog(buf);
            ReleaseSRWLockExclusive(&mLocker);
            return false;
        }
        
        // (+ ���� �÷ο� üũ)
        if (node->checkEnd != this)
        {
            WCHAR buf[128];
            wsprintf(buf, L"Free(T* data), not this pool member or check overflow");
            WriteLog(buf);
            ReleaseSRWLockExclusive(&mLocker);
            return false;
        }

        // free flag Ȯ�� (�̹� free �� �༮�� �ƴ���..)
        // �̹� �ݳ��� �༮�̶��..
        if (node->freeFlag == STATE_RELEASE)
        {
            WCHAR buf[128];

            wsprintf(buf, L"Free(T* data), this parameter is already free");
            WriteLog(buf);

            // �ϴ� �α״� ��� free�� ������ �� free �Ϸ��� �ϸ� ũ���� ������ ���ؾ���....
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

        // free true�� ���� (�ݳ� �Ǿ����� �ǹ�)
        node->freeFlag = STATE_RELEASE;

        node->next = mFreeNode;
        mFreeNode = node;

        // Placement new ��� �� �������� �Ҹ��� ȣ�������
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
