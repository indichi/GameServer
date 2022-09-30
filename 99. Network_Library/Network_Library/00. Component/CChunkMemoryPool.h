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
private:
	CChunk<T>*					m_Chunk;
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
	T* ret = &(m_Chunk->_tData[m_Chunk->_dwIndex++]._tData);

	// Index 체크해서 다 쓴 청크면 다시 alloc 받아와야함
	if (m_Chunk->_dwIndex >= dfCHUNK_NODE_COUNT)
	{
		m_Chunk = m_MainPool->Alloc();
	}

	return ret;
}

