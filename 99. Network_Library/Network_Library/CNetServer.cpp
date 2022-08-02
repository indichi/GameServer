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
    , m_hWorkerThreads(nullptr)
    , m_iWorkerThreadCount(0)
    , m_stMonitoring{ 0, 0, 0, 0, 0 }
    , m_IndexStack()
{
    //InitializeSRWLock(&m_IndexStackLocker);

    // index stack 세팅
    for (int i = iSessionMaxCount - 1; i >= 0; --i)
    {
        m_IndexStack.Push(i);
    }
}

CNetServer::~CNetServer()
{
    CloseHandle(m_hIOCP);
    CloseHandle(m_hAcceptThread);
    CloseHandle(m_hMonitoringThread);

    for (int i = 0; i < m_iWorkerThreadCount; ++i)
    {
        CloseHandle(m_hWorkerThreads[i]);
    }

    delete[] m_hWorkerThreads;

    WSACleanup();

    timeEndPeriod(1);
}

bool CNetServer::Start(const WCHAR* szIp, unsigned short usPort, int iWorkerThreadCount, bool bNagle, int iMaxUserCount)
{
    timeBeginPeriod(1);

    int iRet;
    //int iError;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
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

    //HANDLE hThread;
    for (int i = 0; i < iWorkerThreadCount; i++) {
        m_hWorkerThreads[i] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)CNetServer::Work, this, 0, nullptr);
        if (m_hWorkerThreads[i] == NULL)
            return 1;
    }

    m_hAcceptThread = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)CNetServer::Accept, this, 0, nullptr);
    if (m_hAcceptThread == NULL)
        return 1;

    m_hMonitoringThread = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)CNetServer::Monitoring, this, 0, nullptr);
    if (m_hMonitoringThread == NULL)
        return 1;

    //// disconnect test
    //int i = 1;
    //while (true)
    //{
    //    if (_kbhit())
    //    {
    //        int iKey = _getch();

    //        if (iKey == VK_SPACE)
    //        {
    //            Disconnect(i++);
    //        }
    //    }

    //    Sleep(10);
    //}

    WaitForMultipleObjects(iWorkerThreadCount, m_hWorkerThreads, TRUE, INFINITE);
    WaitForSingleObject(m_hAcceptThread, INFINITE);
    WaitForSingleObject(m_hMonitoringThread, INFINITE);

    return true;
}

void CNetServer::Stop()
{
}

int CNetServer::GetSessionCount() const
{
    // return (int)m_SessionMap.size();
}

bool CNetServer::Disconnect(UINT64 dwSessionID)
{
    /*AcquireSRWLockExclusive(&m_SeesionMapLocker);
    auto iter = m_SessionMap.find(dwSessionID);
    ReleaseSRWLockExclusive(&m_SeesionMapLocker);

    if (iter == m_SessionMap.end())
        return false;*/

    st_SESSION* pSession = FindSession(dwSessionID);
    if (pSession == nullptr)
        return false;

    // IO COUNT 증가(ref cnt)
    InterlockedIncrement((long*)&pSession->IO_Count);

    // ReleaseFlag 확인해서 TRUE면 Release 함수에서 이미 통과된 것 -> IO COUNT 줄여주고 나가기..
    if (pSession->bReleaseFlag == TRUE)
        return false;

    CancelIoEx((HANDLE)pSession->sock, NULL);

    // Disconnect 완료되면 IO COUNT 줄여주기
    InterlockedDecrement((long*)&pSession->IO_Count);

    return true;
}

void CNetServer::ReleaseSession(st_SESSION* pSession)
{
    // IO_COUNT 세션 사용여부 개념(ref cnt)로 사용
    long long comp = 0;
    if (InterlockedCompareExchange64((long long*)&pSession->bReleaseFlag, TRUE, comp) != 0)
        return;
    
    UINT index = GetIndex(pSession->dwSessionID);

    pSession->dwSessionID = 0;
    pSession->iSendPacketCount = 0;
    pSession->recvQ.ClearBuffer();
    //pSession->sendQ.ClearBuffer();

    closesocket(pSession->sock);

    pSession->sock = INVALID_SOCKET;

    InterlockedDecrement((unsigned int*)&m_stMonitoring.iSessionCount);

    m_IndexStack.Push(index);

    //m_SessionPool.Free(pSession);
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

    InterlockedIncrement((unsigned int*)&pSession->IO_Count);
    iRet = WSARecv(pSession->sock, pSession->recv_wsabuf, iBufCount, NULL, &dwFlag, &pSession->recv_overlapped, NULL);
    if (iRet == SOCKET_ERROR)
    {
        iError = WSAGetLastError();

        if (iError != ERROR_IO_PENDING)
        {
            if (iError != 10054)
                printf("# WSARecv (WorkerThread) > Error code:%d\n", iError);

            int iCount = InterlockedDecrement((unsigned int*)&pSession->IO_Count);
            if (iCount == 0)
                ReleaseSession(pSession);
        }
    }
}

void CNetServer::SendPost(st_SESSION* pSession)
{
    if (pSession->sendQ.GetSize() > 0 && InterlockedExchange((unsigned int*)&pSession->bCanSend, FALSE) == TRUE)
    //if (pSession->sendQ.GetUseSize() > 0 && InterlockedExchange((unsigned int*)&pSession->bCanSend, FALSE) == TRUE)
    {
        int iRet;
        int iError;

        //int iUseSize = pSession->sendQ.GetUseSize();
        int iUseSize = pSession->sendQ.GetSize();
        if (iUseSize == 0)
        {
            InterlockedExchange((unsigned int*)&pSession->bCanSend, TRUE);
            return;
        }

        int iPacketCount = iUseSize;
        /*int iPacketCount = iUseSize / sizeof(CPacket*);
        if (iPacketCount > 100)
        {
            iPacketCount = 100;
        }*/

        WSABUF      wsabuf[100];
        //CPacket*    buf[100];
        CPacket*    pPacket;

        //pSession->sendQ.Peek((char*)buf, sizeof(CPacket*) * iPacketCount);
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
                    printf("# WSASend > Error code:%d\n", iError);

                // CPacket 참조 카운트 감소
                for (int i = 0; i < iPacketCount; ++i)
                {
                    CPacket::Free(pSession->deleteQ[i]);
                }

                //pSession->sendQ.MoveFront(sizeof(CPacket*) * iPacketCount);

                int iCount = InterlockedDecrement((unsigned int*)&pSession->IO_Count);

                if (iCount == 0)
                    ReleaseSession(pSession);
                else
                    InterlockedExchange((unsigned int*)&pSession->bCanSend, TRUE);
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
    
    if (index >= 10000)
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

bool CNetServer::SendPacket(UINT64 dwSessionID, CPacket* pPacket)
{
    st_SESSION* pSession = FindSession(dwSessionID);
    if (pSession == nullptr)
        return false;

    // IO COUNT 증가(ref cnt)
    InterlockedIncrement((long*)&pSession->IO_Count);

    // ReleaseFlag 확인해서 TRUE면 Release 함수에서 이미 통과된 것 -> IO COUNT 줄여주고 나가기..
    if (pSession->bReleaseFlag == TRUE)
        return false;

    st_HEADER stHeader;

    // CPacket 메모리 풀에서 alloc, sendQ에 Enqueue 전에 참조 카운트 증가
    CPacket* newPacket = CPacket::Alloc();

    newPacket->SetNetHeader(pPacket);   // 헤더 세팅 + payload붙이기
    newPacket->Encoding();

    newPacket->AddRefCount();
    pSession->sendQ.Enqueue(newPacket);

    SendPost(pSession);

    // 패킷 반환
    CPacket::Free(newPacket);

    // IO COUNT 줄여주기
    InterlockedDecrement((long*)&pSession->IO_Count);

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
        pThis->m_IndexStack.Pop(&index);
        
        st_SESSION* pSession = &pThis->m_ArrSession[index];
        
        ++pThis->m_stMonitoring.iSessionCount;

        pSession->dwSessionID = pThis->CombineIndexID(index, id);
        pSession->sock = client_sock;
        
        ZeroMemory(&pSession->recv_overlapped, sizeof(pSession->recv_overlapped));
        pSession->recv_wsabuf[0].buf = pSession->recvQ.GetRearBufferPtr();
        pSession->recv_wsabuf[0].len = pSession->recvQ.DirectEnqueueSize();
        pSession->iSendPacketCount = 0;
        pSession->bCanSend = TRUE;
        pSession->bReleaseFlag = FALSE;

        // 허용 시 IOCP와 소켓 연결
        CreateIoCompletionPort((HANDLE)client_sock, pThis->m_hIOCP, (ULONG_PTR)pSession, 0);

        pSession->IO_Count = 1;

        // timeout 로그인 전에 처리 확인
        if (!pThis->OnClientJoin(pSession->dwSessionID))
        {
            // timeout 처리..
            printf("Join return False!!\n");
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
                    printf("# WSARecv (AcceptThread) > Error code:%d\n", iError);

                int iCount = InterlockedDecrement((unsigned int*)&pSession->IO_Count);
                if (iCount == 0)
                    pThis->ReleaseSession(pSession);
            }
            continue;
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

    while (1)
    {
        st_SESSION* pSession = nullptr;

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
            ++pThis->m_stMonitoring.iRecvTPS;

            pSession->recvQ.MoveRear(dwTransferred);
            st_HEADER stHeader;
            CPacket* recvPacket = CPacket::Alloc();

            while (pSession->recvQ.GetUseSize() >= sizeof(st_HEADER))
            {
                recvPacket->Clear();
                pSession->recvQ.Peek((char*)&stHeader, sizeof(st_HEADER));

                if (pSession->recvQ.GetUseSize() < sizeof(st_HEADER) + stHeader.len)
                    break;

                pSession->recvQ.MoveFront(sizeof(st_HEADER));
                int iDeqSize = pSession->recvQ.Dequeue(recvPacket->GetBufferPtr(), stHeader.len);
                recvPacket->MoveWritePos(iDeqSize);

                pThis->OnRecv(pSession->dwSessionID, recvPacket);
            }

            CPacket::Free(recvPacket);

            pThis->RecvPost(pSession);
        }
        // send 완료 통지 일 때..
        else if (overlapped == &pSession->send_overlapped)
        {
            ++pThis->m_stMonitoring.iSendTPS;

            // sendQ에 있던 동적할당 된 packet 메모리 해제(CPacket 참조 카운트 감소)
            int iCnt = pSession->iSendPacketCount;

            //CPacket* buf[100];
            //pSession->sendQ.Dequeue((char*)buf, iCnt * sizeof(CPacket*));

            for (int i = 0; i < iCnt; ++i)
            {
                //buf[i]->SubRefCount();
                CPacket::Free(pSession->deleteQ[i]);
            }

            InterlockedExchange((unsigned int*)&pSession->bCanSend, TRUE);
            pThis->SendPost(pSession);
        }

        int iCount = InterlockedDecrement((unsigned int*)&pSession->IO_Count);

        if (iCount == 0)
            pThis->ReleaseSession(pSession);
    }
}

void __stdcall CNetServer::Monitoring(CNetServer* pThis)
{
    while (1)
    {
       /* AcquireSRWLockShared(&pThis->m_SeesionMapLocker);
        pThis->m_stMonitoring.iSessionCount = (int)pThis->m_SessionMap.size();
        ReleaseSRWLockShared(&pThis->m_SeesionMapLocker);*/

        printf("============================================================================================\n");

        /*printf("Session Count:%d\nAccept Total:%d\nAccept TPS:%d\nSend TPS:%d\nRecv TPS:%d\n\nPacket Pool Capacity:%d\nPacket Pool UseCount:%d\n",
            pThis->m_stMonitoring.iSessionCount, pThis->m_stMonitoring.iAcceptTotal, pThis->m_stMonitoring.iAcceptTPS, pThis->m_stMonitoring.iSendTPS, pThis->m_stMonitoring.iRecvTPS, g_PacketPool.GetCapacity(), g_PacketPool.GetUseCount());*/

        printf("Session Count:%d\nAccept Total:%d\nAccept TPS:%d\nSend TPS:%d\nRecv TPS:%d\n",
            pThis->m_stMonitoring.iSessionCount, pThis->m_stMonitoring.iAcceptTotal, pThis->m_stMonitoring.iAcceptTPS, pThis->m_stMonitoring.iSendTPS, pThis->m_stMonitoring.iRecvTPS);
        printf("============================================================================================\n\n");

        pThis->m_stMonitoring.iAcceptTPS = 0;
        pThis->m_stMonitoring.iSendTPS = 0;
        pThis->m_stMonitoring.iRecvTPS = 0;

        Sleep(1000);
    }
}
