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

	// Index üũ�ؼ� �� �� ûũ�� �ٽ� alloc �޾ƿ;���
	if (pChunk->_dwIndex == dfCHUNK_NODE_COUNT)
		m_Chunk = m_MainPool->Alloc();
	
	return ret;
}

template<typename T>
bool CChunkMemoryPool<T>::Free(volatile T* data)
{
	//// ���� ���� chunk ã��
	//CChunk<T>* chunk = (CChunk<T>*)(data - sizeof(T));
	//
	//// Free Count�� ������Ű�� �� free �ƴ��� Ȯ�� -> �� ������ main pool�� chunk ��ȯ
	//if (InterlockedIncrement(&chunk->_dwFreeCount) == dfCHUNK_NODE_COUNT)
	//{
	//	m_MainPool->Free(chunk);
	//}

	return false;
}
