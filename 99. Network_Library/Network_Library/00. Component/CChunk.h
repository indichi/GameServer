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
        CChunk<T>*      _pThis;                 // T타입 데이터가 본인 소속의 Chunk를 찾아가기 위한 Chunk주소 변수
        T               _tData;                 // 데이터
    };

    DWORD                   _dwFreeCount;       // Chunk의 해제 Count
    DWORD                   _dwIndex;           // 현재 Chunk가 할당할 배열 순서의 Index

    st_DATA*                _tData;             // Chunk의 데이터 배열
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
