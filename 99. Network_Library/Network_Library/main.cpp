//#define _LOGIN
//#define _MULTI
#define _MULTI_IO_VER
//#define _SINGLE

#ifdef _LOGIN
#include "CLoginServer.h"
#elif defined _SINGLE
#include "CSingleChatServer.h"
#elif defined _MULTI
#include "CMultiChatServer.h"
#elif defined _MULTI_IO_VER
#include "CMultiChatServer_IOThread.h"
#endif

#include "00. Component/CTextParser.h"

CTextParser g_Config;

int main(void)
{
#ifdef _LOGIN
    CNetServer::st_SERVER_INFO stServerInfo;
    CNetServer::st_DB_INFO stDBInfo;

    g_Config.LoadFile(L"Config//LoginServerConfig.txt");

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

    g_Config.GetData(L"DBIP", stDBInfo.szDBIP);
    g_Config.GetData(L"DBUser", stDBInfo.szDBUser);
    g_Config.GetData(L"DBPW", stDBInfo.szDBPassword);
    g_Config.GetData(L"DBName", stDBInfo.szDBName);
    g_Config.GetData(L"DBPort", &stDBInfo.iDBPort);

    CLoginServer* pChatServer = new CLoginServer(&stServerInfo, &stDBInfo);
#elif defined _SINGLE
    CNetServer::st_SERVER_INFO stServerInfo;
    CNetServer::st_DB_INFO stDBInfo;

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

    g_Config.GetData(L"DBIP", stDBInfo.szDBIP);
    g_Config.GetData(L"DBUser", stDBInfo.szDBUser);
    g_Config.GetData(L"DBPW", stDBInfo.szDBPassword);
    g_Config.GetData(L"DBName", stDBInfo.szDBName);
    g_Config.GetData(L"DBPort", &stDBInfo.iDBPort);

    CSingleChatServer* pChatServer = new CSingleChatServer(&stServerInfo, &stDBInfo);
#elif defined _MULTI
    CNetServer::st_SERVER_INFO stServerInfo;
    CNetServer::st_DB_INFO stDBInfo;

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

    g_Config.GetData(L"DBIP", stDBInfo.szDBIP);
    g_Config.GetData(L"DBUser", stDBInfo.szDBUser);
    g_Config.GetData(L"DBPW", stDBInfo.szDBPassword);
    g_Config.GetData(L"DBName", stDBInfo.szDBName);
    g_Config.GetData(L"DBPort", &stDBInfo.iDBPort);

    CMultiChatServer* pChatServer = new CMultiChatServer(&stServerInfo, &stDBInfo);
#elif defined _MULTI_IO_VER
    CNetServer::st_SERVER_INFO stServerInfo;
    CNetServer::st_DB_INFO stDBInfo;

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

    g_Config.GetData(L"DBIP", stDBInfo.szDBIP);
    g_Config.GetData(L"DBUser", stDBInfo.szDBUser);
    g_Config.GetData(L"DBPW", stDBInfo.szDBPassword);
    g_Config.GetData(L"DBName", stDBInfo.szDBName);
    g_Config.GetData(L"DBPort", &stDBInfo.iDBPort);

    CMultiChatServer_IOThread* pChatServer = new CMultiChatServer_IOThread(&stServerInfo, &stDBInfo);
#endif

    pChatServer->Start();

    delete pChatServer;

    return 0;
}