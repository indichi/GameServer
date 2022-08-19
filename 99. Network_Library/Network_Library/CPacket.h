#ifndef  __PACKET__
#define  __PACKET__

//#include "CLanServer.h"
//#include "CNetServer.h"

#include <Windows.h>

#include "CChunk.h"
#include "CMemoryPoolTLS.h"

class CPacket
{
    friend class CChunk<CPacket>;
private:
    //////////////////////////////////////////////////////////////////////////
    // 생성자, 파괴자.
    //
    // Return:
    //////////////////////////////////////////////////////////////////////////
    CPacket();
    CPacket(int size);
    CPacket(const CPacket& other);

    virtual	~CPacket();

    inline static CMemoryPoolTLS<CPacket> s_PacketPoolTLS;
public:
    static CPacket* Alloc();
    static bool Free(volatile CPacket* pPacket);

    static int GetPacketPoolAllocSize() { return s_PacketPoolTLS.GetAllocChunkSize() * dfCHUNK_NODE_COUNT; }

    static inline unsigned char s_PacketCode = 0;
    static inline unsigned char s_PacketKey = 0;

    /*---------------------------------------------------------------
    Packet Enum.

    ----------------------------------------------------------------*/
    enum en_PACKET
    {
        eBUFFER_DEFAULT = 1400		// 패킷의 기본 버퍼 사이즈.
    };

    //////////////////////////////////////////////////////////////////////////
    // 패킷  파괴.
    //
    // Parameters: 없음.
    // Return: 없음.
    //////////////////////////////////////////////////////////////////////////
    void	Release(void);


    //////////////////////////////////////////////////////////////////////////
    // 패킷 청소.
    //
    // Parameters: 없음.
    // Return: 없음.
    //////////////////////////////////////////////////////////////////////////
    void	Clear(void);


    //////////////////////////////////////////////////////////////////////////
    // 버퍼 사이즈 얻기.
    //
    // Parameters: 없음.
    // Return: (int)패킷 버퍼 사이즈 얻기.
    //////////////////////////////////////////////////////////////////////////
    int	GetBufferSize(void) { return mBufferSize; }
    //////////////////////////////////////////////////////////////////////////
    // 현재 사용중인 사이즈 얻기.
    //
    // Parameters: 없음.
    // Return: (int)사용중인 데이타 사이즈.
    //////////////////////////////////////////////////////////////////////////
    int		GetDataSize(void) { return mDataSize; }



    //////////////////////////////////////////////////////////////////////////
    // 버퍼 포인터 얻기.
    //
    // Parameters: 없음.
    // Return: (char *)버퍼 포인터.
    //////////////////////////////////////////////////////////////////////////
    char* GetBufferPtr(void) { return mData; }

    //////////////////////////////////////////////////////////////////////////
    // 읽기 포인터 얻기.
    //
    // Parameters: 없음.
    // Return: (char *)읽기 포인터.
    //////////////////////////////////////////////////////////////////////////
    char* GetReadBufferPtr(void) { return mData + mReadPos; }

    //////////////////////////////////////////////////////////////////////////
    // 쓰기 포인터 얻기.
    //
    // Parameters: 없음.
    // Return: (char *)읽기 포인터.
    //////////////////////////////////////////////////////////////////////////
    char* GetWriteBufferPtr(void) { return mData + mWritePos; }

    //////////////////////////////////////////////////////////////////////////
    // 버퍼 Pos 이동. (음수이동은 안됨)
    // GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
    //
    // Parameters: (int) 이동 사이즈.
    // Return: (int) 이동된 사이즈.
    //////////////////////////////////////////////////////////////////////////
    int		MoveWritePos(int size);
    int		MoveReadPos(int size);


    /* ============================================================================= */
    // 참조 카운트 관련 함수
    /* ============================================================================= */
    void AddRefCount();
    void SubRefCount();

    /* ============================================================================= */
    // 헤더 세팅 관련 함수 (LanServer, NetServer 용)
    /* ============================================================================= */
    void SetLanHeader();
    void SetNetPacket(volatile CPacket* pPayload);  // header 세팅 + payload 붙이기

    unsigned char MakeCheckSum(BYTE* pStart, int iSize);

    /* ============================================================================= */
    // 인코딩, 디코딩 관련 함수 (NetServer 용)
    /* ============================================================================= */
    void Encoding();
    bool Decoding();    // return : 체크섬 판단 후 디코딩 성공 여부

    /* ============================================================================= */
    // 연산자 오버로딩
    /* ============================================================================= */
    CPacket& operator = (const CPacket& rhs);

    //////////////////////////////////////////////////////////////////////////
    // 넣기.	각 변수 타입마다 모두 만듬.
    //////////////////////////////////////////////////////////////////////////
    CPacket& operator << (unsigned char value);
    CPacket& operator << (char value);

    CPacket& operator << (short value);
    CPacket& operator << (unsigned short value);

    CPacket& operator << (int value);
    CPacket& operator << (long value);
    CPacket& operator << (float value);

    CPacket& operator << (__int64 value);
    CPacket& operator << (double value);


    //////////////////////////////////////////////////////////////////////////
    // 빼기.	각 변수 타입마다 모두 만듬.
    //////////////////////////////////////////////////////////////////////////
    CPacket& operator >> (BYTE& value);
    CPacket& operator >> (char& value);

    CPacket& operator >> (short& value);
    CPacket& operator >> (WORD& value);

    CPacket& operator >> (int& value);
    CPacket& operator >> (unsigned int& value);
    CPacket& operator >> (DWORD& value);
    CPacket& operator >> (float& value);

    CPacket& operator >> (__int64& value);
    CPacket& operator >> (double& value);




    //////////////////////////////////////////////////////////////////////////
    // 데이타 얻기.
    //
    // Parameters: (char *)Dest 포인터. (int)Size.
    // Return: (int)복사한 사이즈.
    //////////////////////////////////////////////////////////////////////////
    int		GetData(char* dest, int cpySize);

    //////////////////////////////////////////////////////////////////////////
    // 데이타 삽입.
    //
    // Parameters: (char *)Src 포인터. (int)SrcSize.
    // Return: (int)복사한 사이즈.
    //////////////////////////////////////////////////////////////////////////
    int		PutData(char* src, int srcSize);


    void WriteLog(const WCHAR* funcName, bool isFull = true);

protected:

    int	mBufferSize;

    //------------------------------------------------------------
    // 현재 버퍼에 사용중인 사이즈.
    //------------------------------------------------------------
    int	mDataSize;

    //------------------------------------------------------------
    // 읽기, 쓰기 위치
    //------------------------------------------------------------
    int mReadPos;
    int mWritePos;

    //------------------------------------------------------------
    // 데이터 포인터
    //------------------------------------------------------------
    char* mData;

    unsigned int    mRefCount;
};

#endif