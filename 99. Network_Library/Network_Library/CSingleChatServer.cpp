#include <process.h>

#include "CSingleChatServer.h"
#include "CommonProtocol.h"

CSingleChatServer::CSingleChatServer(st_SERVER_INFO* stStartInfo, st_DB_INFO* stDBInfo)
    : CNetServer(stStartInfo, stDBInfo)
    , m_JobPool(new CLFMemoryPool<st_JOB>)
    , m_JobQ(new CLFQueue<st_JOB*>)
    , m_PlayerPool(new CLFMemoryPool<st_PLAYER>)
    , m_PlayerMap(new unordered_map<UINT64, st_PLAYER*>)
    , m_SectorList()
    , m_hContentsThread(INVALID_HANDLE_VALUE)
    , m_hEvent(INVALID_HANDLE_VALUE)
    , m_iUpdateTPS(0)

{
    // 섹터 리스트, Around 설정
    for (int i = 0; i < dfSECTOR_X; ++i)
    {
        for (int j = 0; j < dfSECTOR_Y; ++j)
        {
            m_SectorList[i][j].second = new list<st_PLAYER*>;
            SetSectorAround(i, j, &m_SectorList[i][j].first);
        }
    }

    // 컨텐츠 쓰레드 제어용 event 생성
    m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

CSingleChatServer::~CSingleChatServer()
{
    delete m_PlayerPool;
    delete m_JobPool;
    delete m_JobQ;
    delete m_PlayerMap;

    for (int i = 0; i < dfSECTOR_X; ++i)
    {
        for (int j = 0; j < dfSECTOR_Y; ++j)
        {
            delete m_SectorList[i][j].second;
        }
    }
}

bool CSingleChatServer::OnConnectionRequest(unsigned long szIp, unsigned short usPort)
{
    return true;
}

bool CSingleChatServer::OnClientJoin(DWORD64 dwSessionID)
{
    st_JOB* stJob = m_JobPool->Alloc();
    stJob->eHandle = CNetServer::eFuncHandler::OnClientJoin;
    stJob->dwSessionID = dwSessionID;

    m_JobQ->Enqueue(stJob);
    SetEvent(m_hEvent);

    return true;
}

void CSingleChatServer::OnClientLeave(DWORD64 dwSessionID)
{
    st_JOB* stJob = m_JobPool->Alloc();
    stJob->eHandle = CNetServer::eFuncHandler::OnClientLeave;
    stJob->dwSessionID = dwSessionID;

    m_JobQ->Enqueue(stJob);
    SetEvent(m_hEvent);
}

void CSingleChatServer::OnRecv(DWORD64 dwSessionID, CPacket* pPacket)
{
    st_JOB* stJob = m_JobPool->Alloc();
    stJob->eHandle = CNetServer::eFuncHandler::OnRecv;
    stJob->dwSessionID = dwSessionID;
    stJob->pPacket = pPacket;

    pPacket->AddRefCount();
    m_JobQ->Enqueue(stJob);
    SetEvent(m_hEvent);
}

void CSingleChatServer::OnError(DWORD64 dwSessionID, int iErrorCode, WCHAR* szError)
{

}

void CSingleChatServer::OnTimer()
{
    m_iUpdateTPS = 0;

    st_JOB* stJob = m_JobPool->Alloc();
    stJob->eHandle = CNetServer::eFuncHandler::OnTimer;

    m_JobQ->Enqueue(stJob);
    SetEvent(m_hEvent);
}

void CSingleChatServer::OnMonitoring(st_LIB_MONITORING* pLibMonitoring)
{
    printf("============================================================================================\n");

    printf("%30s : %d\n", "Session Count", pLibMonitoring->iSessionCount);
    printf("%30s : %d\n", "Packet Pool Alloc Size", CPacket::GetPacketPoolCapacity());
    //printf("%30s : %d\n", "Packet Pool Use Count", CPacket::GetPacketPoolUseCount());
    printf("\n");
    printf("%30s : %d\n", "Update Job Pool Alloc Size", m_JobPool->GetCapacity());
    printf("%30s : %d\n", "Update Job Queue Size", m_JobQ->GetSize());
    printf("\n");
    printf("%30s : %d\n", "Player Pool Alloc Size", m_PlayerPool->GetCapacity());
    printf("%30s : %d\n", "Player Count", m_PlayerPool->GetUseCount());
    printf("\n");
    printf("%30s : %d\n", "Accpet Total", pLibMonitoring->iAcceptTotal);
    printf("%30s : %d\n", "Accept TPS", pLibMonitoring->iAcceptTPS);
    printf("%30s : %d\n", "Update TPS", m_iUpdateTPS);
    printf("\n");
    printf("%30s : %d\n", "Recv Message TPS", pLibMonitoring->iRecvTPS);
    printf("%30s : %d\n", "Send Message TPS", pLibMonitoring->iSendTPS);

    printf("============================================================================================\n\n");
}

bool CSingleChatServer::Start()
{
    m_hContentsThread = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)Contents, this, 0, nullptr);
    if (m_hContentsThread == NULL)
        return 1;

    CNetServer::Start();

    WaitForSingleObject(m_hContentsThread, INFINITE);

    return true;
}

void CSingleChatServer::ContentsOnClientJoin(DWORD64 dwSessionID)
{
    // 채팅 가능한 플레이어 맵에서 찾아보기 -> 있으면 아직 leave 안된 것임
    auto iter = m_PlayerMap->find(dwSessionID);
    if (iter != m_PlayerMap->end())
    {
        st_JOB* stJob = m_JobPool->Alloc();
        stJob->eHandle = CNetServer::eFuncHandler::OnClientJoin;
        stJob->dwSessionID = dwSessionID;

        m_JobQ->Enqueue(stJob);

        return;
    }

    st_PLAYER* pPlayer = m_PlayerPool->Alloc();

    pPlayer->_dwSessionID = dwSessionID;
    pPlayer->_iAccountNo = 0;
    pPlayer->_bCanChat = false;
    pPlayer->_dwHeartbeat = GetTickCount64();

    m_PlayerMap->insert(make_pair(dwSessionID, pPlayer));
}

void CSingleChatServer::ContentsOnClientLeave(DWORD64 dwSessionID)
{
    auto iter = m_PlayerMap->find(dwSessionID);
    if (iter == m_PlayerMap->end())
        return;     // TODO add log

    st_PLAYER* pPlayer = iter->second;

    if (pPlayer->_bCanChat == true)
    {
        list<st_PLAYER*>* pExistList = m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].second;

        for (auto iter = pExistList->begin(); iter != pExistList->end(); ++iter)
        {
            if ((*iter)->_dwSessionID == pPlayer->_dwSessionID)
            {
                pExistList->erase(iter);
                break;
            }
        }
    }

    m_PlayerMap->erase(pPlayer->_dwSessionID);
    m_PlayerPool->Free(pPlayer);
}

void CSingleChatServer::ContentsOnRecv(st_JOB* stJob)
{
    DWORD64 dwSessionID = stJob->dwSessionID;
    CPacket* pPacket = stJob->pPacket;
    st_PLAYER* pPlayer;

    // 데이터 마샬링
    WORD wType;
    *pPacket >> wType;

    auto iter = m_PlayerMap->find(dwSessionID);
    if (iter == m_PlayerMap->end())
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

void CSingleChatServer::ContentsOnTimer()
{
    DWORD64 dwTime = GetTickCount64();

    for (auto iter = m_PlayerMap->begin(); iter != m_PlayerMap->end(); ++iter)
    {
        if (iter->second->_dwSessionID == 0)
            continue;

        // 타임아웃 처리
        if (dwTime > iter->second->_dwHeartbeat && dwTime - iter->second->_dwHeartbeat > 40000)
        {
            Disconnect(iter->second->_dwSessionID);
        }
    }
}

void CSingleChatServer::Packet_Login(st_PLAYER* pPlayer, CPacket* pPacket)
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

void CSingleChatServer::Packet_SectorMove(st_PLAYER* pPlayer, CPacket* pPacket)
{
    WORD wX;
    WORD wY;
    INT64 iAccountNo;

    *pPacket >> iAccountNo >> wX >> wY;

    if (wX < 0 || wY < 0 || wX >= dfSECTOR_X || wY >= dfSECTOR_Y)
        return;

    if (pPlayer->_bCanChat == true)
    {
        list<st_PLAYER*>* pOldList = m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].second;

        for (auto iter = pOldList->begin(); iter != pOldList->end(); ++iter)
        {
            if ((*iter)->_dwSessionID == pPlayer->_dwSessionID)
            {
                pOldList->erase(iter);
                break;
            }
        }
    }
    else
    {
        pPlayer->_bCanChat = true;
    }

    pPlayer->_stSector._wX = wX;
    pPlayer->_stSector._wY = wY;
    m_SectorList[wX][wY].second->push_back(pPlayer);

    CPacket* pResPacket = CPacket::Alloc();
    *pResPacket << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << iAccountNo << wX << wY;

    SendPacket_Unicast(pPlayer->_dwSessionID, pResPacket);

    CPacket::Free(pResPacket);
}

void CSingleChatServer::Packet_Chat(st_PLAYER* pPlayer, CPacket* pPacket)
{
    list<st_PLAYER*>* pExistList;
    st_SECTOR_AROUND* pAround = &m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].first;

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
        pExistList = m_SectorList[pAround->Around[i]._wX][pAround->Around[i]._wY].second;

        for (auto iter = pExistList->begin(); iter != pExistList->end(); ++iter)
        {
            vecSessionID.push_back((*iter)->_dwSessionID);
        }
    }

    SendPacket_Multicast(&vecSessionID, pPlayer->_dwSessionID, pResPacket);
    CPacket::Free(pResPacket);
}

void CSingleChatServer::Packet_Heartbeat(st_PLAYER* pPlayer, CPacket* pPacket)
{
}

void __stdcall CSingleChatServer::Contents(CSingleChatServer* pThis)
{
    st_JOB* stJob;

    while (1)
    {
        WaitForSingleObject(pThis->m_hEvent, INFINITE);

        while (pThis->m_JobQ->GetSize() > 0)
        {
            pThis->m_JobQ->Dequeue(&stJob);

            switch (stJob->eHandle)
            {
            case CNetServer::eFuncHandler::OnConnectionRequest:
                break;
            case CNetServer::eFuncHandler::OnClientJoin:
            {
                pThis->ContentsOnClientJoin(stJob->dwSessionID);
            }
                break;
            case CNetServer::eFuncHandler::OnClientLeave:
            {
                pThis->ContentsOnClientLeave(stJob->dwSessionID);
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
                pThis->ContentsOnTimer();
            }
                break;
            default:
                break;
                // TODO add log
            }

            ++pThis->m_iUpdateTPS;
            pThis->m_JobPool->Free(stJob);
        }
    }
}

void CSingleChatServer::SetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
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

void CSingleChatServer::GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
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