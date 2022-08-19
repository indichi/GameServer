#pragma once
#pragma comment(lib, "ws2_32")

#include <WinSock2.h>
#include <Windows.h>

#include "CLFStack.h"
#include "CLFQueue.h"
#include "CRingBuffer.h"
#include "CPacket.h"

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
        UINT64                  dwSessionID;

        SOCKET                  sock;
        OVERLAPPED              recv_overlapped;
        OVERLAPPED              send_overlapped;

        CRingBuffer             recvQ;
        CLFQueue<CPacket*>      sendQ;
        CPacket*                deleteQ[100];

        WSABUF                  recv_wsabuf[2];
        int                     iSendPacketCount;

        int                     bCanSend;

        int                     bReleaseFlag;
        int                     IO_Count;
    };

    struct st_MONITORING
    {
        int iSessionCount;
        int iPacketPoolAllocSize;

        int iAcceptTotal;
        int iAcceptTPS;
        int iUpdateTPS;

        int iRecvTPS;
        int iSendTPS;
    };

public:
    CNetServer() = delete;
    CNetServer(const CNetServer& other) = delete;
    CNetServer(int iSessionMaxCount);
    
    virtual ~CNetServer();

    virtual bool Start(const WCHAR* szIp, unsigned short usPort, int iWorkerThreadCount, int iRunningThreadCount, bool bNagle, int iMaxUserCount, unsigned char uchPacketCode, unsigned char uchPacketKey, int iTimeout);
    void Stop();

    int GetSessionCount() const;

    bool Disconnect(DWORD64 dwSessionID);
    bool SendPacket_Unicast(DWORD64 dwSessionID, CPacket* pPacket);
    bool SendPacket_Multicast(vector<DWORD64>* pVecSessionID, DWORD64 dwExceptSessionID, CPacket* pPacket);

    // ���� ���� �Լ�
    virtual bool OnConnectionRequest(unsigned long szIp, unsigned short usPort) = 0;        // Accept ���� �ɷ����� ����
    virtual bool OnClientJoin(DWORD64 dwSessionID) = 0;                                     // Accept �� ����ó�� �Ϸ� �� ȣ�� (���ǻ���)
    virtual void OnClientLeave(DWORD64 dwSessionID) = 0;                                    // Release �� ȣ��
    virtual void OnRecv(DWORD64 dwSessionID, CPacket* pPacket) = 0;                         // ��Ŷ ���� �Ϸ� ��
    virtual void OnError(DWORD64 dwSessionID, int iErrorCode, WCHAR* szError) = 0;          // ���� �ڵ鸵
    virtual void OnTimer() = 0;                                                             // Ÿ�Ӿƿ� ��
    virtual void OnMonitoring() = 0;                                                        // ����͸� ��
protected:
    enum class eFuncHandler
    {
        OnConnectionRequest     = 0,
        OnClientJoin,
        OnClientLeave,
        OnRecv,
        OnError,
        OnTimer
    };
private:
    static void __stdcall Accept(CNetServer* pThis);
    static void __stdcall Work(CNetServer* pThis);
    static void __stdcall Monitoring(CNetServer* pThis);
    static void __stdcall Timer(CNetServer* pThis);

    void ReleaseSession(st_SESSION* pSession);

    void RecvPost(st_SESSION* pSession);
    void SendPost(st_SESSION* pSession);

    st_SESSION* FindSession(UINT64 uiSessionID);
    UINT GetIndex(UINT64 uiSessionID);
    UINT64 CombineIndexID(UINT uiIndex, UINT uiID);

private:
    st_SESSION*                         m_ArrSession;
    CLFStack<UINT>*                     m_IndexStack;

    SOCKET                              m_ListenSocket;

    HANDLE                              m_hIOCP;
    HANDLE                              m_hAcceptThread;
    HANDLE                              m_hMonitoringThread;
    HANDLE                              m_hTimeThread;
    HANDLE*                             m_hWorkerThreads;
    
    int                                 m_iWorkerThreadCount;
    int                                 m_iSessionMaxCount;
    int                                 m_iTimeout;

protected:
    alignas(64)
    st_MONITORING                       m_stMonitoring;
};
