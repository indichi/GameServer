#pragma once

#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")
#pragma comment (lib, "ws2_32.lib")
#pragma comment(lib, "ws2_32")

#include <cpp_redis/cpp_redis>
#include <WinSock2.h>
#include <Windows.h>


#include "../00. Component/CLFStack.h"
#include "../00. Component/CLFQueue.h"
#include "../00. Component/CRingBuffer.h"
#include "../00. Component/CPacket.h"
#include "../00. Component/CMemoryPoolTLS.h"

#define dfMAX_SEND_COUNT  (100)

using namespace std;
using namespace procademy;

class CNetServer
{
    friend class CPacket;
private:

#pragma pack(push, 1)
    struct st_HEADER
    {
        unsigned char       code;           // Packet Code
        unsigned short      len;            // Packet length
        unsigned char       rKey;           // Random Key (Encoding, Decoding)
        unsigned char       checkSum;       // Packet Checksum
    };
#pragma pack(pop)

    struct st_SESSION
    {
        DWORD64                 dwSessionID;

        SOCKET                  sock;
        OVERLAPPED              recv_overlapped;
        OVERLAPPED              send_overlapped;

        CRingBuffer             recvQ;
        CLFQueue<CPacket*>      sendQ;
        CPacket*                deleteQ[dfMAX_SEND_COUNT];

        WSABUF                  recv_wsabuf[2];
        int                     iSendPacketCount;

        DWORD64                 dwHeartbeat;
        DWORD                   IO_Count        = 0x80000000;

        alignas(64) DWORD       bCanSend        = FALSE;
    };

protected:

    struct st_LIB_MONITORING
    {
        int iSessionCount = 0;
        int iAcceptTotal = 0;
        int iAcceptTPS = 0;
        int iRecvTPS = 0;
        int iSendTPS = 0;
    };

    enum class eFuncHandler
    {
        OnConnectionRequest = 0,
        OnClientJoin,
        OnClientLeave,
        OnRecv,
        OnError,
        OnTimer
    };

public:
    struct st_SERVER_INFO
    {
        WCHAR               szIp[20];
        unsigned short      usPort;
        int                 iWorkerThreadCount;
        int                 iRunningThreadCount;
        bool                bNagle;
        int                 iMaxSessionCount;
        unsigned char       uchPacketCode;
        unsigned char       uchPacketKey;
        int                 iTimeout;
        int                 iContentsThreadCount;
    };

    struct st_DB_INFO
    {
        WCHAR		szDBIP[16];
        WCHAR		szDBUser[64];
        WCHAR		szDBPassword[64];
        WCHAR		szDBName[64];
        int			iDBPort;
    };

public:
    CNetServer(st_SERVER_INFO* stServerInfo, st_DB_INFO* stDBInfo);
    CNetServer(const CNetServer& other) = delete;
    
    virtual ~CNetServer();

    virtual bool Start();                                                                                   // ���� ���� (�ʿ��� ������ ��ü �����ڿ��� �޾Ƽ� ����� ����)

    bool Disconnect(DWORD64 dwSessionID);                                                                   // Session ���� (Player out)
    bool SendPacket_Unicast(DWORD64 dwSessionID, CPacket* pPacket);                                         // ���� ���� Packet �۽�
    bool SendPacket_Multicast(vector<DWORD64>* pVecSessionID, DWORD64 dwExceptSessionID, CPacket* pPacket); // ���� ���� Packet �۽�

    // ���� ���� �Լ�
    virtual bool OnConnectionRequest(unsigned long szIp, unsigned short usPort)     = 0;                    // Accept ���� �ɷ����� ����
    virtual bool OnClientJoin(DWORD64 dwSessionID)                                  = 0;                    // Accept �� ����ó�� �Ϸ� �� ȣ�� (���� ����)
    virtual void OnClientLeave(DWORD64 dwSessionID)                                 = 0;                    // Release �� ȣ��
    virtual void OnRecv(DWORD64 dwSessionID, CPacket* pPacket)                      = 0;                    // ��Ŷ ���� �Ϸ� ��
    virtual void OnError(DWORD64 dwSessionID, int iErrorCode, WCHAR* szError)       = 0;                    // ���� �ڵ鸵
    virtual void OnTimer()                                                          = 0;                    // Ÿ�Ӿƿ� ��
    virtual void OnMonitoring(st_LIB_MONITORING* pLibMonitoring)                    = 0;                    // ����͸� ��

private:

    static void __stdcall Accept(CNetServer* pThis);                                                        // Accept Thread
    static void __stdcall Work(CNetServer* pThis);                                                          // IOCP�� ��ϵǴ� I/O Worker Threads
    static void __stdcall Monitoring(CNetServer* pThis);                                                    // Monitoring Thread
    static void __stdcall Timer(CNetServer* pThis);                                                         // Timer Thread

private:
    void ReleaseSession(st_SESSION* pSession);
    void RecvPost(st_SESSION* pSession);
    void SendPost(st_SESSION* pSession);

    DWORD64 CombineIndexID(DWORD64 dwIndex, DWORD64 dwID);
    st_SESSION* GetCheckedSession(DWORD64 dwSessionID);

private:
    st_SESSION*                         m_ArrSession;
    CLFStack<DWORD64>*                  m_IndexStack;
    
    SOCKET                              m_ListenSocket;

    HANDLE                              m_hIOCP;
    HANDLE                              m_hAcceptThread;
    HANDLE                              m_hMonitoringThread;
    HANDLE                              m_hTimeThread;
    HANDLE*                             m_hWorkerThreads;

    alignas(64) st_LIB_MONITORING       m_stMonitoring;
    alignas(64) st_SERVER_INFO          m_stServerInfo;

    st_DB_INFO                          m_stDBInfo;
protected:
    DWORD                               m_dwDBconnTlsIndex;
};
