#pragma once

#include "01. Library/CNetServer.h"

#define dfSECTOR_X		(50)
#define dfSECTOR_Y		(50)

#define dfMAX_CHAT_LEN	(200)

class CMultiChatServer_IOThread final : public CNetServer
{
public:
	CMultiChatServer_IOThread(st_SERVER_INFO* stServerInfo, st_DB_INFO* stDBInfo);
	virtual ~CMultiChatServer_IOThread();

	bool OnConnectionRequest(unsigned long szIp, unsigned short usPort) override;
	bool OnClientJoin(DWORD64 dwSessionID) override;
	void OnClientLeave(DWORD64 dwSessionID) override;
	void OnRecv(DWORD64 dwSessionID, CPacket* pPacket) override;
	void OnError(DWORD64 dwSessionID, int iErrorCode, WCHAR* szError) override;
	void OnTimer() override;
	void OnMonitoring(st_LIB_MONITORING* pLibMonitoring) override;

	bool Start() override;

private:
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
		bool		_bCanChat;			// 채팅 가능 여부 (로그인 + 섹터 업데이트 완료 된 플레이어)
	};

	struct st_SECTOR_INFO
	{
		st_SECTOR_AROUND	stAround;
		list<st_PLAYER*>*	pPlayerList;
		SRWLOCK				locker;
	};

private:
	static void __stdcall ContentsOnClientLeave(LPVOID arg);

	st_PLAYER* FindPlayerOrNull(DWORD64 dwSessionID);

	void Packet_Login(st_PLAYER* pPlayer, CPacket* pPacket);
	void Packet_SectorMove(st_PLAYER* pPlayer, CPacket* pPacket);
	void Packet_Chat(st_PLAYER* pPlayer, CPacket* pPacket);
	void Packet_Heartbeat(st_PLAYER* pPlayer, CPacket* pPacket);

	void SetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround);

private:
	CLFMemoryPool<st_PLAYER>*					m_PlayerPool;
	st_SECTOR_INFO								m_SectorList[dfSECTOR_X][dfSECTOR_Y];	// 자신 주변 9섹터 정보와 해당 섹터 플레이어를 가지고 있는 섹터 리스트

	unordered_map<DWORD64, st_PLAYER*>*			m_PlayerMap;							// 실질적으로 채팅이 가능한 플레이어 맵
	SRWLOCK										m_PlayerMapLocker;

	CLFQueue<st_PLAYER*>*						m_LeaveJobQ;
	HANDLE										m_hLeaveEvent;
	HANDLE										m_hLeaveThread;

	alignas(64) int								m_iUpdateTPS;
	alignas(64) int								m_iContentsThreadsCount;
};