#pragma comment(lib, "Winmm.lib")
#include <thread>
#include <conio.h>

#include "CNetServer.h"
#include "CPacket.h"
#include "CMemoryPoolTLS.h"

CNetServer::CNetServer(int iSessionMaxCount)
    : m_ListenSocket(INVALID_SOCKET)
    , m_ArrSession(new st_SESSION[iSessionMaxCount])
    , m_hIOCP(INVALID_HANDLE_VALUE)
    , m_hAcceptThread(INVALID_HANDLE_VALUE)
    , m_hMonitoringThread(INVALID_HANDLE_VALUE)
    , m_hTimeThread(INVALID_HANDLE_VALUE)
    , m_hWorkerThreads(nullptr)
    , m_iWorkerThreadCount(0)
    , m_stMonitoring{ 0, 0, 0, 0, 0 }
    , m_IndexStack(new CLFStack<UINT>)
    , m_iSessionMaxCount(iSessionMaxCount)
    , m_iTimeout(40000)
{
    // index stack 세팅
    for (int i = iSessionMaxCount - 1; i >= 0; --i)
    {
        m_IndexStack->Push(i);
    }
}

CNetServer::~CNetServer()
{
    CloseHandle(m_hIOCP);
    CloseHandle(m_hAcceptThread);
    CloseHandle(m_hMonitoringThread);
    CloseHandle(m_hTimeThread);

    for (int i = 0; i < m_iWorkerThreadCount; ++i)
    {
        CloseHandle(m_hWorkerThreads[i]);
    }

    delete[] m_hWorkerThreads;
    delete m_IndexStack;

    WSACleanup();

    timeEndPeriod(1);
}

bool CNetServer::Start(const WCHAR* szIp, unsigned short usPort, int iWorkerThreadCount, int iRunningThreadCount, bool bNagle, int iMaxUserCount, unsigned char uchPacketCode, unsigned char uchPacketKey, int iTimeout)
{
    CPacket::s_PacketCode = uchPacketCode;
    CPacket::s_PacketKey = uchPacketKey;

    m_iTimeout = iTimeout;

    timeBeginPeriod(1);

    int iRet;
    //int iError;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, iRunningThreadCount);
    if (m_hIOCP == NULL)
        return 1;

    m_ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_ListenSocket == INVALID_SOCKET)
        return 1;

    SOCKADDR_IN server;
    ZeroMemory(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    server.sin_port = htons(usPort);
    
    // linger 설정
    LINGER stLinger;

    stLinger.l_onoff = 1;
    stLinger.l_linger = 0;

    iRet = setsockopt(m_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));
    if (iRet == SOCKET_ERROR)
        return 1;

    if (bNagle)
    {
        int iOpt = 1;
        iRet = setsockopt(m_ListenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&iOpt, sizeof(iOpt));
        if (iRet == SOCKET_ERROR)
            return 1;
    }

    iRet = bind(m_ListenSocket, (SOCKADDR*)&server, sizeof(server));

    if (iRet == SOCKET_ERROR)
        return 1;

    iRet = listen(m_ListenSocket, SOMAXCONN);
    if (iRet == SOCKET_ERROR)
        return 1;

    // 작업자 스레드 생성
    m_hWorkerThreads = new HANDLE[iWorkerThreadCount];
    m_iWorkerThreadCount = iWorkerThreadCount;

    for (int i = 0; i < iWorkerThreadCount; i++) {
        m_hWorkerThreads[i] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)CNetServer::Work, this, 0, nullptr);
        if (m_hWorkerThreads[i] == NULL)
            return 1;
    }

    // Accept 쓰레드 생성
    m_hAcceptThread = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)CNetServer::Accept, this, 0, nullptr);
    if (m_hAcceptThread == NULL)
        return 1;

    // 모니터링 쓰레드 생성
    m_hMonitoringThread = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)CNetServer::Monitoring, this, 0, nullptr);
    if (m_hMonitoringThread == NULL)
        return 1;

    // 타이머(타임아웃용) 쓰레드 생성
    m_hTimeThread = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)CNetServer::Timer, this, 0, nullptr);
    if (m_hTimeThread == NULL)
        return 1;

    /*WaitForMultipleObjects(iWorkerThreadCount, m_hWorkerThreads, TRUE, INFINITE);
    WaitForSingleObject(m_hAcceptThread, INFINITE);
    WaitForSingleObject(m_hMonitoringThread, INFINITE);*/

    return true;
}

void CNetServer::Stop()
{
}

int CNetServer::GetSessionCount() const
{
    // return (int)m_SessionMap.size();
}

bool CNetServer::Disconnect(DWORD64 dwSessionID)
{
    st_SESSION* pSession = FindSession(dwSessionID);
    if (pSession == nullptr)
        return false;

    if (pSession->dwSessionID != dwSessionID)
        return false;

    // IO COUNT 증가(ref cnt)
    InterlockedIncrement((long*)&pSession->IO_Count);

    // ReleaseFlag 확인해서 TRUE면 Release 함수에서 이미 통과된 것 -> IO COUNT 줄여주고 나가기..
    if (pSession->bReleaseFlag == TRUE)
    {
        //InterlockedDecrement((long*)&pSession->IO_Count);
        return false;
    }
    
    if (CancelIoEx((HANDLE)pSession->sock, NULL) == FALSE)
    {

    }

    // Disconnect 완료되면 IO COUNT 줄여주기
    int iCount = InterlockedDecrement((long*)&pSession->IO_Count);
    //if (iCount == 0)
    //    ReleaseSession(pSession);
    
    return true;
}

void CNetServer::ReleaseSession(st_SESSION* pSession)
{
    // IO_COUNT 세션 사용여부 개념(ref cnt)로 사용
    if (InterlockedCompareExchange64((long long*)&pSession->bReleaseFlag, TRUE, FALSE) != FALSE)
        return;

    UINT index = GetIndex(pSession->dwSessionID);
    DWORD64 dwSessionID = pSession->dwSessionID;

    pSession->dwSessionID = 0;
    pSession->iSendPacketCount = 0;
    pSession->recvQ.ClearBuffer();

    closesocket(pSession->sock);

    //pSession->sock = INVALID_SOCKET;

    InterlockedDecrement((long*)&m_stMonitoring.iSessionCount);
    
    m_IndexStack->Push(index);

    OnClientLeave(dwSessionID);
}

void CNetServer::RecvPost(st_SESSION* pSession)
{
    int iRet;
    int iError;

    int iDirectSize = pSession->recvQ.DirectEnqueueSize();
    int iFreeSize = pSession->recvQ.GetFreeSize();
    int iBufCount;

    DWORD dwFlag = 0;

    ZeroMemory(&pSession->recv_overlapped, sizeof(pSession->recv_overlapped));
    if (iDirectSize < iFreeSize)
    {
        iBufCount = 2;
        pSession->recv_wsabuf[0].buf = pSession->recvQ.GetRearBufferPtr();
        pSession->recv_wsabuf[0].len = iDirectSize;

        pSession->recv_wsabuf[1].buf = pSession->recvQ.GetBufferStartPtr();
        pSession->recv_wsabuf[1].len = iFreeSize - iDirectSize;
    }
    else
    {
        iBufCount = 1;
        pSession->recv_wsabuf[0].buf = pSession->recvQ.GetRearBufferPtr();
        pSession->recv_wsabuf[0].len = iDirectSize;
    }

    //InterlockedIncrement((unsigned int*)&pSession->IO_Count);
    iRet = WSARecv(pSession->sock, pSession->recv_wsabuf, iBufCount, NULL, &dwFlag, &pSession->recv_overlapped, NULL);
    if (iRet == SOCKET_ERROR)
    {
        iError = WSAGetLastError();

        if (iError != ERROR_IO_PENDING)
        {
            if (iError != 10054)
            {
                //printf("# WSARecv (WorkerThread) > Error code:%d\n", iError);
            }

            int iCount = InterlockedDecrement((unsigned int*)&pSession->IO_Count);
            if (iCount == 0)
                ReleaseSession(pSession);
        }
    }
}

void CNetServer::SendPost(st_SESSION* pSession)
{
    if (pSession->sendQ.GetSize() > 0 && InterlockedExchange((unsigned int*)&pSession->bCanSend, FALSE) == TRUE)
    {
        int iRet;
        int iError;

        int iUseSize = pSession->sendQ.GetSize();
        if (iUseSize == 0)
        {
            InterlockedExchange((unsigned int*)&pSession->bCanSend, TRUE);
            return;
        }

        int iPacketCount = 0;
        if (iUseSize > 100)
            iPacketCount = 100;
        else
            iPacketCount = iUseSize;

        WSABUF      wsabuf[100];
        CPacket*    pPacket;

        pSession->iSendPacketCount = iPacketCount;

        for (int i = 0; i < iPacketCount; ++i)
        {
            pSession->sendQ.Dequeue(&pPacket);
            pSession->deleteQ[i] = pPacket;

            wsabuf[i].buf = pPacket->GetReadBufferPtr();
            wsabuf[i].len = pPacket->GetDataSize();
        }

        ZeroMemory(&pSession->send_overlapped, sizeof(pSession->send_overlapped));

        InterlockedIncrement((unsigned int*)&pSession->IO_Count);
        iRet = WSASend(pSession->sock, wsabuf, iPacketCount, NULL, 0, &pSession->send_overlapped, NULL);

        if (iRet == SOCKET_ERROR)
        {
            iError = WSAGetLastError();

            if (iError != ERROR_IO_PENDING)
            {
                if (iError != 10054)
                {
                    //printf("# WSASend > Error code:%d\n", iError);
                }

                // CPacket 참조 카운트 감소
                for (int i = 0; i < iPacketCount; ++i)
                {
                    CPacket::Free(pSession->deleteQ[i]);
                }

                pSession->iSendPacketCount = 0;

                int iCount = InterlockedDecrement((unsigned int*)&pSession->IO_Count);

                if (iCount == 0)
                    ReleaseSession(pSession);
                else
                    pSession->bCanSend = TRUE;
            }
        }
    }
}

UINT CNetServer::GetIndex(UINT64 uiSessionID)
{
    return (uiSessionID & 0xffff000000000000) >> 48;
}

CNetServer::st_SESSION* CNetServer::FindSession(UINT64 uiSessionID)
{
    UINT index = GetIndex(uiSessionID);
    
    if (index >= m_iSessionMaxCount)
        return nullptr;
    else
        return &m_ArrSession[index];
}

UINT64 CNetServer::CombineIndexID(UINT uiIndex, UINT uiID)
{
    UINT64 ret = uiID;
    UINT64 index = uiIndex;

    return ret | (index << 48);
}

bool CNetServer::SendPacket_Unicast(DWORD64 dwSessionID, CPacket* pPacket)
{
    st_SESSION* pSession = FindSession(dwSessionID);
    if (pSession == nullptr)
        return false;

    // 세션 재활용 됐을 때 release flag 보다 send가 먼저 타진다면 바뀐 세션으로 sendpost 하게 됨
    if (pSession->dwSessionID != 0 && pSession->dwSessionID != dwSessionID)
        return false;

    // IO COUNT 증가(ref cnt)
    InterlockedIncrement((long*)&pSession->IO_Count);

    // ReleaseFlag 확인해서 TRUE면 Release 함수에서 이미 통과된 것 -> IO COUNT 줄여주고 나가기..
    if (pSession->bReleaseFlag == TRUE)
    {
        return false;
    }

    // CPacket 메모리 풀에서 alloc, sendQ에 Enqueue 전에 참조 카운트 증가
    CPacket* newPacket = CPacket::Alloc();

    newPacket->SetNetPacket(pPacket);   // 헤더 세팅 + payload붙이기
    newPacket->Encoding();

    newPacket->AddRefCount();
    pSession->sendQ.Enqueue(newPacket);

    SendPost(pSession);

    // 패킷 반환
    CPacket::Free(newPacket);

    // IO COUNT 줄여주기
    int iCount = InterlockedDecrement((unsigned int*)&pSession->IO_Count);
    if (iCount == 0)
        ReleaseSession(pSession);
    
    return true;
}

bool CNetServer::SendPacket_Multicast(vector<DWORD64>* pVecSessionID, DWORD64 dwExceptSessionID, CPacket* pPacket)
{
    int iCount;
    st_SESSION* pSession;
    CPacket* newPacket = CPacket::Alloc();

    newPacket->SetNetPacket(pPacket);   // 헤더 세팅 + payload붙이기
    newPacket->Encoding();
    
    for (auto iter = pVecSessionID->begin(); iter != pVecSessionID->end(); ++iter)
    {
        pSession = FindSession(*iter);
        if (pSession == nullptr)
            return false;

        // 나 빼고 보내야 하는가?
        /*if (*iter == dwExceptSessionID)
            continue;*/

        // 세션 재활용 됐을 때 release flag 보다 send가 먼저 타진다면 바뀐 세션으로 sendpost 하게 됨
        if (pSession->dwSessionID != 0 && pSession->dwSessionID != *iter)
            continue;

        // IO COUNT 증가(ref cnt)
        InterlockedIncrement((long*)&pSession->IO_Count);

        // ReleaseFlag 확인해서 TRUE면 Release 함수에서 이미 통과된 것 -> IO COUNT 줄여주고 나가기..
        if (pSession->bReleaseFlag == TRUE)
        {
            continue;
        }

        newPacket->AddRefCount();
        pSession->sendQ.Enqueue(newPacket);

        // 여기서 연결 끊기고 다른 세션으로 바뀜....
        SendPost(pSession);

        // IO COUNT 줄여주기
        iCount = InterlockedDecrement((unsigned int*)&pSession->IO_Count);
        if (iCount == 0)
            ReleaseSession(pSession);
    }

    // 패킷 반환
    CPacket::Free(newPacket);

    return true;
}

void __stdcall CNetServer::Accept(CNetServer* pThis)
{
    UINT64 uiSessionID = 1;

    int iRet;
    int iError;

    SOCKET client_sock;
    SOCKADDR_IN client;
    int addrlen;
    DWORD dwFlag = 0;

    while (1)
    {
        // accept
        addrlen = sizeof(client);
        client_sock = accept(pThis->m_ListenSocket, (SOCKADDR*)&client, &addrlen);
        if (client_sock == INVALID_SOCKET)
            break;

        ++pThis->m_stMonitoring.iAcceptTotal;
        ++pThis->m_stMonitoring.iAcceptTPS;

        if (pThis->m_iSessionMaxCount == pThis->m_stMonitoring.iSessionCount)
        {
            closesocket(client_sock);
            continue;
        }
            

        // accept 직후 클라이언트 거부할건지 허용할건지 확인
        if (!pThis->OnConnectionRequest(htonl(client.sin_addr.S_un.S_addr), htons(client.sin_port)))
        {
            closesocket(client_sock);
            continue;
        }

        // 세션 생성

        // id 할당 -> index stack에서 사용가능한 세션 배열 index 꺼내오기
        UINT id = uiSessionID++;
        
        UINT index;
        pThis->m_IndexStack->Pop(&index);
        
        st_SESSION* pSession = &pThis->m_ArrSession[index];
        
        InterlockedIncrement((long*)&pThis->m_stMonitoring.iSessionCount);

        pSession->dwSessionID = pThis->CombineIndexID(index, id);
        pSession->sock = client_sock;
        
        ZeroMemory(&pSession->recv_overlapped, sizeof(pSession->recv_overlapped));
        pSession->recv_wsabuf[0].buf = pSession->recvQ.GetRearBufferPtr();
        pSession->recv_wsabuf[0].len = pSession->recvQ.DirectEnqueueSize();
        pSession->iSendPacketCount = 0;
        pSession->bReleaseFlag = FALSE;
        pSession->bCanSend = TRUE;
        pSession->IO_Count = 1;

        // 허용 시 IOCP와 소켓 연결
        CreateIoCompletionPort((HANDLE)client_sock, pThis->m_hIOCP, (ULONG_PTR)pSession, 0);
        
        // 클라이언트가 connect 되었다면 악의적인 connection인지 판단하여 처리해줘야 함
        if (!pThis->OnClientJoin(pSession->dwSessionID))
        {
            // timeout 처리..
            
        }

        // 비동기 IO 시작
        iRet = WSARecv(client_sock, pSession->recv_wsabuf, 1, NULL, &dwFlag, &pSession->recv_overlapped, NULL);

        if (iRet == SOCKET_ERROR)
        {
            iError = WSAGetLastError();

            if (iError != ERROR_IO_PENDING)
            {
                // 로깅..
                if (iError != 10054)
                {
                    //printf("# WSARecv (AcceptThread) > Error code:%d\n", iError);
                }
                
                int iCount = InterlockedDecrement((unsigned int*)&pSession->IO_Count);
                if (iCount == 0)
                    pThis->ReleaseSession(pSession);
            }
            //continue;
        }
    }
}

void __stdcall CNetServer::Work(CNetServer* pThis)
{
    int iRet;

    DWORD dwTransferred;
    SOCKET client_sock;
    OVERLAPPED* overlapped;
    DWORD dwFlag = 0;

    st_SESSION* pSession;

    while (1)
    {
        
        pSession = nullptr;

        dwTransferred = 0;
        client_sock = 0;

        // GQCS..
        iRet = GetQueuedCompletionStatus(pThis->m_hIOCP, &dwTransferred, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&overlapped, INFINITE);

        if (overlapped == nullptr)
            continue;

        if (pSession == nullptr)
        {
            // printf("Session is nullptr\n");
            continue;
        }

        // 비동기 입출력 결과 확인
        if (iRet == 0 || dwTransferred == 0)
        {
            int iCount = InterlockedDecrement((unsigned int*)&pSession->IO_Count);
            if (iCount == 0)
                pThis->ReleaseSession(pSession);

            continue;
        }

        // 주소 값 비교 (recv, send 판단)
        // recv 완료 통지 일 때..
        if (overlapped == &pSession->recv_overlapped)
        {
            InterlockedIncrement((long*)&pThis->m_stMonitoring.iRecvTPS);
            
            pSession->recvQ.MoveRear(dwTransferred);
            st_HEADER stHeader;
            
            while (pSession->recvQ.GetUseSize() >= sizeof(st_HEADER))
            {
                CPacket* recvPacket = CPacket::Alloc();
                recvPacket->Clear();
                pSession->recvQ.Peek((char*)&stHeader, sizeof(st_HEADER));

                if (pSession->recvQ.GetUseSize() < sizeof(st_HEADER) + stHeader.len)
                    break;

                //pSession->recvQ.MoveFront(sizeof(st_HEADER)); // 이제는 header까지 같이 붙여 줘야함
                int iDeqSize = pSession->recvQ.Dequeue(recvPacket->GetBufferPtr(), sizeof(st_HEADER) + stHeader.len);
                recvPacket->MoveWritePos(iDeqSize);
                recvPacket->Decoding();
                recvPacket->MoveReadPos(sizeof(st_HEADER));
                
                pThis->OnRecv(pSession->dwSessionID, recvPacket);

                CPacket::Free(recvPacket);
            }

            pThis->RecvPost(pSession);
        }
        // send 완료 통지 일 때..
        else if (overlapped == &pSession->send_overlapped)
        {
            InterlockedIncrement((long*)&pThis->m_stMonitoring.iSendTPS);

            // sendQ에 있던 동적할당 된 packet 메모리 해제(CPacket 참조 카운트 감소)
            int iCnt = pSession->iSendPacketCount;

            for (int i = 0; i < iCnt; ++i)
            {
                CPacket::Free(pSession->deleteQ[i]);
            }

            //InterlockedExchange((unsigned int*)&pSession->bCanSend, TRUE);
            pSession->bCanSend = TRUE;
            pThis->SendPost(pSession);

            int iCount = InterlockedDecrement((unsigned int*)&pSession->IO_Count);
            if (iCount == 0)
                pThis->ReleaseSession(pSession);
        }
    }
}

void __stdcall CNetServer::Monitoring(CNetServer* pThis)
{
    while (1)
    {
        pThis->m_stMonitoring.iPacketPoolAllocSize = CPacket::GetPacketPoolAllocSize();
        pThis->OnMonitoring();
        
        pThis->m_stMonitoring.iAcceptTPS = 0;
        pThis->m_stMonitoring.iSendTPS = 0;
        pThis->m_stMonitoring.iRecvTPS = 0;

        Sleep(1000);
    }
}

void __stdcall CNetServer::Timer(CNetServer* pThis)
{
    while (1)
    {
        //pThis->OnTimer(GetTickCount64());
        pThis->OnTimer();

        Sleep(1000);
    }
}
