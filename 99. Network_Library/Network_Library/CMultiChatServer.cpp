#include <process.h>

#include "CMultiChatServer.h"
#include "CommonProtocol.h"

CMultiChatServer::CMultiChatServer(st_SERVER_INFO* stServerInfo)
    : CNetServer(stServerInfo)
    , m_JobPool(new CLFMemoryPool<st_JOB>)
    , m_PlayerPool(new CLFMemoryPool<st_PLAYER>)
    , m_SectorList{}
    , m_iContentsThreadsCount(stServerInfo->iContentsThreadCount)
    , m_hContentsThread(new HANDLE[stServerInfo->iContentsThreadCount])
    , m_JobQ(new CLFQueue<st_JOB*>[stServerInfo->iContentsThreadCount])
    , m_PlayerMap(new unordered_map<DWORD64, st_PLAYER*>[stServerInfo->iContentsThreadCount])
    , m_hEvent(new HANDLE[stServerInfo->iContentsThreadCount])
    , m_iUpdateTPS(0)
{
    // 섹터 리스트, Around, SRWLock 초기화 및 설정
    for (int i = 0; i < dfSECTOR_X; ++i)
    {
        for (int j = 0; j < dfSECTOR_Y; ++j)
        {
            InitializeSRWLock(&m_SectorList[i][j].locker);
            SetSectorAround(i, j, &m_SectorList[i][j].stAround);
            m_SectorList[i][j].pPlayerList = new list<st_PLAYER*>;
        }
    }

    // 이벤트 생성
    for (int i = 0; i < stServerInfo->iContentsThreadCount; ++i)
    {
        m_hEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
}

CMultiChatServer::~CMultiChatServer()
{
    // 섹터 리스트, Around, SRWLock 초기화 및 설정
    for (int i = 0; i < dfSECTOR_X; ++i)
    {
        for (int j = 0; j < dfSECTOR_Y; ++j)
        {
            delete m_SectorList[i][j].pPlayerList;
        }
    }

    for (int i = 0; i < m_iContentsThreadsCount; ++i)
    {
        CloseHandle(m_hContentsThread[i]);
        CloseHandle(m_hEvent[i]);
    }

    delete[] m_JobQ;
    delete[] m_PlayerMap;
    delete[] m_hContentsThread;
    delete[] m_hEvent;

    delete m_JobPool;
    delete m_PlayerPool;
}

bool CMultiChatServer::OnConnectionRequest(unsigned long szIp, unsigned short usPort)
{
    return true;
}

bool CMultiChatServer::OnClientJoin(DWORD64 dwSessionID)
{
    st_JOB* stJob = m_JobPool->Alloc();
    stJob->eHandle = CNetServer::eFuncHandler::OnClientJoin;
    stJob->dwSessionID = dwSessionID;

    // TODO Join 시 담당 쓰레드 정해줘야 함
    //int iIndex = dwSessionID % m_iContentsThreadsCount;

    m_JobQ[dwSessionID % m_iContentsThreadsCount].Enqueue(stJob);
    SetEvent(m_hEvent[dwSessionID % m_iContentsThreadsCount]);

    return true;
}

void CMultiChatServer::OnClientLeave(DWORD64 dwSessionID)
{
    st_JOB* stJob = m_JobPool->Alloc();
    stJob->eHandle = CNetServer::eFuncHandler::OnClientLeave;
    stJob->dwSessionID = dwSessionID;

    //int iIndex = dwSessionID % m_iContentsThreadsCount;

    m_JobQ[dwSessionID % m_iContentsThreadsCount].Enqueue(stJob);
    SetEvent(m_hEvent[dwSessionID % m_iContentsThreadsCount]);
}

void CMultiChatServer::OnRecv(DWORD64 dwSessionID, CPacket* pPacket)
{
    st_JOB* stJob = m_JobPool->Alloc();
    stJob->eHandle = CNetServer::eFuncHandler::OnRecv;
    stJob->dwSessionID = dwSessionID;
    stJob->pPacket = pPacket;

    // TODO 각 쓰레드의 세션 맵과 인덱스 가지고 있는 컨테이너와 어떻게 동기화 맞출건지..
    //int iIndex = dwSessionID % m_iContentsThreadsCount;
    pPacket->AddRefCount();
    m_JobQ[dwSessionID % m_iContentsThreadsCount].Enqueue(stJob);
    SetEvent(m_hEvent[dwSessionID % m_iContentsThreadsCount]);
}

void CMultiChatServer::OnError(DWORD64 dwSessionID, int iErrorCode, WCHAR* szError)
{
}

void CMultiChatServer::OnTimer()
{
    m_iUpdateTPS = 0;

    for (int i = 0; i < m_iContentsThreadsCount; ++i)
    {
        st_JOB* stJob = m_JobPool->Alloc();
        stJob->eHandle = CNetServer::eFuncHandler::OnTimer;

        m_JobQ[i].Enqueue(stJob);
        SetEvent(m_hEvent[i]);
    }
}

void CMultiChatServer::OnMonitoring(st_LIB_MONITORING* pLibMonitoring)
{
    int iJobQSize[10];
    int iPlayerMapSize[10];

    for (int i = 0; i < m_iContentsThreadsCount; ++i)
    {
        iJobQSize[i] = m_JobQ[i].GetSize();
        iPlayerMapSize[i] = (int)m_PlayerMap[i].size();
    }

    printf("============================================================================================\n");

    printf("%30s : %d\n", "Session Count", pLibMonitoring->iSessionCount);
    printf("%30s : %d\n", "Packet Pool Alloc Size", CPacket::GetPacketPoolAllocSize());
    printf("\n");
    printf("%30s : %d\n", "Update Job Pool Alloc Size", m_JobPool->GetCapacity());

    for (int i = 0; i < m_iContentsThreadsCount; ++i)
    {
        printf("[Th%d]%25s : %d\n", i, "Update Job Queue Size", iJobQSize[i]);
    }

    printf("\n");
    printf("%30s : %d\n", "Player Pool Alloc Size", m_PlayerPool->GetCapacity());
    printf("[Total]%23s : %d\n", "Player Count", m_PlayerPool->GetUseCount());

    for (int i = 0; i < m_iContentsThreadsCount; ++i)
    {
        printf("[Th%d]%25s : %d\n", i, "Player Count", iPlayerMapSize[i]);
    }

    printf("\n");
    printf("%30s : %d\n", "Accpet Total", pLibMonitoring->iAcceptTotal);
    printf("%30s : %d\n", "Accept TPS", pLibMonitoring->iAcceptTPS);
    printf("%30s : %d\n", "Update TPS", m_iUpdateTPS);
    printf("\n");
    printf("%30s : %d\n", "Recv Message TPS", pLibMonitoring->iRecvTPS);
    printf("%30s : %d\n", "Send Message TPS", pLibMonitoring->iSendTPS);

    printf("============================================================================================\n\n");
}

bool CMultiChatServer::Start()
{
    CNetServer::Start();
    
    st_THREAD_ARGS* args = new st_THREAD_ARGS[m_iContentsThreadsCount];

    for (int i = 0; i < m_iContentsThreadsCount; ++i)
    {
        args[i].pThis = this;
        args[i].iMyIndex = i;

        m_hContentsThread[i] = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)Contents, (void*)&args[i], 0, nullptr);
        /*if (m_hContentsThread == NULL)
            return 1;*/
    }

    WaitForMultipleObjects(m_iContentsThreadsCount, m_hContentsThread, TRUE, INFINITE);

    delete[] args;

    return true;
}

void __stdcall CMultiChatServer::Contents(LPVOID args)
{
    st_THREAD_ARGS* pArgs = (st_THREAD_ARGS*)args;

    CMultiChatServer* pThis = pArgs->pThis;
    int iMyIndex = pArgs->iMyIndex;
    HANDLE hMyEvent = pThis->m_hEvent[iMyIndex];
    CLFQueue<st_JOB*>* pMyJobQ = &pThis->m_JobQ[iMyIndex];

    st_JOB* stJob;

    while (1)
    {
        WaitForSingleObject(hMyEvent, INFINITE);

        while (pMyJobQ->GetSize() > 0)
        {
            pMyJobQ->Dequeue(&stJob);

            switch (stJob->eHandle)
            {
            case CNetServer::eFuncHandler::OnConnectionRequest:
                break;
            case CNetServer::eFuncHandler::OnClientJoin:
            {
                if (pThis->ContentsOnClientJoin(stJob->dwSessionID))
                {
                }
            }
            break;
            case CNetServer::eFuncHandler::OnClientLeave:
            {
                if (pThis->ContentsOnClientLeave(stJob->dwSessionID))
                {
                }
            }
            break;
            case CNetServer::eFuncHandler::OnRecv:
            {
                pThis->ContentsOnRecv(stJob);
            }
            break;
            case CNetServer::eFuncHandler::OnError:
            {

            }
            break;
            case CNetServer::eFuncHandler::OnTimer:
            {
                pThis->ContentsOnTimer(iMyIndex);
            }
            break;
            default:
                break;
                // TODO add log
            }

            InterlockedIncrement((long*)&pThis->m_iUpdateTPS);
            pThis->m_JobPool->Free(stJob);
        }
    }
}

void CMultiChatServer::SetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
{
    pSectorAround->iCount = 0;

    --iSectorX;
    --iSectorY;

    for (int iX = 0; iX < 3; ++iX)
    {
        if (iSectorX + iX < 0 || iSectorX + iX >= dfSECTOR_X)
            continue;

        for (int iY = 0; iY < 3; ++iY)
        {
            if (iSectorY + iY < 0 || iSectorY + iY >= dfSECTOR_Y)
                continue;

            pSectorAround->Around[pSectorAround->iCount]._wX = iSectorX + iX;
            pSectorAround->Around[pSectorAround->iCount]._wY = iSectorY + iY;
            ++pSectorAround->iCount;
        }
    }
}

bool CMultiChatServer::ContentsOnClientJoin(DWORD64 dwSessionID)
{
    unordered_map<DWORD64, st_PLAYER*>* pPlayerMap = &m_PlayerMap[dwSessionID % m_iContentsThreadsCount];

    // Join인데 있으면 이상한거임
    auto iter = pPlayerMap->find(dwSessionID);
    if (iter != pPlayerMap->end())
        return false;
    
    st_PLAYER* pPlayer = m_PlayerPool->Alloc();

    pPlayer->_dwSessionID = dwSessionID;
    pPlayer->_iAccountNo = 0;
    pPlayer->_bCanChat = false;
    pPlayer->_dwHeartbeat = GetTickCount64();

    pPlayerMap->insert(make_pair(dwSessionID, pPlayer));

    return true;
}

bool CMultiChatServer::ContentsOnClientLeave(DWORD64 dwSessionID)
{
    unordered_map<DWORD64, st_PLAYER*>* pPlayerMap = &m_PlayerMap[dwSessionID % m_iContentsThreadsCount];

    auto iter = pPlayerMap->find(dwSessionID);
    if (iter == pPlayerMap->end())
        return false;     // TODO add log

    st_PLAYER* pPlayer = iter->second;

    if (pPlayer->_bCanChat == true)
    {
        list<st_PLAYER*>* pExistList = m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].pPlayerList;

        AcquireSRWLockExclusive(&m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].locker);
        for (auto iter = pExistList->begin(); iter != pExistList->end(); ++iter)
        {
            if ((*iter)->_dwSessionID == pPlayer->_dwSessionID)
            {
                pExistList->erase(iter);
                break;
            }
        }
        ReleaseSRWLockExclusive(&m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].locker);
    }

    pPlayerMap->erase(pPlayer->_dwSessionID);
    m_PlayerPool->Free(pPlayer);

    return true;
}

void CMultiChatServer::ContentsOnRecv(st_JOB* stJob)
{
    DWORD64 dwSessionID = stJob->dwSessionID;
    unordered_map<DWORD64, st_PLAYER*>* pPlayerMap = &m_PlayerMap[dwSessionID % m_iContentsThreadsCount];
    CPacket* pPacket = stJob->pPacket;
    st_PLAYER* pPlayer;

    // 데이터 마샬링
    WORD wType;
    *pPacket >> wType;

    auto iter = pPlayerMap->find(dwSessionID);
    if (iter == pPlayerMap->end())
        return;

    pPlayer = iter->second;
    pPlayer->_dwHeartbeat = GetTickCount64();

    switch (wType)
    {
    case en_PACKET_CS_CHAT_REQ_LOGIN:
    {
        Packet_Login(pPlayer, pPacket);
    }
    break;
    case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
    {
        Packet_SectorMove(pPlayer, pPacket);
    }
    break;
    case en_PACKET_CS_CHAT_REQ_MESSAGE:
    {
        Packet_Chat(pPlayer, pPacket);
    }
    break;
    case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
    {
        pPlayer->_dwHeartbeat = GetTickCount64();
    }
    break;
    default:
        // TODO 이상한 메세지 타입은 로그 남겨야 함..
        break;
    }

    CPacket::Free(pPacket);
}

void CMultiChatServer::ContentsOnTimer(int iThreadIndex)
{
    //unordered_map<DWORD64, st_PLAYER*>* pPlayerMap = &m_PlayerMap[iThreadIndex];
    //DWORD64 dwTime = GetTickCount64();

    //for (auto iter = pPlayerMap->begin(); iter != pPlayerMap->end(); ++iter)
    //{
    //    if (iter->second->_dwSessionID == 0)
    //        continue;

    //    // 타임아웃 처리
    //    if (dwTime > iter->second->_dwHeartbeat && dwTime - iter->second->_dwHeartbeat > 40000)
    //    {
    //        Disconnect(iter->second->_dwSessionID);
    //    }
    //}
}

void CMultiChatServer::Packet_Login(st_PLAYER* pPlayer, CPacket* pPacket)
{
    if (pPlayer->_bCanChat == true)
        return;

    *pPacket >> pPlayer->_iAccountNo;
    pPacket->GetData((char*)pPlayer->_ID, sizeof(WCHAR[20]));
    pPacket->GetData((char*)pPlayer->_Nickname, sizeof(WCHAR[20]));
    pPacket->GetData((char*)pPlayer->_SessionKey, sizeof(char[64]));

    // 로그인 응답 패킷 보내기..
    CPacket* pResPacket = CPacket::Alloc();
    *pResPacket << (WORD)en_PACKET_CS_CHAT_RES_LOGIN << (BYTE)TRUE << pPlayer->_iAccountNo;

    SendPacket_Unicast(pPlayer->_dwSessionID, pResPacket);

    CPacket::Free(pResPacket);
}

void CMultiChatServer::Packet_SectorMove(st_PLAYER* pPlayer, CPacket* pPacket)
{
    WORD wX;
    WORD wY;
    INT64 iAccountNo;

    *pPacket >> iAccountNo >> wX >> wY;

    if (wX < 0 || wY < 0 || wX >= dfSECTOR_X || wY >= dfSECTOR_Y)
        return;

    if (pPlayer->_bCanChat == true)
    {
        WORD oldX = pPlayer->_stSector._wX;
        WORD oldY = pPlayer->_stSector._wY;

        list<st_PLAYER*>* pOldList = m_SectorList[oldX][oldY].pPlayerList;
        
        AcquireSRWLockExclusive(&m_SectorList[oldX][oldY].locker);
        for (auto iter = pOldList->begin(); iter != pOldList->end(); ++iter)
        {
            if ((*iter)->_dwSessionID == pPlayer->_dwSessionID)
            {
                pOldList->erase(iter);
                break;
            }
        }
        ReleaseSRWLockExclusive(&m_SectorList[oldX][oldY].locker);
    }
    else
    {
        pPlayer->_bCanChat = true;
    }

    pPlayer->_stSector._wX = wX;
    pPlayer->_stSector._wY = wY;

    AcquireSRWLockExclusive(&m_SectorList[wX][wY].locker);
    m_SectorList[wX][wY].pPlayerList->push_back(pPlayer);
    ReleaseSRWLockExclusive(&m_SectorList[wX][wY].locker);

    CPacket* pResPacket = CPacket::Alloc();
    *pResPacket << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << iAccountNo << wX << wY;

    SendPacket_Unicast(pPlayer->_dwSessionID, pResPacket);

    CPacket::Free(pResPacket);
}

void CMultiChatServer::Packet_Chat(st_PLAYER* pPlayer, CPacket* pPacket)
{
    list<st_PLAYER*>* pExistList;
    st_SECTOR_AROUND* pAround = &m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].stAround;

    INT64 iAccountNo;
    WORD wChatLen;
    WCHAR wchChat[dfMAX_CHAT_LEN];

    *pPacket >> iAccountNo >> wChatLen;
    pPacket->GetData((char*)wchChat, wChatLen);

    CPacket* pResPacket = CPacket::Alloc();
    *pResPacket << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE << iAccountNo;
    pResPacket->PutData((char*)pPlayer->_ID, sizeof(pPlayer->_ID));
    pResPacket->PutData((char*)pPlayer->_Nickname, sizeof(pPlayer->_Nickname));
    *pResPacket << wChatLen;
    pResPacket->PutData((char*)wchChat, wChatLen);

    vector<DWORD64> vecSessionID;
    for (int i = 0; i < pAround->iCount; ++i)
    {
        pExistList = m_SectorList[pAround->Around[i]._wX][pAround->Around[i]._wY].pPlayerList;

        AcquireSRWLockShared(&m_SectorList[pAround->Around[i]._wX][pAround->Around[i]._wY].locker);
        for (auto iter = pExistList->begin(); iter != pExistList->end(); ++iter)
        {
            vecSessionID.push_back((*iter)->_dwSessionID);
        }
        ReleaseSRWLockShared(&m_SectorList[pAround->Around[i]._wX][pAround->Around[i]._wY].locker);
    }

    SendPacket_Multicast(&vecSessionID, pPlayer->_dwSessionID, pResPacket);
    CPacket::Free(pResPacket);
}

void CMultiChatServer::Packet_Heartbeat(st_PLAYER* pPlayer, CPacket* pPacket)
{
}