#include <process.h>

#include "CChatServer.h"
#include "CommonProtocol.h"

CChatServer::CChatServer()
    : CNetServer(100000)
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
            SetSectorAround(i, j, &m_SectorList[i][j].first);
        }
    }

    // 컨텐츠 쓰레드 제어용 event 생성
    m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

CChatServer::~CChatServer()
{
    delete m_PlayerPool;
    delete m_JobPool;
    delete m_JobQ;
    delete m_PlayerMap;
}

bool CChatServer::OnConnectionRequest(unsigned long szIp, unsigned short usPort)
{
    return true;
}

bool CChatServer::OnClientJoin(DWORD64 dwSessionID)
{
    st_JOB* stJob = m_JobPool->Alloc();
    stJob->eHandle = CNetServer::eFuncHandler::OnClientJoin;
    stJob->uSessionInfo.dwSessionID = dwSessionID;

    m_JobQ->Enqueue(stJob);
    SetEvent(m_hEvent);

    return true;
}

void CChatServer::OnClientLeave(DWORD64 dwSessionID)
{
    st_JOB* stJob = m_JobPool->Alloc();
    stJob->eHandle = CNetServer::eFuncHandler::OnClientLeave;
    stJob->uSessionInfo.dwSessionID = dwSessionID;

    m_JobQ->Enqueue(stJob);
    SetEvent(m_hEvent);
}

void CChatServer::OnRecv(DWORD64 dwSessionID, CPacket* pPacket)
{
    st_JOB* stJob = m_JobPool->Alloc();
    stJob->eHandle = CNetServer::eFuncHandler::OnRecv;
    stJob->uSessionInfo.dwSessionID = dwSessionID;
    stJob->pPacket = pPacket;

    pPacket->AddRefCount();
    m_JobQ->Enqueue(stJob);
    SetEvent(m_hEvent);
}

void CChatServer::OnError(DWORD64 dwSessionID, int iErrorCode, WCHAR* szError)
{

}

void CChatServer::OnTimer()
{
    /*st_JOB* stJob = m_JobPool->Alloc();
    stJob->eHandle = CNetServer::eFuncHandler::OnTimer;
    stJob->uSessionInfo.dwTime = dwTime;

    m_JobQ->Enqueue(stJob);
    SetEvent(m_hEvent);*/

    DWORD64 dwTime = GetTickCount64();

    for (auto iter = m_PlayerMap->begin(); iter != m_PlayerMap->end(); ++iter)
    {
        // 타임아웃 처리
        if (dwTime > iter->second->_dwHeartbeat && dwTime - iter->second->_dwHeartbeat > 15000)
        {
            Disconnect(iter->second->_dwSessionID);
        }
    }

    m_iUpdateTPS = 0;
}

void CChatServer::OnMonitoring()
{
    printf("============================================================================================\n");

    printf("%30s : %d\n", "Session Count", m_stMonitoring.iSessionCount);
    printf("%30s : %d\n", "Packet Pool Alloc Size", m_stMonitoring.iPacketPoolAllocSize);
    printf("\n");
    printf("%30s : %d\n", "Update Job Pool Alloc Size", m_JobPool->GetCapacity());
    printf("%30s : %d\n", "Update Job Queue Size", m_JobQ->GetSize());
    printf("\n");
    printf("%30s : %d\n", "Player Pool Alloc Size", m_PlayerPool->GetCapacity());
    printf("%30s : %d\n", "Player Count", m_PlayerPool->GetUseCount());
    printf("\n");
    printf("%30s : %d\n", "Accpet Total", m_stMonitoring.iAcceptTotal);
    printf("%30s : %d\n", "Accept TPS", m_stMonitoring.iAcceptTPS);
    printf("%30s : %d\n", "Update TPS", m_iUpdateTPS);
    printf("\n");
    printf("%30s : %d\n", "Recv Message TPS", m_stMonitoring.iRecvTPS);
    printf("%30s : %d\n", "Send Message TPS", m_stMonitoring.iSendTPS);

    printf("============================================================================================\n\n");
}

bool CChatServer::Start(const WCHAR* szIp, unsigned short usPort, int iWorkerThreadCount, int iRunningThreadCount, bool bNagle, int iMaxUserCount, unsigned char uchPacketCode, unsigned char uchPacketKey, int iTimeout)
{
    CNetServer::Start(szIp, usPort, iWorkerThreadCount, bNagle, iMaxUserCount, iRunningThreadCount, uchPacketCode, uchPacketKey, iTimeout);

    m_hContentsThread = (HANDLE)_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)Contents, this, 0, nullptr);
    if (m_hContentsThread == NULL)
        return 1;

    WaitForSingleObject(m_hContentsThread, INFINITE);
}

void __stdcall CChatServer::Contents(CChatServer* pThis)
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
                DWORD64 dwSessionID = stJob->uSessionInfo.dwSessionID;

                // 채팅 가능한 플레이어 맵에서 찾아보기 -> 있으면 아직 leave 안된 것임
                auto iter = pThis->m_PlayerMap->find(dwSessionID);
                if (iter != pThis->m_PlayerMap->end())
                {
                    st_JOB* stJob = pThis->m_JobPool->Alloc();
                    stJob->eHandle = CNetServer::eFuncHandler::OnClientJoin;
                    stJob->uSessionInfo.dwSessionID = dwSessionID;

                    pThis->m_JobQ->Enqueue(stJob);

                    break;
                }

                st_PLAYER* pPlayer = pThis->m_PlayerPool->Alloc();

                pPlayer->_dwSessionID = dwSessionID;
                pPlayer->_iAccountNo = 0;
                pPlayer->_bCanChat = false;
                pPlayer->_dwHeartbeat = GetTickCount64();

                pThis->m_PlayerMap->insert(make_pair(dwSessionID, pPlayer));
            }
                break;
            case CNetServer::eFuncHandler::OnClientLeave:
            {
                auto iter = pThis->m_PlayerMap->find(stJob->uSessionInfo.dwSessionID);
                if (iter == pThis->m_PlayerMap->end())
                    break;

                st_PLAYER* pPlayer = iter->second;
                
                if (pPlayer->_bCanChat == true)
                {
                    list<st_PLAYER*>* pExistList = &pThis->m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].second;

                    for (auto iter = pExistList->begin(); iter != pExistList->end(); ++iter)
                    {
                        if ((*iter)->_dwSessionID == pPlayer->_dwSessionID)
                        {
                            pExistList->erase(iter);
                            break;
                        }
                    }
                }

                pThis->m_PlayerMap->erase(pPlayer->_dwSessionID);
                pThis->m_PlayerPool->Free(pPlayer);
            }
                break;
            case CNetServer::eFuncHandler::OnRecv:
            {
                DWORD64 dwSessionID = stJob->uSessionInfo.dwSessionID;
                CPacket* pPacket = stJob->pPacket;
                CPacket* pResPacket;
                st_PLAYER* pPlayer;

                // 데이터 마샬링
                WORD wType;
                *pPacket >> wType;

                auto iter = pThis->m_PlayerMap->find(dwSessionID);
                if (iter == pThis->m_PlayerMap->end())
                    break;

                pPlayer = iter->second;

                pPlayer->_dwHeartbeat = GetTickCount64();

                switch (wType)
                {
                case en_PACKET_CS_CHAT_REQ_LOGIN:
                {
                    if (pPlayer->_bCanChat == true)
                        break;

                    *pPacket >> pPlayer->_iAccountNo;
                    pPacket->GetData((char*)pPlayer->_ID, sizeof(WCHAR[20]));
                    pPacket->GetData((char*)pPlayer->_Nickname, sizeof(WCHAR[20]));
                    pPacket->GetData((char*)pPlayer->_SessionKey, sizeof(char[64]));

                    // 로그인 응답 패킷 보내기..
                    pResPacket = CPacket::Alloc();
                    *pResPacket << (WORD)en_PACKET_CS_CHAT_RES_LOGIN << (BYTE)TRUE << pPlayer->_iAccountNo;

                    pThis->SendPacket_Unicast(dwSessionID, pResPacket);

                    CPacket::Free(pResPacket);
                }
                    break;
                case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
                {
                    WORD wX;
                    WORD wY;
                    INT64 iAccountNo;

                    *pPacket >> iAccountNo >> wX >> wY;

                    if (wX < 0 || wY < 0 || wX >= dfSECTOR_X || wY >= dfSECTOR_Y)
                        break;

                    if (pPlayer->_bCanChat == true)
                    {
                        list<st_PLAYER*>* pOldList = &pThis->m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].second;

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
                    pThis->m_SectorList[wX][wY].second.push_back(pPlayer);

                    pResPacket = CPacket::Alloc();
                    *pResPacket << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << iAccountNo << wX << wY;

                    pThis->SendPacket_Unicast(dwSessionID, pResPacket);

                    CPacket::Free(pResPacket);
                }
                    break;
                case en_PACKET_CS_CHAT_REQ_MESSAGE:
                {
                    list<st_PLAYER*>* pExistList;
                    st_SECTOR_AROUND* pAround = &pThis->m_SectorList[pPlayer->_stSector._wX][pPlayer->_stSector._wY].first;

                    INT64 iAccountNo;
                    WORD wChatLen;
                    WCHAR wchChat[dfMAX_CHAT_LEN];

                    *pPacket >> iAccountNo >> wChatLen;
                    pPacket->GetData((char*)wchChat, wChatLen);

                    pResPacket = CPacket::Alloc();
                    *pResPacket << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE << iAccountNo;
                    pResPacket->PutData((char*)pPlayer->_ID, sizeof(pPlayer->_ID));
                    pResPacket->PutData((char*)pPlayer->_Nickname, sizeof(pPlayer->_Nickname));
                    *pResPacket << wChatLen;
                    pResPacket->PutData((char*)wchChat, wChatLen);

                    vector<DWORD64> vecSessionID;
                    for (int i = 0; i < pAround->iCount; ++i)
                    {
                        pExistList = &pThis->m_SectorList[pAround->Around[i]._wX][pAround->Around[i]._wY].second;

                        for (auto iter = pExistList->begin(); iter != pExistList->end(); ++iter)
                        {
                            vecSessionID.push_back((*iter)->_dwSessionID);
                        }
                    }

                    pThis->SendPacket_Multicast(&vecSessionID, dwSessionID, pResPacket);
                    CPacket::Free(pResPacket);
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
            }
                break;
            case CNetServer::eFuncHandler::OnError:
            {

            }
                break;
            case CNetServer::eFuncHandler::OnTimer:
            {
                
            }
                break;
            default:
                break;
            }

            ++pThis->m_iUpdateTPS;
            pThis->m_JobPool->Free(stJob);
        }
    }
}

void CChatServer::SetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
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

void CChatServer::GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
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