//#include "CEchoServer.h"
#include "CChatServer.h"
#include "CTextParser.h"


CTextParser g_Config;
//CEchoServer g_Server;
CChatServer g_Server;

int main(void)
{
    WCHAR IP[20];
    int iPort;
    int iIOThreadCnt;
    int iRunningThreadCnt;
    int iSessionMaxCnt;
    int iTimeout;
    bool bNagle;

    unsigned char uchPacketCode;
    unsigned char uchPacketKey;

    g_Config.LoadFile(L"Config//ChatServerConfig.txt");
    
    g_Config.GetData(L"IP", IP);
    g_Config.GetData(L"PORT", &iPort);
    g_Config.GetData(L"IOThreadCnt", &iIOThreadCnt);
    g_Config.GetData(L"RunningThreadCnt", &iRunningThreadCnt);
    g_Config.GetData(L"Nagle", &bNagle);
    g_Config.GetData(L"SessionMaxCnt", &iSessionMaxCnt);
    g_Config.GetDataHex(L"PacketCode", &uchPacketCode);
    g_Config.GetDataHex(L"PacketKey", &uchPacketKey);
    g_Config.GetData(L"Timeout", &iTimeout);

    g_Server.Start(IP, iPort, iIOThreadCnt, bNagle, iSessionMaxCnt, iRunningThreadCnt, uchPacketCode, uchPacketKey, iTimeout);
    //g_Server.Start(IP, iPort, iIOThreadCnt, bNagle, iSessionMaxCnt);

    return 0;
}