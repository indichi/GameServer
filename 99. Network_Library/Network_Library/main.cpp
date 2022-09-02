//#define _SINGLE

#ifdef _SINGLE
#include "CSingleChatServer.h"
#else
#include "CMultiChatServer.h"
#endif

#include "CTextParser.h"


CTextParser g_Config;

int main(void)
{
    /*CNetServer::st_SESSION p;

    p.bReleaseFlag = 0;
    p.IO_Count = 0;

    long long cmp[2] = { 0 , 0 };

    if (InterlockedCompareExchange128(&p.bReleaseFlag, 10, 20, cmp) == TRUE)
        int a = 0;*/

    /*DWORD c[2] = { 1, 0 };
    LONG64 p = *(LONG64*)&c[0];
    
    if (InterlockedCompareExchange64((LONG64*)c, TRUE, FALSE) != FALSE)
        int a = 0;*/

    CNetServer::st_SERVER_INFO stServerInfo;

    g_Config.LoadFile(L"Config//ChatServerConfig.txt");
    
    g_Config.GetData(L"IP", stServerInfo.szIp);
    g_Config.GetData(L"PORT", &stServerInfo.usPort);
    g_Config.GetData(L"IOThreadCnt", &stServerInfo.iWorkerThreadCount);
    g_Config.GetData(L"RunningThreadCnt", &stServerInfo.iRunningThreadCount);
    g_Config.GetData(L"Nagle", &stServerInfo.bNagle);
    g_Config.GetData(L"SessionMaxCnt", &stServerInfo.iMaxSessionCount);
    g_Config.GetDataHex(L"PacketCode", &stServerInfo.uchPacketCode);
    g_Config.GetDataHex(L"PacketKey", &stServerInfo.uchPacketKey);
    g_Config.GetData(L"Timeout", &stServerInfo.iTimeout);
    g_Config.GetData(L"ContentsThreadCnt", &stServerInfo.iContentsThreadCount);

#ifdef _SINGLE
    CSingleChatServer* pChatServer = new CSingleChatServer(&stServerInfo);
#else
    CMultiChatServer* pChatServer = new CMultiChatServer(&stServerInfo);
#endif
    pChatServer->Start();

    delete pChatServer;

    return 0;
}