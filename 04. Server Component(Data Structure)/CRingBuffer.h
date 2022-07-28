#pragma once

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

    /*bool IsEmpty() const;
    bool IsFull() const;*/
private:
    int             mFront;
    int             mRear;
    char* const     mStartPtr;
    int             mBufferSize;
};

