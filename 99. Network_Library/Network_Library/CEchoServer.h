#pragma once
#include "CLanServer.h"

class CEchoServer final : public CLanServer
{
public:
	CEchoServer();
	~CEchoServer();

	bool OnConnectionRequest(unsigned long szIp, unsigned short usPort) override;
	bool OnClientJoin(UINT64 dwSessionID) override;
	void OnClientLeave() override;
	void OnRecv(UINT64 dwSessionID, CPacket* pPacket) override;
	void OnError(int iErrorCode, WCHAR* szError) override;
private:

};