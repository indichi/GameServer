#include "CEchoServer.h"

CEchoServer g_Server;

int main(void)
{
    g_Server.Start(L"127.0.0.1", 6000, 12, false, 1000);

    return 0;
}