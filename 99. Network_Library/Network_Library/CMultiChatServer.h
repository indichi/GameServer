#pragma once
#include "CNetServer.h"

#define dfSECTOR_X		(50)
#define dfSECTOR_Y		(50)

#define dfMAX_CHAT_LEN	(200)

class CMultiChatServer final : public CNetServer
{
public:
    CMultiChatServer(st_SERVER_INFO* stServerInfo);
    virtual ~CMultiChatServer();

	bool OnConnectionRequest(unsigned long szIp, unsigned short usPort) override;
	bool OnClientJoin(DWORD64 dwSessionID) override;
	void OnClientLeave(DWORD64 dwSessionID) override;
	void OnRecv(DWORD64 dwSessionID, CPacket* pPacket) override;
	void OnError(DWORD64 dwSessionID, int iErrorCode, WCHAR* szError) override;
	void OnTimer() override;
	void OnMonitoring(st_LIB_MONITORING* pLibMonitoring) override;

	bool Start() override;

private:
	struct st_JOB
	{
		CNetServer::eFuncHandler	eHandle;
		DWORD64						dwSessionID;
		CPacket*					pPacket;
	};

	struct st_THREAD_ARGS
	{
		CMultiChatServer*	pThis;
		int					iMyIndex;
	};

	struct st_SECTOR
	{
		WORD	_wX;
		WORD	_wY;
	};

	struct st_SECTOR_AROUND
	{
		int			iCount;
		st_SECTOR	Around[9];
	};

	struct st_PLAYER
	{
		DWORD64		_dwSessionID;
		INT64		_iAccountNo;
		WCHAR		_ID[20];
		WCHAR		_Nickname[20];
		char		_SessionKey[64];

		st_SECTOR	_stSector;

		DWORD64		_dwHeartbeat;
		bool		_bCanChat;			// 채팅 가능 여부 (로그인 + 섹터 업데이트 완료 된 플레이어)
	};

	struct st_SECTOR_INFO
	{
		st_SECTOR_AROUND	stAround;
		list<st_PLAYER*>*	pPlayerList;
		SRWLOCK				locker;
	};

private:
	bool ContentsOnClientJoin(DWORD64 dwSessionID);
	bool ContentsOnClientLeave(DWORD64 dwSessionID);
	void ContentsOnRecv(st_JOB* stJob);
	void ContentsOnTimer(int iThreadIndex);

	void Packet_Login(st_PLAYER* pPlayer, CPacket* pPacket);
	void Packet_SectorMove(st_PLAYER* pPlayer, CPacket* pPacket);
	void Packet_Chat(st_PLAYER* pPlayer, CPacket* pPacket);
	void Packet_Heartbeat(st_PLAYER* pPlayer, CPacket* pPacket);

private:
	static void __stdcall Contents(LPVOID args);

	void SetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround);

private:
	CLFMemoryPool<st_JOB>*						m_JobPool;
	CLFMemoryPool<st_PLAYER>*					m_PlayerPool;
	st_SECTOR_INFO								m_SectorList[dfSECTOR_X][dfSECTOR_Y];	// 자신 주변 9섹터 정보와 해당 섹터 플레이어를 가지고 있는 섹터 리스트

	HANDLE*										m_hContentsThread;
	CLFQueue<st_JOB*>*							m_JobQ;
	unordered_map<DWORD64, st_PLAYER*>*			m_PlayerMap;							// 실질적으로 채팅이 가능한 플레이어 맵
	
	HANDLE*										m_hEvent;								// 컨텐츠 쓰레드 제어용 이벤트

	alignas(64) int								m_iUpdateTPS;
	alignas(64) int								m_iContentsThreadsCount;
};