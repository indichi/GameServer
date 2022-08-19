#pragma once

#include <unordered_map>

#include "CChunk.h"
#include "CLFMemoryPool.h"
#include "CChunkMemoryPool.h"

using namespace procademy;

template <typename T>
class CMemoryPoolTLS
{
public:
    CMemoryPoolTLS();
    ~CMemoryPoolTLS();

    T* Alloc();
    bool Free(volatile T* data);

private:
    CLFMemoryPool<CChunk<T>>*                           m_MainPool;
    DWORD                                               m_dwTLSindex;
};

template<typename T>
CMemoryPoolTLS<T>::CMemoryPoolTLS()
    : m_MainPool(new CLFMemoryPool<CChunk<T>>(false))
{
    m_dwTLSindex = TlsAlloc();
    if (m_dwTLSindex == TLS_OUT_OF_INDEXES)
        exit(-1);
}

template<typename T>
CMemoryPoolTLS<T>::~CMemoryPoolTLS()
{
    delete m_MainPool;
}

template<typename T>
T* CMemoryPoolTLS<T>::Alloc()
{
    //DWORD dwIndex = m_dwTLSindex;
    CChunkMemoryPool<T>* chunkPool = (CChunkMemoryPool<T>*)TlsGetValue(m_dwTLSindex);

    // �������� ���� -> ������ ù ��� -> TLS�� Set
    if (chunkPool == nullptr)
    {
        chunkPool = new CChunkMemoryPool<T>(m_MainPool);
        TlsSetValue(m_dwTLSindex, chunkPool);
        //if (!TlsSetValue(m_dwTLSindex, chunkPool))
        //{
        //    // ũ����
        //}
    }

    return chunkPool->Alloc();
}

template<typename T>
bool CMemoryPoolTLS<T>::Free(volatile T* data)
{
    // �����Ϳ��� ���ܵ� this�� ���ؼ� ���� �����־��� CChunk�� FreeCount�� ���� �� Ȯ���ؼ� �ִ�ġ�� ���� �޸�Ǯ�� free (�ٸ� �����忡�� free �� ���� ������ interlockedincrement��)

    // ���� ���� chunk ã��
    CChunk<T>* chunk = *(CChunk<T>**)((char*)data - sizeof(CChunk<T>*));

    // Free Count�� ������Ű�� �� free �ƴ��� Ȯ�� -> �� ������ main pool�� chunk ��ȯ
    if (InterlockedIncrement(&chunk->_dwFreeCount) == dfCHUNK_NODE_COUNT)
    {
        chunk->_dwFreeCount = 0;
        chunk->_dwIndex = 0;
        m_MainPool->Free(chunk);
    }

    return true;
}
