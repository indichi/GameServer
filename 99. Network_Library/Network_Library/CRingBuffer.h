#pragma once

#include <Windows.h>

#define DEFAULT_SIZE    (10000)

class CRingBuffer
{
public:
    CRingBuffer();
    CRingBuffer(int size);
    CRingBuffer(const CRingBuffer& other) = delete;

    ~CRingBuffer();

    int GetBufferSize() const;
    int GetUseSize() const;
    int GetFreeSize() const;

    int DirectEnqueueSize() const;
    int DirectDequeueSize() const;

    int Enqueue(const char* src, int size);
    int Dequeue(char* dest, int size);
    int Peek(char* dest, int size);

    void MoveFront(int size);
    void MoveRear(int size);
    
    void ClearBuffer();

    char* GetFrontBufferPtr();
    char* GetRearBufferPtr();
    char* GetBufferStartPtr();

    /*bool IsEmpty() const;
    bool IsFull() const;*/

    void Lock();
    void Unlock();

private:
    int             m_iFront;
    int             m_iRear;
    char* const     m_pStart;
    int             m_iBufferSize;

    SRWLOCK         m_SRWLock;
};

