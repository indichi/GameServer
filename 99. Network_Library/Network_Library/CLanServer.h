#pragma once
#pragma comment(lib, "ws2_32")

#include <WinSock2.h>
#include <Windows.h>

#include "CLFStack.h"
#include "CLFQueue.h"
#include "CRingBuffer.h"
#include "CPacket.h"


#define USER_MAX    (10000)

using namespace std;
using namespace procademy;

class CLanServer
{
private:
#pragma pack(push, 1)
    struct st_HEADER
    {
        short   len;
    };
#pragma pack(pop)

    struct st_SESSION
    {
        UINT64          dwSessionID;
        SOCKET          sock;
        OVERLAPPED      recv_overlapped;
        OVERLAPPED      send_overlapped;

        CRingBuffer     recvQ;
        /* Lock Free Queue로 변경 예정 */
        //CRingBuffer     sendQ;
        CLFQueue<CPacket*>      sendQ;
        CPacket*                deleteQ[100];

        WSABUF          recv_wsabuf[2];
        int             iSendPacketCount;

        SRWLOCK         locker;
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
    CLanServer() = delete;
    CLanServer(const CLanServer& other) = delete;
    CLanServer(int iSessionMaxCount);
    
    virtual ~CLanServer();

    bool Start(const WCHAR* szIp, unsigned short usPort, int iWorkerThreadCount, bool bNagle, int iMaxUserCount);
    void Stop();

    int GetSessionCount() const;

    bool Disconnect(UINT64 dwSessionID);
    bool SendPacket(UINT64 dwSessionID, CPacket* pPacket);

    // 순수 가상 함수
    virtual bool OnConnectionRequest(unsigned long szIp, unsigned short usPort) = 0;        // Accept 직후 걸러내기 위함
    virtual bool OnClientJoin(UINT64 dwSessionID) = 0;                                       // Accept 후 접속처리 완료 후 호출 (세션생성)
    virtual void OnClientLeave() = 0;                                                       // Release 후 호출
    virtual void OnRecv(UINT64 dwSessionID, CPacket* pPacket) = 0;                           // 패킷 수신 완료 후
    virtual void OnError(int iErrorCode, WCHAR* szError) = 0;                               // 에러 핸들링

    //virtual void OnSend(int iSessionID, int iSendSize);
    //virtual void OnWorkerThreadBegin() = 0;
    //virtual void OnWorkerThreadEnd() = 0;
private:
    static void __stdcall Accept(CLanServer* pThis);
    static void __stdcall Work(CLanServer* pThis);
    static void __stdcall Monitoring(CLanServer* pThis);

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
    //stack<UINT>                         m_IndexStack;
    //SRWLOCK                             m_IndexStackLocker;
};
