#include "CEchoServer.h"
#include "CPacket.h"

CEchoServer::CEchoServer()
    : CLanServer(10000)
{
}

CEchoServer::~CEchoServer()
{
}

bool CEchoServer::OnConnectionRequest(unsigned long szIp, unsigned short usPort)
{
    return true;
}

bool CEchoServer::OnClientJoin(UINT64 dwSessionID)
{
    CPacket* sendPacket = CPacket::Alloc();
    *sendPacket << 0x7fffffffffffffff;
    SendPacket(dwSessionID, sendPacket);

    CPacket::Free(sendPacket);

    return true;
}

void CEchoServer::OnClientLeave()
{
}

void CEchoServer::OnRecv(UINT64 dwSessionID, CPacket* pPacket)
{
    CPacket* sendPacket = CPacket::Alloc();
    
    sendPacket->PutData(pPacket->GetReadBufferPtr(), pPacket->GetDataSize());
    SendPacket(dwSessionID, sendPacket);

    CPacket::Free(sendPacket);
}

void CEchoServer::OnError(int iErrorCode, WCHAR* szError)
{
}
