#pragma once

#include "CChunk.h"
#include "CLFMemoryPool.h"

using namespace procademy;

template <typename T>
class CChunkMemoryPool
{
public:
	CChunkMemoryPool() = delete;
	CChunkMemoryPool(CLFMemoryPool<CChunk<T>>* _MainPool);
	~CChunkMemoryPool();

	T* Alloc();
	bool Free(volatile T* data);
private:
	CChunk<T>*					m_Chunk;

	alignas(64)
	CLFMemoryPool<CChunk<T>>*	m_MainPool;
};

template <typename T>
CChunkMemoryPool<T>::CChunkMemoryPool(CLFMemoryPool<CChunk<T>>* _MainPool)
	: m_Chunk()
	, m_MainPool(_MainPool)
{
	m_Chunk = m_MainPool->Alloc();
}

template <typename T>
CChunkMemoryPool<T>::~CChunkMemoryPool()
{
}

template<typename T>
T* CChunkMemoryPool<T>::Alloc()
{
	CChunk<T>* pChunk = m_Chunk;
	T* ret = &(pChunk->_tData[pChunk->_dwIndex]._tData);
	++pChunk->_dwIndex;

	// Index 체크해서 다 쓴 청크면 다시 alloc 받아와야함
	if (pChunk->_dwIndex == dfCHUNK_NODE_COUNT)
		m_Chunk = m_MainPool->Alloc();
	
	return ret;
}

template<typename T>
bool CChunkMemoryPool<T>::Free(volatile T* data)
{
	//// 내가 속한 chunk 찾기
	//CChunk<T>* chunk = (CChunk<T>*)(data - sizeof(T));
	//
	//// Free Count를 증가시키고 다 free 됐는지 확인 -> 다 됐으면 main pool에 chunk 반환
	//if (InterlockedIncrement(&chunk->_dwFreeCount) == dfCHUNK_NODE_COUNT)
	//{
	//	m_MainPool->Free(chunk);
	//}

	return false;
}
