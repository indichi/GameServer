#pragma once

#define dfCHUNK_NODE_COUNT  (500)

#include <Windows.h>

template <typename T>
class CChunk
{
public:
    CChunk();
    ~CChunk();

    struct st_DATA
    {
        CChunk<T>*      _pThis;                 // TŸ�� �����Ͱ� ���� �Ҽ��� Chunk�� ã�ư��� ���� Chunk�ּ� ����
        T               _tData;                 // ������
    };

    DWORD                   _dwFreeCount;       // Chunk�� ���� Count
    DWORD                   _dwIndex;           // ���� Chunk�� �Ҵ��� �迭 ������ Index

    st_DATA*                _tData;             // Chunk�� ������ �迭
};

template<typename T>
CChunk<T>::CChunk()
    : _dwFreeCount(0)
    , _dwIndex(0)
    , _tData(new st_DATA[dfCHUNK_NODE_COUNT])
{
    for (int i = 0; i < dfCHUNK_NODE_COUNT; ++i)
    {
        _tData[i]._pThis = this;
    }
}

template<typename T>
CChunk<T>::~CChunk()
{
    delete[] _tData;
}
