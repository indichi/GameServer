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
    // ������, �ı���.
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
        eBUFFER_DEFAULT = 1400		// ��Ŷ�� �⺻ ���� ������.
    };

    //////////////////////////////////////////////////////////////////////////
    // ��Ŷ  �ı�.
    //
    // Parameters: ����.
    // Return: ����.
    //////////////////////////////////////////////////////////////////////////
    void	Release(void);


    //////////////////////////////////////////////////////////////////////////
    // ��Ŷ û��.
    //
    // Parameters: ����.
    // Return: ����.
    //////////////////////////////////////////////////////////////////////////
    void	Clear(void);


    //////////////////////////////////////////////////////////////////////////
    // ���� ������ ���.
    //
    // Parameters: ����.
    // Return: (int)��Ŷ ���� ������ ���.
    //////////////////////////////////////////////////////////////////////////
    int	GetBufferSize(void) { return mBufferSize; }
    //////////////////////////////////////////////////////////////////////////
    // ���� ������� ������ ���.
    //
    // Parameters: ����.
    // Return: (int)������� ����Ÿ ������.
    //////////////////////////////////////////////////////////////////////////
    int		GetDataSize(void) { return mDataSize; }



    //////////////////////////////////////////////////////////////////////////
    // ���� ������ ���.
    //
    // Parameters: ����.
    // Return: (char *)���� ������.
    //////////////////////////////////////////////////////////////////////////
    char* GetBufferPtr(void) { return mData; }

    //////////////////////////////////////////////////////////////////////////
    // �б� ������ ���.
    //
    // Parameters: ����.
    // Return: (char *)�б� ������.
    //////////////////////////////////////////////////////////////////////////
    char* GetReadBufferPtr(void) { return mData + mReadPos; }

    //////////////////////////////////////////////////////////////////////////
    // ���� ������ ���.
    //
    // Parameters: ����.
    // Return: (char *)�б� ������.
    //////////////////////////////////////////////////////////////////////////
    char* GetWriteBufferPtr(void) { return mData + mWritePos; }

    //////////////////////////////////////////////////////////////////////////
    // ���� Pos �̵�. (�����̵��� �ȵ�)
    // GetBufferPtr �Լ��� �̿��Ͽ� �ܺο��� ������ ���� ������ ������ ��� ���. 
    //
    // Parameters: (int) �̵� ������.
    // Return: (int) �̵��� ������.
    //////////////////////////////////////////////////////////////////////////
    int		MoveWritePos(int size);
    int		MoveReadPos(int size);


    /* ============================================================================= */
    // ���� ī��Ʈ ���� �Լ�
    /* ============================================================================= */
    void AddRefCount();
    void SubRefCount();

    /* ============================================================================= */
    // ��� ���� ���� �Լ� (LanServer, NetServer ��)
    /* ============================================================================= */
    void SetLanHeader();
    void SetNetPacket(volatile CPacket* pPayload);  // header ���� + payload ���̱�

    unsigned char MakeCheckSum(BYTE* pStart, int iSize);

    /* ============================================================================= */
    // ���ڵ�, ���ڵ� ���� �Լ� (NetServer ��)
    /* ============================================================================= */
    void Encoding();
    bool Decoding();    // return : üũ�� �Ǵ� �� ���ڵ� ���� ����

    /* ============================================================================= */
    // ������ �����ε�
    /* ============================================================================= */
    CPacket& operator = (const CPacket& rhs);

    //////////////////////////////////////////////////////////////////////////
    // �ֱ�.	�� ���� Ÿ�Ը��� ��� ����.
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
    // ����.	�� ���� Ÿ�Ը��� ��� ����.
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
    // ����Ÿ ���.
    //
    // Parameters: (char *)Dest ������. (int)Size.
    // Return: (int)������ ������.
    //////////////////////////////////////////////////////////////////////////
    int		GetData(char* dest, int cpySize);

    //////////////////////////////////////////////////////////////////////////
    // ����Ÿ ����.
    //
    // Parameters: (char *)Src ������. (int)SrcSize.
    // Return: (int)������ ������.
    //////////////////////////////////////////////////////////////////////////
    int		PutData(char* src, int srcSize);


    void WriteLog(const WCHAR* funcName, bool isFull = true);

protected:

    int	mBufferSize;

    //------------------------------------------------------------
    // ���� ���ۿ� ������� ������.
    //------------------------------------------------------------
    int	mDataSize;

    //------------------------------------------------------------
    // �б�, ���� ��ġ
    //------------------------------------------------------------
    int mReadPos;
    int mWritePos;

    //------------------------------------------------------------
    // ������ ������
    //------------------------------------------------------------
    char* mData;

    unsigned int    mRefCount;
};

#endif