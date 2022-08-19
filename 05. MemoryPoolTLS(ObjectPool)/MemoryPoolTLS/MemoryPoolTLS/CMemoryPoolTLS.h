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

    // 존재하지 않음 -> 쓰레드 첫 등록 -> TLS에 Set
    if (chunkPool == nullptr)
    {
        chunkPool = new CChunkMemoryPool<T>(m_MainPool);
        TlsSetValue(m_dwTLSindex, chunkPool);
        //if (!TlsSetValue(m_dwTLSindex, chunkPool))
        //{
        //    // 크래쉬
        //}
    }

    return chunkPool->Alloc();
}

template<typename T>
bool CMemoryPoolTLS<T>::Free(volatile T* data)
{
    // 데이터옆에 숨겨둔 this를 통해서 내가 속해있었던 CChunk의 FreeCount를 증가 및 확인해서 최대치면 메인 메모리풀에 free (다른 쓰레드에서 free 할 수도 있으니 interlockedincrement로)

    // 내가 속한 chunk 찾기
    CChunk<T>* chunk = *(CChunk<T>**)((char*)data - sizeof(CChunk<T>*));

    // Free Count를 증가시키고 다 free 됐는지 확인 -> 다 됐으면 main pool에 chunk 반환
    if (InterlockedIncrement(&chunk->_dwFreeCount) == dfCHUNK_NODE_COUNT)
    {
        chunk->_dwFreeCount = 0;
        chunk->_dwIndex = 0;
        m_MainPool->Free(chunk);
    }

    return true;
}
