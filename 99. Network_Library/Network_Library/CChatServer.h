#pragma once
#include <unordered_map>
#include "CNetServer.h"

#define dfSECTOR_X		(50)
#define dfSECTOR_Y		(50)

#define dfMAX_CHAT_LEN	(200)

using namespace procademy;

class CChatServer final : public CNetServer
{
public:
    CChatServer();
    virtual ~CChatServer();

	bool OnConnectionRequest(unsigned long szIp, unsigned short usPort) override;
	bool OnClientJoin(DWORD64 dwSessionID) override;
	void OnClientLeave(DWORD64 dwSessionID) override;
	void OnRecv(DWORD64 dwSessionID, CPacket* pPacket) override;
	void OnError(DWORD64 dwSessionID, int iErrorCode, WCHAR* szError) override;
	void OnTimer() override;
	void OnMonitoring() override;

	bool Start(const WCHAR* szIp, unsigned short usPort, int iWorkerThreadCount, int iRunningThreadCount, bool bNagle, int iMaxUserCount, unsigned char uchPacketCode, unsigned char uchPacketKey, int iTimeout) override;

private:
	union SessionInfo
	{
		DWORD64						dwSessionID;
		DWORD64						dwTime;
	};

	struct st_JOB
	{
		CNetServer::eFuncHandler	eHandle;
		SessionInfo					uSessionInfo;
		CPacket*					pPacket;
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
		bool		_bCanChat;			// ä�� ���� ���� (�α��� + ���� ������Ʈ �Ϸ� �� �÷��̾�)
	};

private:
	static void __stdcall Contents(CChatServer* pThis);

	void SetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround);
	void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround);

private:
	CLFMemoryPool<st_JOB>*						m_JobPool;
	CLFQueue<st_JOB*>*							m_JobQ;
	CLFMemoryPool<st_PLAYER>*					m_PlayerPool;
	unordered_map<DWORD64, st_PLAYER*>*			m_PlayerMap;							// ���������� ä���� ������ �÷��̾� ��
	pair<st_SECTOR_AROUND, list<st_PLAYER*>>	m_SectorList[dfSECTOR_X][dfSECTOR_Y];	// �ڽ� �ֺ� 9���� ������ �ش� ���� �÷��̾ ������ �ִ� ���� ����Ʈ

	HANDLE										m_hContentsThread;
	HANDLE										m_hEvent;								// ������ ������ ����� �̺�Ʈ

	alignas(64) int								m_iUpdateTPS;
};

