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
        CChunk<T>*  _pThis;
        T           _tData;
    };

    DWORD                   _dwIndex;
    char                    _garbage1[64];
    DWORD                   _dwFreeCount;

    st_DATA                 _tData[dfCHUNK_NODE_COUNT];

private:
};

template<typename T>
CChunk<T>::CChunk()
    : _dwFreeCount(0)
    , _dwIndex(0)
{
    for (int i = 0; i < dfCHUNK_NODE_COUNT; ++i)
    {
        _tData[i]._pThis = this;
    }
}

template<typename T>
CChunk<T>::~CChunk()
{
}
