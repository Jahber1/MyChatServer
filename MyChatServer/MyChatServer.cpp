#pragma comment(lib, "ws2_32")
#include <iostream>
#include <Winsock2.h>
#include <WS2tcpip.h>

#define SERVERPORT 1589

using namespace std;
SOCKET g_ListenSocket; // 전역으로 Listen 소켓 생성

int main(void)
{
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		cout << "윈속 초기화 실패..." << endl;
		return 1;
	}

	// Listen 소켓 초기화, 리턴값은 디스크립터
	g_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);

	// Listen 소켓 Bind
	SOCKADDR serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
}
