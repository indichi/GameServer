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
        unsigned char       rKey;       // 랜덤키
        unsigned char       checkSum;
    };
#pragma pack(pop)

    struct st_SESSION
    {
        DWORD64                 dwSessionID     = -1;

        SOCKET                  sock;
        OVERLAPPED              recv_overlapped;
        OVERLAPPED              send_overlapped;

        CRingBuffer             recvQ;
        CLFQueue<CPacket*>      sendQ;
        CPacket*                deleteQ[100];

        WSABUF                  recv_wsabuf[2];
        int                     iSendPacketCount;

        /*alignas(64) DWORD       bReleaseFlag    = FALSE;
        DWORD                   IO_Count        = 0;*/

        alignas(16) DWORD64     bReleaseFlag    = FALSE;
        DWORD64                 IO_Count        = 0;

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

public:
    CNetServer(st_SERVER_INFO* stServerInfo);
    CNetServer(const CNetServer& other) = delete;
    
    virtual ~CNetServer();

    virtual bool Start();

    void Stop();

    bool Disconnect(DWORD64 dwSessionID);
    bool SendPacket_Unicast(DWORD64 dwSessionID, CPacket* pPacket);
    bool SendPacket_Multicast(vector<DWORD64>* pVecSessionID, DWORD64 dwExceptSessionID, CPacket* pPacket);

    // 순수 가상 함수
    virtual bool OnConnectionRequest(unsigned long szIp, unsigned short usPort) = 0;        // Accept 직후 걸러내기 위함
    virtual bool OnClientJoin(DWORD64 dwSessionID) = 0;                                     // Accept 후 접속처리 완료 후 호출 (세션생성)
    virtual void OnClientLeave(DWORD64 dwSessionID) = 0;                                    // Release 후 호출
    virtual void OnRecv(DWORD64 dwSessionID, CPacket* pPacket) = 0;                         // 패킷 수신 완료 후
    virtual void OnError(DWORD64 dwSessionID, int iErrorCode, WCHAR* szError) = 0;          // 에러 핸들링
    virtual void OnTimer() = 0;                                                             // 타임아웃 용
    virtual void OnMonitoring(st_LIB_MONITORING* pLibMonitoring) = 0;                       // 모니터링 용

private:
    static void __stdcall Accept(CNetServer* pThis);
    static void __stdcall Work(CNetServer* pThis);
    static void __stdcall Monitoring(CNetServer* pThis);
    static void __stdcall Timer(CNetServer* pThis);

    void ReleaseSession(st_SESSION* pSession);

    void RecvPost(st_SESSION* pSession);
    void SendPost(st_SESSION* pSession);

private:
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
};
