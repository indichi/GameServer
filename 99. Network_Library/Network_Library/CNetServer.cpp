#pragma comment(lib, "Winmm.lib")
#include <thread>
#include <conio.h>

#include "CNetServer.h"
#include "CPacket.h"
#include "CMemoryPoolTLS.h"

CCrashDump g_Dump;

CNetServer::CNetServer(st_SERVER_INFO* stServerInfo)
    : m_ListenSocket(INVALID_SOCKET)
    , m_hIOCP(INVALID_HANDLE_VALUE)
    , m_hAcceptThread(INVALID_HANDLE_VALUE)
    , m_hMonitoringThread(INVALID_HANDLE_VALUE)
    , m_hTimeThread(INVALID_HANDLE_VALUE)
    , m_IndexStack(new CLFStack<DWORD64>)
    , m_hWorkerThreads(nullptr)
    , m_stMonitoring{ 0, 0, 0, 0, 0 }
    , m_ArrSession(nullptr)
{
    wcscpy_s(m_stServerInfo.szIp, stServerInfo->szIp);
    m_stServerInfo.usPort = stServerInfo->usPort;
    m_stServerInfo.iWorkerThreadCount = stServerInfo->iWorkerThreadCount;
    m_stServerInfo.iRunningThreadCount = stServerInfo->iRunningThreadCount;
    m_stServerInfo.bNagle = stServerInfo->bNagle;
    m_stServerInfo.iMaxSessionCount = stServerInfo->iMaxSessionCount;
    m_stServerInfo.uchPacketCode = stServerInfo->uchPacketCode;
    m_stServerInfo.uchPacketKey = stServerInfo->uchPacketKey;
    m_stServerInfo.iTimeout = stServerInfo->iTimeout;
    m_stServerInfo.iContentsThreadCount = stServerInfo->iContentsThreadCount;
}

CNetServer::~CNetServer()
{
    CloseHandle(m_hIOCP);
    CloseHandle(m_hAcceptThread);
    CloseHandle(m_hMonitoringThread);
    CloseHandle(m_hTimeThread);

    for (int i = 0; i < m_stServerInfo.iWorkerThreadCount; ++i)
    {
        CloseHandle(m_hWorkerThreads[i]);
    }

    delete[] m_hWorkerThreads;
    delete[] m_ArrSession;
    delete m_IndexStack;

    WSACleanup();

    timeEndPeriod(1);
}

bool CNetServer::Start()
{
    // 세션 배열 세팅
    m_ArrSession = new st_SESSION[m_stServerInfo.iMaxSessionCount];

    // index stack 세팅
    for (int i = m_stServerInfo.iMaxSessionCount - 1; i >= 0; --i)
    {
        m_IndexStack->Push(i);
    }

    CPacket::s_PacketCode = m_stServerInfo.uchPacketCode;
    CPacket::s_PacketKey = m_stServerInfo.uchPacketKey;

    timeBeginPeriod(1);

    int iRet;
    //int iError;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, m_stServerInfo.iRunningThreadCount);
    if (m_hIOCP == NULL)
        return 1;

    m_ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_ListenSocket == INVALID_SOCKET)
        return 1;

    SOCKADDR_IN server;
    ZeroMemory(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    server.sin_port = htons(m_stServerInfo.usPort);

    // SEND 버퍼 사이즈 0 만들기
    int buffsize = 0;
    iRet = setsockopt(m_ListenSocket, SOL_SOCKET, SO_RCVBUF, (char*)&buffsize, sizeof(buffsize));
    if (iRet == SOCKET_ERROR)
        return -1;
    
    // LINGER 설정
    LINGER stLinger;

    stLinger.l_onoff = 1;
    stLinger.l_linger = 0;

    iRet = setsockopt(m_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));
    if (iRet == SOCKET_ERROR)
        return 1;

    if (m_stServerInfo.bNagle)
    {
        int iOpt = 1;
        iRet = setsockopt(m_ListenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&iOpt, sizeof(iOpt));
        if (iRet == SOCKET_ERROR)
            return 1;
    }

    iRet = bind(m_ListenSocket, (SOCKADDR*)&server, sizeof(server));

    if (iRet == SOCKET_ERROR)
        return 1;

    iRet = listen(m_ListenSocket, SOMAXCONN_HINT(60000));
    if (iRet == SOCKET_ERROR)
        return 1;

    // 작업자 스레드 생성
    m_hWorkerThreads = new HANDLE[m_stServerInfo.iWorkerThreadCount];

    for (int i = 0; i < m_stServerInfo.iWorkerThreadCount; i++) {
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

bool CNetServer::Disconnect(DWORD64 dwSessionID)
{
    st_SESSION* pSession = GetCheckedSession(dwSessionID);
    if (pSession == nullptr)
        return false;
    
    CancelIoEx((HANDLE)pSession->sock, NULL);

    // Disconnect 완료되면 IO COUNT 줄여주기
    if (InterlockedDecrement((long*)&pSession->IO_Count) == 0)
        ReleaseSession(pSession);
    
    return true;
}

void CNetServer::ReleaseSession(st_SESSION* pSession)
{
    // IO_COUNT 세션 사용여부 개념(ref cnt)로 사용
    /*if (InterlockedCompareExchange64((long long*)&pSession->bReleaseFlag, TRUE, FALSE) != FALSE)
        return;*/
    
    LONG64 cmp[2] = { 0 , 0 };

    if (!InterlockedCompareExchange128((LONG64*)&pSession->bReleaseFlag, 0, TRUE, cmp))
        return;

    closesocket(pSession->sock);

    DWORD64 index = pSession->dwSessionID & 0xffff;

    OnClientLeave(pSession->dwSessionID);

    //InterlockedExchange((unsigned long*)&pSession->bCanSend, TRUE);
    pSession->sock = INVALID_SOCKET;
    pSession->dwSessionID = -1;
    pSession->iSendPacketCount = 0;
    pSession->recvQ.ClearBuffer();

    CPacket* pPacket;
    while (pSession->sendQ.Dequeue(&pPacket))
    {
        CPacket::Free(pPacket);
    }

    m_IndexStack->Push(index);
    InterlockedDecrement((long*)&m_stMonitoring.iSessionCount);
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

    InterlockedIncrement((long*)&pSession->IO_Count);
    
    iRet = WSARecv(pSession->sock, pSession->recv_wsabuf, iBufCount, NULL, &dwFlag, &pSession->recv_overlapped, NULL);
    if (iRet == SOCKET_ERROR)
    {
        iError = WSAGetLastError();

        if (iError != ERROR_IO_PENDING)
        {
            if (iError != 10038 && iError != 10053 && iError != 10054 && iError != 10058)
            {
                FILE* hFile;
                _wfopen_s(&hFile, L"SOCKET_ERROR.txt", L"a");

                if (hFile != NULL)
                {
                    WCHAR buf[100];
                    swprintf_s(buf, L"(RecvPost) Error No : %d\n", iError);
                    fwrite(buf, sizeof(WCHAR), wcslen(buf), hFile);
                    fclose(hFile);
                }
            }

            if (InterlockedDecrement((long*)&pSession->IO_Count) == 0)
                ReleaseSession(pSession);

        }
    }
}

void CNetServer::SendPost(st_SESSION* pSession)
{
    if (pSession->sendQ.GetSize() > 0 && InterlockedExchange((long*)&pSession->bCanSend, FALSE) == TRUE)
    {
        int iRet;
        int iError;

        int iUseSize = pSession->sendQ.GetSize();
        if (iUseSize == 0)
        {
            InterlockedExchange((unsigned long*)&pSession->bCanSend, TRUE);
            //pSession->bCanSend = TRUE;
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

        InterlockedIncrement((long*)&pSession->IO_Count);
        iRet = WSASend(pSession->sock, wsabuf, iPacketCount, NULL, 0, &pSession->send_overlapped, NULL);

        if (iRet == SOCKET_ERROR)
        {
            iError = WSAGetLastError();

            if (iError != ERROR_IO_PENDING)
            {
                if (iError != 10038 && iError != 10053 && iError != 10054 && iError != 10058)
                {
                    FILE* hFile;
                    _wfopen_s(&hFile, L"SOCKET_ERROR.txt", L"a");

                    if (hFile != NULL)
                    {
                        WCHAR buf[100];
                        swprintf_s(buf, L"(SendPost) Error No : %d\n", iError);
                        fwrite(buf, sizeof(WCHAR), wcslen(buf), hFile);
                        fclose(hFile);
                    }
                }

                // CPacket 참조 카운트 감소
                for (int i = 0; i < iPacketCount; ++i)
                {
                    CPacket::Free(pSession->deleteQ[i]);
                }

                if (InterlockedDecrement((long*)&pSession->IO_Count) == 0)
                    ReleaseSession(pSession);
            }
        }
    }
}

DWORD64 CNetServer::CombineIndexID(DWORD64 dwIndex, DWORD64 dwID)
{
    return (dwID << 16) | dwIndex;
}

CNetServer::st_SESSION* CNetServer::GetCheckedSession(DWORD64 dwSessionID)
{
    DWORD64 index = dwSessionID & 0xffff;

    if (index == 0xffff)
    {
        FILE* hFile;
        _wfopen_s(&hFile, L"Check.txt", L"a");

        if (hFile != NULL)
        {
            WCHAR buf[100];
            swprintf_s(buf, L"(Check) index == ffff\n");
            fwrite(buf, sizeof(WCHAR), wcslen(buf), hFile);
            fclose(hFile);
        }

        return nullptr;
    }

    if (index == -1)
    {
        FILE* hFile;
        _wfopen_s(&hFile, L"Check.txt", L"a");

        if (hFile != NULL)
        {
            WCHAR buf[100];
            swprintf_s(buf, L"(Check) index == -1\n");
            fwrite(buf, sizeof(WCHAR), wcslen(buf), hFile);
            fclose(hFile);
        }

        return nullptr;
    }

    st_SESSION* pSession = &m_ArrSession[index];

    // 재활용 된 세션 확인
    if (pSession->dwSessionID != dwSessionID)
        return nullptr;

    if (pSession->bReleaseFlag == TRUE)
        return nullptr;

    // IO Count 가 0 이었을 때
    if (InterlockedIncrement((long*)&pSession->IO_Count) == 1)
    {
        if (InterlockedDecrement((long*)&pSession->IO_Count) == 0)
            ReleaseSession(pSession);

        return nullptr;
    }

    if (pSession->dwSessionID != dwSessionID)
    {
        if (InterlockedDecrement((long*)&pSession->IO_Count) == 0)
            ReleaseSession(pSession);

        return nullptr;
    }

    if (pSession->bReleaseFlag == TRUE)
    {
        if (InterlockedDecrement((long*)&pSession->IO_Count) == 0)
            ReleaseSession(pSession);
        
        return nullptr;
    }

    return pSession;
}

bool CNetServer::SendPacket_Unicast(DWORD64 dwSessionID, CPacket* pPacket)
{
    // GetCheckedSession에서 체크하면서 IO COUNT 증가 시킴
    st_SESSION* pSession = GetCheckedSession(dwSessionID);
    if (pSession == nullptr)
        return false;

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
    if (InterlockedDecrement((long*)&pSession->IO_Count) == 0)
        ReleaseSession(pSession);
    
    return true;
}

bool CNetServer::SendPacket_Multicast(vector<DWORD64>* pVecSessionID, DWORD64 dwExceptSessionID, CPacket* pPacket)
{
    st_SESSION* pSession;
    CPacket* newPacket = CPacket::Alloc();

    newPacket->SetNetPacket(pPacket);   // 헤더 세팅 + payload붙이기
    newPacket->Encoding();
    
    for (auto iter = pVecSessionID->begin(); iter != pVecSessionID->end(); ++iter)
    {
        // GetCheckedSession에서 체크하면서 IO COUNT 증가 시킴
        st_SESSION* pSession = GetCheckedSession(*iter);
        if (pSession == nullptr)
            continue;

        newPacket->AddRefCount();
        pSession->sendQ.Enqueue(newPacket);

        SendPost(pSession);

        // IO COUNT 줄여주기
        if (InterlockedDecrement((long*)&pSession->IO_Count) == 0)
            ReleaseSession(pSession);
    }

    // 패킷 반환
    CPacket::Free(newPacket);

    return true;
}

void __stdcall CNetServer::Accept(CNetServer* pThis)
{
    UINT64 uiSessionID = 1;

    SOCKET client_sock;
    SOCKADDR_IN client;
    int addrlen;
    DWORD dwFlag = 0;

    while (1)
    {
        client_sock = INVALID_SOCKET;

        // accept
        addrlen = sizeof(client);
        client_sock = accept(pThis->m_ListenSocket, (SOCKADDR*)&client, &addrlen);
        if (client_sock == INVALID_SOCKET)
            continue;

        ++pThis->m_stMonitoring.iAcceptTotal;
        ++pThis->m_stMonitoring.iAcceptTPS;

        if (pThis->m_stServerInfo.iMaxSessionCount == pThis->m_stMonitoring.iSessionCount)
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
        DWORD64 id = uiSessionID++;
        DWORD64 index;

        pThis->m_IndexStack->Pop(&index);
        st_SESSION* pSession = &pThis->m_ArrSession[index];
        InterlockedIncrement((long*)&pThis->m_stMonitoring.iSessionCount);

        pSession->dwSessionID = pThis->CombineIndexID(index, id);
        pSession->sock = client_sock;
        ZeroMemory(&pSession->recv_overlapped, sizeof(pSession->recv_overlapped));
        pSession->recv_wsabuf[0].buf = pSession->recvQ.GetRearBufferPtr();
        pSession->recv_wsabuf[0].len = pSession->recvQ.DirectEnqueueSize();
        pSession->iSendPacketCount = 0;
        
        //pSession->IO_Count = 0;
        InterlockedIncrement((long*)&pSession->IO_Count);
        InterlockedExchange(&pSession->bReleaseFlag, FALSE);
        InterlockedExchange(&pSession->bCanSend, TRUE);
        
        // 허용 시 IOCP와 소켓 연결
        CreateIoCompletionPort((HANDLE)client_sock, pThis->m_hIOCP, (ULONG_PTR)pSession, 0);
        
        // 클라이언트가 connect 되었다면 악의적인 connection인지 판단하여 처리해줘야 함
        if (!pThis->OnClientJoin(pSession->dwSessionID))
        {
            // timeout 처리..
            
        }

        //// 비동기 IO 시작
        pThis->RecvPost(pSession);
        if (InterlockedDecrement((long*)&pSession->IO_Count) == 0)
            pThis->ReleaseSession(pSession);
    }
}

void __stdcall CNetServer::Work(CNetServer* pThis)
{
    int iRet;

    DWORD dwTransferred;
    OVERLAPPED* overlapped;
    DWORD dwFlag = 0;

    st_SESSION* pSession;

    while (1)
    {
        overlapped = 0;
        pSession = 0;
        dwTransferred = 0;

        // GQCS..
        iRet = GetQueuedCompletionStatus(pThis->m_hIOCP, &dwTransferred, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&overlapped, INFINITE);

        if (overlapped == NULL)
            continue;
        
        if (pSession == NULL)
            continue;

        if (dwTransferred == 0)
        {
            if (InterlockedDecrement((long*)&pSession->IO_Count) == 0)
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

            //pSession->bCanSend = TRUE;
            InterlockedExchange((unsigned long*)&pSession->bCanSend, TRUE);
            pThis->SendPost(pSession);
        }

        if (InterlockedDecrement((long*)&pSession->IO_Count) == 0)
            pThis->ReleaseSession(pSession);
            
    }
}

void __stdcall CNetServer::Monitoring(CNetServer* pThis)
{
    st_LIB_MONITORING stTemp;

    while (1)
    {
        stTemp = pThis->m_stMonitoring;

        pThis->m_stMonitoring.iAcceptTPS = 0;
        pThis->m_stMonitoring.iSendTPS = 0;
        pThis->m_stMonitoring.iRecvTPS = 0;

        pThis->OnMonitoring(&stTemp);
        
        Sleep(1000);
    }
}

void __stdcall CNetServer::Timer(CNetServer* pThis)
{
    while (1)
    {
        //pThis->OnTimer(GetTickCount64());
        Sleep(1000);

        pThis->OnTimer();
    }
}
