#include <iostream>
#include <tchar.h>
#include "CLanServer.h"
#include "CNetServer.h"

#include "CPacket.h"
#include "CMemoryPool.h"
#include "CMemoryPoolTLS.h"

using namespace procademy;

CPacket::CPacket()
    : mBufferSize(eBUFFER_DEFAULT)
    , mDataSize(0)
    , mReadPos(0)
    , mWritePos(0)
    , mData(new char[eBUFFER_DEFAULT])
    , mRefCount(0)
{
}

CPacket::CPacket(int size)
    : mBufferSize(size)
    , mDataSize(0)
    , mReadPos(0)
    , mWritePos(0)
    , mData(new char[size])
    , mRefCount(0)
{
}

CPacket::CPacket(const CPacket& other)
    : mBufferSize(other.mBufferSize)
    , mDataSize(other.mDataSize)
    , mReadPos(other.mReadPos)
    , mWritePos(other.mWritePos)
    , mData(new char[other.mBufferSize])
    , mRefCount(other.mRefCount)
{
    memcpy(&mData[mReadPos], &other.mData[other.mReadPos], other.mDataSize);
}

CPacket::~CPacket()
{
    delete[] mData;
}

CPacket* CPacket::Alloc()
{
    CPacket* ret = m_PacketPoolTLS.Alloc();
    ret->mRefCount = 1;
    return ret;
}

bool CPacket::Free(volatile CPacket* pPacket)
{
    int iRefCnt = InterlockedDecrement(&pPacket->mRefCount);
    if (iRefCnt == 0)
    {
        pPacket->mRefCount = 0;

        pPacket->mReadPos = 0;
        pPacket->mWritePos = 0;
        pPacket->mDataSize = 0;
        return m_PacketPoolTLS.Free(pPacket);
    }

    return true;
}

void CPacket::Release(void)
{
}

void CPacket::Clear(void)
{
    mReadPos = 0;
    mWritePos = 0;
    mDataSize = 0;
}

int CPacket::MoveWritePos(int size)
{
    if (size < 1 || (mWritePos + size) > mBufferSize)
    {
        return 0;
    }

    mWritePos += size;
    mDataSize += size;

    return size;
}

int CPacket::MoveReadPos(int size)
{
    if (size < 1 || (mReadPos + size) > mBufferSize)
    {
        return 0;
    }

    mReadPos += size;
    mDataSize -= size;

    return size;
}

void CPacket::AddRefCount()
{
    InterlockedIncrement(&mRefCount);
}

void CPacket::SubRefCount()
{
    int iRefCnt = InterlockedDecrement(&mRefCount);
    if (iRefCnt == 0)
    {
        Clear();
        m_PacketPoolTLS.Free(this);
    }
}

void CPacket::SetLanHeader()
{
}

void CPacket::SetNetHeader(volatile CPacket* pPayload)
{
    unsigned char checkSum;
    unsigned char rKey;

    int iSize = pPayload->mDataSize;
    int iTotal = 0;

    // üũ�� ����
    checkSum = MakeCheckSum((BYTE*)pPayload->mData, iSize);

    // ����Ű ���� (srand �־�� �ұ� ����� �غ���..)
    //srand(time(NULL));
    rKey = rand();

    CNetServer::st_HEADER stHeader = { dfCODE, iSize, rKey, checkSum };

    PutData((char*)&stHeader, sizeof(stHeader));
    PutData((char*)pPayload->mData, iSize);
}

unsigned char CPacket::MakeCheckSum(BYTE* pStart, int iSize)
{
    int iTotal = 0;

    for (int i = 0; i < iSize; ++i)
    {
        iTotal += *pStart;
        ++pStart;
    }

    return (unsigned char)(iTotal % 256);
}

void CPacket::Encoding()
{
    // checkSum ~ payload ���� ���ڵ�
    CNetServer::st_HEADER* pHeader = (CNetServer::st_HEADER*)mData;
    unsigned char* pPos = (unsigned char*)&pHeader->checkSum;

    int iLen = pHeader->len;
    unsigned char randKey = pHeader->rKey;
    unsigned char fixKey = dfKEY;

    unsigned char randValue;
    unsigned char fixValue;

    // ���� ����Ʈ ���ڵ� -> �� ���� ����Ʈ ���ʹ� �� ����Ʈ ���� �޴� ����̾ ���� ��������� ��
    *pPos = (*pPos ^ (randKey + 1));
    randValue = *pPos;

    *pPos = *pPos ^ (fixKey + 1);
    fixValue = *pPos;

    ++pPos;

    for (int i = 1; i < iLen; ++i)
    {
        *pPos = *pPos ^ (randValue + randKey + i + 1);
        randValue = *pPos;

        *pPos = *pPos ^ (fixValue + fixKey + i + 1);
        fixValue = *pPos;

        ++pPos;
    }
}

bool CPacket::Decoding()
{
    // checkSum ~ payload ���� ���ڵ�
    CNetServer::st_HEADER* pHeader = (CNetServer::st_HEADER*)mData;
    unsigned char* pPos = (unsigned char*)&pHeader->checkSum;

    int iLen = pHeader->len;
    unsigned char randKey = pHeader->rKey;
    unsigned char fixKey = dfKEY;

    unsigned char prevRand;
    unsigned char prevFix;

    unsigned char prevRandTemp;
    unsigned char prevFixTemp;

    // ���� ����Ʈ ���ڵ� -> CheckSum Ȯ��
    prevFix = *pPos;

    *pPos = *pPos ^ (fixKey + 1);
    prevRand = *pPos;

    *pPos = (*pPos ^ (randKey + 1));

    ++pPos;

    for (int i = 1; i < iLen; ++i)
    {
        prevFixTemp = prevFix;
        prevRandTemp = prevRand;

        prevFix = *pPos;

        *pPos = *pPos ^ (prevFixTemp + fixKey + i + 1);
        prevRand = *pPos;

        *pPos = (*pPos ^ (prevRandTemp + randKey + i + 1));

        ++pPos;
    }

    pPos = ((unsigned char*)&pHeader->checkSum) + 1;    // payload �κ�

    if (pHeader->checkSum != MakeCheckSum(pPos, iLen))
        return false;
        
    return true;
}

CPacket& CPacket::operator=(const CPacket& rhs)
{
    mBufferSize = rhs.mBufferSize;
    mDataSize = rhs.mDataSize;
    mReadPos = rhs.mReadPos;
    mWritePos = rhs.mWritePos;
    mData = new char[rhs.mBufferSize];

    memcpy(&mData[mReadPos], &rhs.mData[rhs.mReadPos], rhs.mDataSize);

    return *this;
}

CPacket& CPacket::operator<<(unsigned char value)
{
    if ((mWritePos + sizeof(value)) > mBufferSize)
    {
        WriteLog(L"operator<< unsigned char value");
        return *this;
    }

    mData[mWritePos] = value;

    mDataSize += sizeof(value);
    mWritePos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator<<(char value)
{
    if ((mWritePos + sizeof(value)) > mBufferSize)
    {
        WriteLog(L"operator<< char value");
        return *this;
    }

    mData[mWritePos] = value;

    mDataSize += sizeof(value);
    mWritePos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator<<(short value)
{
    if ((mWritePos + sizeof(value)) > mBufferSize)
    {
        WriteLog(L"operator<< short value");
        return *this;
    }

    *((short*)&mData[mWritePos]) = value;

    mDataSize += sizeof(value);
    mWritePos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator<<(unsigned short value)
{
    if ((mWritePos + sizeof(value)) > mBufferSize)
    {
        WriteLog(L"operator<< unsigned short value");
        return *this;
    }

    *((unsigned short*)&mData[mWritePos]) = value;

    mDataSize += sizeof(value);
    mWritePos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator<<(int value)
{
    if ((mWritePos + sizeof(value)) > mBufferSize)
    {
        WriteLog(L"operator<< int value");
        return *this;
    }

    *((int*)&mData[mWritePos]) = value;

    mDataSize += sizeof(value);
    mWritePos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator<<(long value)
{
    if ((mWritePos + sizeof(value)) > mBufferSize)
    {
        WriteLog(L"operator<< long value");
        return *this;
    }

    *((long*)&mData[mWritePos]) = value;

    mDataSize += sizeof(value);
    mWritePos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator<<(float value)
{
    if ((mWritePos + sizeof(value)) > mBufferSize)
    {
        WriteLog(L"operator<< float value");
        return *this;
    }

    *((float*)&mData[mWritePos]) = value;

    mDataSize += sizeof(value);
    mWritePos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator<<(__int64 value)
{
    if ((mWritePos + sizeof(value)) > mBufferSize)
    {
        WriteLog(L"operator<< __int64 value");
        return *this;
    }

    *((__int64*)&mData[mWritePos]) = value;

    mDataSize += sizeof(value);
    mWritePos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator<<(double value)
{
    if ((mWritePos + sizeof(value)) > mBufferSize)
    {
        WriteLog(L"operator<< double value");
        return *this;
    }

    *((double*)&mData[mWritePos]) = value;

    mDataSize += sizeof(value);
    mWritePos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator>>(BYTE& value)
{
    if (mDataSize < sizeof(value))
    {
        WriteLog(L"operator>> BYTE value", false);
        return *this;
    }

    value = *((BYTE*)&mData[mReadPos]);

    mDataSize -= sizeof(value);
    mReadPos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator>>(char& value)
{
    if (mDataSize < sizeof(value))
    {
        WriteLog(L"operator>> char value", false);
        return *this;
    }

    value = *((char*)&mData[mReadPos]);

    mDataSize -= sizeof(value);
    mReadPos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator>>(short& value)
{
    if (mDataSize < sizeof(value))
    {
        WriteLog(L"operator>> short value", false);
        return *this;
    }

    value = *((short*)&mData[mReadPos]);

    mDataSize -= sizeof(value);
    mReadPos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator>>(WORD& value)
{
    if (mDataSize < sizeof(value))
    {
        WriteLog(L"operator>> WORD value", false);
        return *this;
    }

    value = *((WORD*)&mData[mReadPos]);

    mDataSize -= sizeof(value);
    mReadPos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator>>(int& value)
{
    if (mDataSize < sizeof(value))
    {
        WriteLog(L"operator>> int value", false);
        return *this;
    }

    value = *((int*)&mData[mReadPos]);

    mDataSize -= sizeof(value);
    mReadPos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator>>(unsigned int& value)
{
    if (mDataSize < sizeof(value))
    {
        WriteLog(L"operator>> int value", false);
        return *this;
    }

    value = *((unsigned int*)&mData[mReadPos]);

    mDataSize -= sizeof(value);
    mReadPos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator>>(DWORD& value)
{
    if (mDataSize < sizeof(value))
    {
        WriteLog(L"operator>> DWORD value", false);
        return *this;
    }

    value = *((DWORD*)&mData[mReadPos]);

    mDataSize -= sizeof(value);
    mReadPos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator>>(float& value)
{
    if (mDataSize < sizeof(value))
    {
        WriteLog(L"operator>> float value", false);
        return *this;
    }

    value = *((float*)&mData[mReadPos]);

    mDataSize -= sizeof(value);
    mReadPos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator>>(__int64& value)
{
    if (mDataSize < sizeof(value))
    {
        WriteLog(L"operator>> __int64 value", false);
        return *this;
    }

    value = *((__int64*)&mData[mReadPos]);

    mDataSize -= sizeof(value);
    mReadPos += sizeof(value);

    return *this;
}

CPacket& CPacket::operator>>(double& value)
{
    //if ((mReadPos + sizeof(value)) > mBufferSize || mDataSize < 1)
    if (mDataSize < sizeof(value))
    {
        WriteLog(L"operator>> double value", false);
        return *this;
    }

    value = *((double*)&mData[mReadPos]);

    mDataSize -= sizeof(value);
    mReadPos += sizeof(value);

    return *this;
}

int CPacket::GetData(char* dest, int cpySize)
{
    if (nullptr == dest || cpySize < 1 || mDataSize < 1)
    {
        return 0;
    }

    int get_size;

    if ((mReadPos + cpySize) > mBufferSize)
    {
        get_size = mBufferSize - mReadPos;
    }
    else
    {
        get_size = cpySize;
    }

    memcpy(dest, &mData[mReadPos], get_size);

    mReadPos += get_size;
    mDataSize -= get_size;

    return get_size;
}

int CPacket::PutData(char* src, int srcSize)
{
    if (nullptr == src || srcSize < 1 || mDataSize > mBufferSize)
    {
        return 0;
    }

    int put_size;

    if ((mWritePos + srcSize) > mBufferSize)
    {
        put_size = mBufferSize - mWritePos;
    }
    else
    {
        put_size = srcSize;
    }

    memcpy(&mData[mWritePos], src, put_size);

    mWritePos += put_size;
    mDataSize += put_size;

    return put_size;
}

void CPacket::WriteLog(const WCHAR* funcName, bool isFull)
{
    FILE* stream;

    errno_t err = _wfopen_s(&stream, L"CPacket_log.txt", L"a");

    if (err != 0 || stream == 0)
    {
        return;
    }

    //WCHAR log[128];

    if (isFull)
    {
        fwprintf_s(stream, L"buffer is full, function name : %s [%s | %s]\n", funcName, _T(__DATE__), _T(__TIME__));
    }
    else
    {
        fwprintf_s(stream, L"buffer is empty or fail to get, function name : %s [%s | %s]\n", funcName, _T(__DATE__), _T(__TIME__));
    }

    fclose(stream);
}
