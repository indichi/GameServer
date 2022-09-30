#include <process.h>

#include "CMultiChatServer_IOThread.h"
#include "CommonProtocol.h"

CMultiChatServer_IOThread::CMultiChatServer_IOThread(st_SERVER_INFO* stServerInfo, st_DB_INFO* stDBInfo)
    : CNetServer(stServerInfo, stDBInfo)
    , m_PlayerPool(new CLFMemoryPool<st_PLAYER>)
    , m_SectorList{}
    , m_iContentsThreadsCount(stServerInfo->iContentsThreadCount)
    , m_PlayerMap(new unordered_map<DWORD64, st_PLAYER*>)
    , m_iUpdateTPS(0)
    , m_LeaveJobQ(new CLFQueue<st_PLAYER*>)
{
    InitializeSRWLock(&m_PlayerMapLocker);

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
}

CMultiChatServer_IOThread::~CMultiChatServer_IOThread()
{
    for (int i = 0; i < dfSECTOR_X; ++i)
    {
        for (int j = 0; j < dfSECTOR_Y; ++j)
        {
            delete m_SectorList[i][j].pPlayerList;
        }
    }

    CloseHandle(m_hLeaveEvent);
    CloseHandle(m_hLeaveThread);
    delete m_LeaveJobQ;

    delete m_PlayerMap;
    delete m_PlayerPool;
}

bool CMultiChatServer_IOThread::OnConnectionRequest(unsigned long szIp, unsigned short usPort)
{
    return true;
}

bool CMultiChatServer_IOThread::OnClientJoin(DWORD64 dwSessionID)
{
    // Join인데 있으면 이상한거임
    st_PLAYER* pPlayer = FindPlayerOrNull(dwSessionID);
    if (pPlayer != nullptr)
        return false;

    pPlayer = m_PlayerPool->Alloc();

    pPlayer->_dwSessionID = dwSessionID;
    pPlayer->_iAccountNo = 0;
    pPlayer->_bCanChat = false;

    AcquireSRWLockExclusive(&m_PlayerMapLocker);
    m_PlayerMap->insert(make_pair(dwSessionID, pPlayer));
    ReleaseSRWLockExclusive(&m_PlayerMapLocker);

    InterlockedIncrement((long*)&m_iUpdateTPS);

    return true;
}

void CMultiChatServer_IOThread::OnClientLeave(DWORD64 dwSessionID)
{
    st_PLAYER* pPlayer = FindPlayerOrNull(dwSessionID);
    if (pPlayer == nullptr)
        return;

    m_LeaveJobQ->Enqueue(pPlayer);
    SetEvent(m_hLeaveEvent);
}

void CMultiChatServer_IOThread::OnRecv(DWORD64 dwSessionID, CPacket* pPacket)
{
    st_PLAYER* pPlayer = FindPlayerOrNull(dwSessionID);
    if (pPlayer == nullptr)
        return;

    // 데이터 마샬링
    WORD wType;
    *pPacket >> wType;

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

    }
    break;
    default:
        // TODO 이상한 메세지 타입은 로그 남겨야 함..
        break;
    }

    CPacket::Free(pPacket);

    InterlockedIncrement((long*)&m_iUpdateTPS);
}

void CMultiChatServer_IOThread::OnError(DWORD64 dwSessionID, int iErrorCode, WCHAR* szError)
{
}

void CMultiChatServer_IOThread::OnTimer()
{
    m_iUpdateTPS = 0;
}

void CMultiChatServer_IOThread::OnMonitoring(st_LIB_MONITORING* pLibMonitoring)
{
    int iPlayerMapSize = (int)m_PlayerMap->size();

    int iPacketPoolCapacity = CPacket::GetPacketPoolCapacity();
    int iPlayerPoolCapacity = m_PlayerPool->GetCapacity();
    int iPlayerPoolUseCount = m_PlayerPool->GetUseCount();

    printf("============================================================================================\n");

    printf("%30s : %d\n", "Session Count", pLibMonitoring->iSessionCount);
    printf("%30s : %d\n", "Packet Pool Alloc Size", iPacketPoolCapacity);
    printf("\n");

    printf("%30s : %d\n", "Player Pool Alloc Size", iPlayerPoolCapacity);
    printf("%30s : %d\n", "Player Pool Use Size", iPlayerPoolUseCount);
    printf("%30s : %d\n", "Player Count", iPlayerMapSize);
    printf("\n");

    printf("%30s : %d\n", "Accpet Total", pLibMonitoring->iAcceptTotal);
    printf("%30s : %d\n", "Accept TPS", pLibMonitoring->iAcceptTPS);
    printf("%30s : %d\n", "Update TPS", m_iUpdateTPS);
    printf("\n");

    printf("%30s : %d\n", "Recv Message TPS", pLibMonitoring->iRecvTPS);
    printf("%30s : %d\n", "Send Message TPS", pLibMonitoring->iSendTPS);

    printf("============================================================================================\n\n");
}

bool CMultiChatServer_IOThread::Start()
{
    m_hLeaveEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hLeaveThread = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)ContentsOnClientLeave, this, NULL, NULL);

    CNetServer::Start();

    return true;
}

void CMultiChatServer_IOThread::SetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
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

void __stdcall CMultiChatServer_IOThread::ContentsOnClientLeave(LPVOID arg)
{
    CMultiChatServer_IOThread* pThis = (CMultiChatServer_IOThread*)arg;

    while (1)
    {
        WaitForSingleObject(pThis->m_hLeaveEvent, INFINITE);

        while (pThis->m_LeaveJobQ->GetSize() > 0)
        {
            //PROFILE_BEGIN(L"Processing Contents");
            st_PLAYER* pPlayer;
            pThis->m_LeaveJobQ->Dequeue(&pPlayer);

            if (pPlayer->_bCanChat == true)
            {
                list<st_PLAYER*>* pExistList = pThis->m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].pPlayerList;

                AcquireSRWLockExclusive(&pThis->m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].locker);
                for (auto iter = pExistList->begin(); iter != pExistList->end(); ++iter)
                {
                    if ((*iter)->_dwSessionID == pPlayer->_dwSessionID)
                    {
                        pExistList->erase(iter);
                        break;
                    }
                }
                ReleaseSRWLockExclusive(&pThis->m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].locker);
            }

            AcquireSRWLockExclusive(&pThis->m_PlayerMapLocker);
            pThis->m_PlayerMap->erase(pPlayer->_dwSessionID);
            ReleaseSRWLockExclusive(&pThis->m_PlayerMapLocker);

            pThis->m_PlayerPool->Free(pPlayer);
            //PROFILE_END(L"Processing Contents");
        }

        InterlockedIncrement((long*)&pThis->m_iUpdateTPS);
    }
}

CMultiChatServer_IOThread::st_PLAYER* CMultiChatServer_IOThread::FindPlayerOrNull(DWORD64 dwSessionID)
{
    AcquireSRWLockShared(&m_PlayerMapLocker);
    auto iter_end = m_PlayerMap->end();
    auto iter_find = m_PlayerMap->find(dwSessionID);
    ReleaseSRWLockShared(&m_PlayerMapLocker);

    if (iter_find == iter_end)
        return nullptr;

    return iter_find->second;
}

void CMultiChatServer_IOThread::Packet_Login(st_PLAYER* pPlayer, CPacket* pPacket)
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

void CMultiChatServer_IOThread::Packet_SectorMove(st_PLAYER* pPlayer, CPacket* pPacket)
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

void CMultiChatServer_IOThread::Packet_Chat(st_PLAYER* pPlayer, CPacket* pPacket)
{
    list<st_PLAYER*>* pExistList;
    st_SECTOR_AROUND* pAround = &m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].stAround;

    INT64 iAccountNo;
    WORD wChatLen;
    WCHAR wchChat[dfMAX_CHAT_LEN];

    vector<DWORD64> vecSessionID;

    for (int i = 0; i < pAround->iCount; ++i)
    {
        pExistList = m_SectorList[pAround->Around[i]._wX][pAround->Around[i]._wY].pPlayerList;

        AcquireSRWLockShared(&m_SectorList[pAround->Around[i]._wX][pAround->Around[i]._wY].locker);
        for (auto iter = pExistList->begin(); iter != pExistList->end(); ++iter)
        {
            vecSessionID.push_back((*iter)->_dwSessionID);
        }
        //ReleaseSRWLockShared(&m_SectorList[pAround->Around[i]._wX][pAround->Around[i]._wY].locker);
    }

    *pPacket >> iAccountNo >> wChatLen;
    pPacket->GetData((char*)wchChat, wChatLen);

    CPacket* pResPacket = CPacket::Alloc();
    *pResPacket << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE << iAccountNo;
    pResPacket->PutData((char*)pPlayer->_ID, sizeof(pPlayer->_ID));
    pResPacket->PutData((char*)pPlayer->_Nickname, sizeof(pPlayer->_Nickname));
    *pResPacket << wChatLen;
    pResPacket->PutData((char*)wchChat, wChatLen);

    SendPacket_Multicast(&vecSessionID, pPlayer->_dwSessionID, pResPacket);
    CPacket::Free(pResPacket);

    for (int i = 0; i < pAround->iCount; ++i)
    {
        ReleaseSRWLockShared(&m_SectorList[pAround->Around[i]._wX][pAround->Around[i]._wY].locker);
    }
}

void CMultiChatServer_IOThread::Packet_Heartbeat(st_PLAYER* pPlayer, CPacket* pPacket)
{
}