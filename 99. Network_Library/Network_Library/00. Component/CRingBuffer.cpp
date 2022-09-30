#include "CRingBuffer.h"

CRingBuffer::CRingBuffer()
    : m_iFront(0)
    , m_iRear(0)
    , m_pStart(new char[DEFAULT_SIZE])
    , m_iBufferSize(DEFAULT_SIZE)
    , m_SRWLock()
{
    InitializeSRWLock(&m_SRWLock);
}

CRingBuffer::CRingBuffer(int size)
    : m_iFront(0)
    , m_iRear(0)
    , m_pStart(new char[size])
    , m_iBufferSize(size)
    , m_SRWLock()
{
    InitializeSRWLock(&m_SRWLock);
}

CRingBuffer::~CRingBuffer()
{
    delete[] m_pStart;
}

int CRingBuffer::GetBufferSize() const
{
    return m_iBufferSize;
}

int CRingBuffer::GetUseSize() const
{
    int iFront = m_iFront;
    int iRear = m_iRear;
    
    if (iFront == iRear)
        return 0;
    else if (iFront < iRear)
        return iRear - iFront;
    else
        return m_iBufferSize - m_iFront + m_iRear;
}

int CRingBuffer::GetFreeSize() const
{
    return (m_iBufferSize - 1) - GetUseSize();
}

int CRingBuffer::DirectEnqueueSize() const
{
    int iRear = m_iRear;
    int iFront = m_iFront;

    if ((iRear + 1) % m_iBufferSize == 0)
        return GetFreeSize();
    else if ((iRear + 1) % m_iBufferSize == iFront)
        return 0;
    else if (iRear < iFront)
        return iFront - iRear - 1;
    else
        return (m_iBufferSize - 1) - iRear;
}

int CRingBuffer::DirectDequeueSize() const
{
    int iRear = m_iRear;
    int iFront = m_iFront;

    if ((iFront + 1) % m_iBufferSize == 0)
        return GetUseSize();
    else if (iFront < iRear)
        return iRear - iFront;
    else if (iFront == iRear)
        return 0;
    else
        return (m_iBufferSize - 1) - iFront;
}

int CRingBuffer::Enqueue(const char* src, int size)
{
    int iWritePos = (m_iRear + 1) % m_iBufferSize;
    int iMaxSize = m_iBufferSize - iWritePos;

    // 한 반퀴 안 넘어갈 때
    if (iMaxSize >= size)
    {
        memcpy(&m_pStart[iWritePos], src, size);
    }
    // 한 바퀴 넘어갈 때
    else
    {
        memcpy(&m_pStart[iWritePos], src, iMaxSize);
        memcpy(m_pStart, &src[iMaxSize], size - iMaxSize);
    }

    m_iRear = (iWritePos + size - 1) % m_iBufferSize;

    return size;
}

int CRingBuffer::Dequeue(char* dest, int size)
{
    int iReadPos = (m_iFront + 1) % m_iBufferSize;
    int iMaxSize = m_iBufferSize - iReadPos;

    // 한 반퀴 안 넘어갈 때
    if (iMaxSize >= size)
    {
        memcpy(dest, &m_pStart[iReadPos], size);
    }
    // 한 바퀴 넘어갈 때
    else
    {
        memcpy(dest, &m_pStart[iReadPos], iMaxSize);
        memcpy(&dest[iMaxSize], m_pStart, size - iMaxSize);
    }

    m_iFront = (iReadPos + size - 1) % m_iBufferSize;

    return size;
}

int CRingBuffer::Peek(char* dest, int size)
{
    int iReadPos = (m_iFront + 1) % m_iBufferSize;
    int iMaxSize = m_iBufferSize - iReadPos;

    // 한 반퀴 안 넘어갈 때
    if (iMaxSize >= size)
    {
        memcpy(dest, &m_pStart[iReadPos], size);
    }
    // 한 바퀴 넘어갈 때
    else
    {
        memcpy(dest, &m_pStart[iReadPos], iMaxSize);
        memcpy(&dest[iMaxSize], m_pStart, size - iMaxSize);
    }

    return size;
}

void CRingBuffer::MoveFront(int size)
{
    m_iFront = (m_iFront + size) % m_iBufferSize;
}

void CRingBuffer::MoveRear(int size)
{
    m_iRear = (m_iRear + size) % m_iBufferSize;
}

void CRingBuffer::ClearBuffer()
{
    m_iFront = 0;
    m_iRear = 0;
}

char* CRingBuffer::GetFrontBufferPtr()
{
    return &m_pStart[(m_iFront + 1) % m_iBufferSize];
}

char* CRingBuffer::GetRearBufferPtr()
{
    return &m_pStart[(m_iRear + 1) % m_iBufferSize];
}

char* CRingBuffer::GetBufferStartPtr()
{
    return m_pStart;
}

void CRingBuffer::Lock()
{
    AcquireSRWLockExclusive(&m_SRWLock);
}

void CRingBuffer::Unlock()
{
    ReleaseSRWLockExclusive(&m_SRWLock);
}