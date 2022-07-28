#include "CRingBuffer.h"

#include <memory>

CRingBuffer::CRingBuffer()
    : mFront(0)
    , mRear(0)
    , mStartPtr(new char[DEFAULT_SIZE + 1])
    , mBufferSize(DEFAULT_SIZE + 1)
{
}

CRingBuffer::CRingBuffer(int size)
    : mFront(0)
    , mRear(0)
    , mStartPtr(new char[size + 1])
    , mBufferSize(size + 1)
{
}

CRingBuffer::~CRingBuffer()
{
    delete[] mStartPtr;
}

int CRingBuffer::GetBufferSize() const
{
    return mBufferSize - 1;
}

int CRingBuffer::GetUseSize() const
{
    int use_size;
    int iFront = mFront;
    int iRear = mRear;

    if (iFront > iRear)
    {
        use_size = mBufferSize - iFront;
        use_size += iRear;
    }
    else
    {
        use_size = iRear - iFront;
    }

    return use_size;
}

int CRingBuffer::GetFreeSize() const
{
    return (mBufferSize - 1) - GetUseSize();
}

int CRingBuffer::DirectEnqueueSize() const
{
    int iFront = mFront;
    int iRear = mRear;

    if (iRear % (mBufferSize - 1) == 0)
        return GetFreeSize();
    else if ((iRear + 1) % mBufferSize == iFront)
        return 0;
    else if (iRear < iFront)
        return iFront - iRear - 1;
    else
        return mBufferSize - 1 - iRear;
}

int CRingBuffer::DirectDequeueSize() const
{
    int iFront = mFront;
    int iRear = mRear;

    if (iFront % mBufferSize == 0)
        return GetUseSize();
    else if (iFront == iRear)
        return 0;
    else if (iFront < iRear)
        return iRear - iFront;
    else
        return mBufferSize - iFront;
}

int CRingBuffer::Enqueue(const char* src, int size)
{
    if (nullptr == src || size < 1)
    {
        return 0;
    }

    int free_size = GetFreeSize();
    int cpy_size;
    int iRear = mRear;

    // 남은 공간보다 size가 클 때
    if (free_size < size)
    {
        cpy_size = free_size;
    }
    else
    {
        cpy_size = size;
    }

    char* rear_ptr = mStartPtr + iRear;

    // 한 바퀴 돌아서 넘어갈 때
    if (iRear + cpy_size > mBufferSize)
    {
        int free_end_size = mBufferSize - iRear;
        int remain_size = cpy_size - free_end_size;
        const char* source = src;

        memcpy_s(rear_ptr, free_end_size, source, free_end_size);
        source += free_end_size;
        memcpy_s(mStartPtr, remain_size, source, remain_size);
    }
    else
    {
        memcpy_s(rear_ptr, cpy_size, src, cpy_size);
    }

    mRear = (iRear + size) % (mBufferSize);

    return cpy_size;
}

int CRingBuffer::Dequeue(char* dest, int size)
{
    if (nullptr == dest || size < 1)
    {
        return 0;
    }

    int use_size = GetUseSize();
    int cpy_size;
    int iFront = mFront;

    // 사용 공간보다 size가 클 때
    if (use_size < size)
    {
        cpy_size = use_size;
    }
    else
    {
        cpy_size = size;
    }

    char* front_ptr = mStartPtr + iFront;

    // 한 바퀴 돌아서 넘어갈 때
    if (iFront + cpy_size > mBufferSize)
    {
        int free_end_size = mBufferSize - iFront;
        int remain_size = cpy_size - free_end_size;
        char* dst = dest;

        memcpy_s(dst, free_end_size, front_ptr, free_end_size);
        dst += free_end_size;
        memcpy_s(dst, remain_size, mStartPtr, remain_size);
    }
    else
    {
        memcpy_s(dest, cpy_size, front_ptr, cpy_size);
    }

    mFront = (iFront + size) % (mBufferSize);

    return cpy_size;
}

int CRingBuffer::Peek(char* dest, int size)
{
    if (nullptr == dest || size < 1)
    {
        return 0;
    }

    int use_size = GetUseSize();
    int cpy_size;
    int iFront = mFront;

    // 사용 공간보다 size가 클 때
    if (use_size < size)
    {
        cpy_size = use_size;
    }
    else
    {
        cpy_size = size;
    }

    char* front_ptr = mStartPtr + iFront;

    // 한 바퀴 돌아서 넘어갈 때
    if (iFront + cpy_size > mBufferSize)
    {
        int free_end_size = mBufferSize - iFront;
        int remain_size = cpy_size - free_end_size;
        char* dst = dest;

        memcpy_s(dst, free_end_size, front_ptr, free_end_size);
        dst += free_end_size;
        memcpy_s(dst, remain_size, mStartPtr, remain_size);
    }
    else
    {
        memcpy_s(dest, cpy_size, front_ptr, cpy_size);
    }

    return cpy_size;
}

void CRingBuffer::MoveFront(int size)
{
    mFront = (mFront + size) % (mBufferSize);
}

void CRingBuffer::MoveRear(int size)
{
    mRear = (mRear + size) % (mBufferSize);
}

void CRingBuffer::ClearBuffer()
{
    mFront = 0;
    mRear = 0;
}

char* CRingBuffer::GetFrontBufferPtr()
{
    return mStartPtr + mFront;
}

char* CRingBuffer::GetRearBufferPtr()
{
    return mStartPtr + mRear;
}

//bool CRingBuffer::IsEmpty() const
//{
//    return mFront == mRear;
//}
//
//bool CRingBuffer::IsFull() const
//{
//    return 0 == GetFreeSize();
//}