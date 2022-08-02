#pragma once
#pragma comment(lib, "ws2_32")

#include <WinSock2.h>
#include <Windows.h>

#include "CLFStack.h"
#include "CLFQueue.h"
#include "CRingBuffer.h"
#include "CPacket.h"


#define dfCODE  (0x89)
#define dfKEY   (0xa9)

using namespace std;
using namespace procademy;

class CNetServer
{
    friend class CPacket;
private:
#pragma pack(push, 1)
    struct st_HEADER
    {
        unsigned char       code;
        short               len;
        unsigned char       rKey;       // ����Ű
        unsigned char       checkSum;
    };
#pragma pack(pop)

    struct st_SESSION
    {
        UINT64          dwSessionID;
        SOCKET          sock;
        OVERLAPPED      recv_overlapped;
        OVERLAPPED      send_overlapped;

        CRingBuffer     recvQ;
        /* Lock Free Queue�� ���� ���� */
        //CRingBuffer     sendQ;
        CLFQueue<CPacket*>      sendQ;
        CPacket*                deleteQ[100];

        WSABUF          recv_wsabuf[2];
        int             iSendPacketCount;

        BOOL            bCanSend;

        int             bReleaseFlag;
        int             IO_Count;
    };

    struct st_MONITORING
    {
        int iSessionCount;
        int iAcceptTotal;
        int iAcceptTPS;
        int iSendTPS;
        int iRecvTPS;
    };
public:
    CNetServer() = delete;
    CNetServer(const CNetServer& other) = delete;
    CNetServer(int iSessionMaxCount);
    
    virtual ~CNetServer();

    bool Start(const WCHAR* szIp, unsigned short usPort, int iWorkerThreadCount, bool bNagle, int iMaxUserCount);
    void Stop();

    int GetSessionCount() const;

    bool Disconnect(UINT64 dwSessionID);
    bool SendPacket(UINT64 dwSessionID, CPacket* pPacket);

    // ���� ���� �Լ�
    virtual bool OnConnectionRequest(unsigned long szIp, unsigned short usPort) = 0;        // Accept ���� �ɷ����� ����
    virtual bool OnClientJoin(UINT64 dwSessionID) = 0;                                       // Accept �� ����ó�� �Ϸ� �� ȣ�� (���ǻ���)
    virtual void OnClientLeave() = 0;                                                       // Release �� ȣ��
    virtual void OnRecv(UINT64 dwSessionID, CPacket* pPacket) = 0;                           // ��Ŷ ���� �Ϸ� ��
    virtual void OnError(int iErrorCode, WCHAR* szError) = 0;                               // ���� �ڵ鸵

    //virtual void OnSend(int iSessionID, int iSendSize);
    //virtual void OnWorkerThreadBegin() = 0;
    //virtual void OnWorkerThreadEnd() = 0;
private:
    static void __stdcall Accept(CNetServer* pThis);
    static void __stdcall Work(CNetServer* pThis);
    static void __stdcall Monitoring(CNetServer* pThis);

    void ReleaseSession(st_SESSION* pSession);

    void RecvPost(st_SESSION* pSession);
    void SendPost(st_SESSION* pSession);

    st_SESSION* FindSession(UINT64 uiSessionID);
    UINT GetIndex(UINT64 uiSessionID);
    UINT64 CombineIndexID(UINT uiIndex, UINT uiID);

    SOCKET                              m_ListenSocket;

    HANDLE                              m_hIOCP;
    HANDLE                              m_hAcceptThread;
    HANDLE                              m_hMonitoringThread;
    HANDLE*                             m_hWorkerThreads;
    
    int                                 m_iWorkerThreadCount;
    st_MONITORING                       m_stMonitoring;
    st_SESSION*                         m_ArrSession;
    CLFStack<UINT>                      m_IndexStack;
};
