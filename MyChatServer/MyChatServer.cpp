#pragma comment(lib, "ws2_32")
#include <iostream>
#include <Winsock2.h>
#include <WS2tcpip.h>
#include <list>
#include "RingBuffer.h"
#include "Message.h"
#include "MyChatServer.h"
#include "PacketMessage.h"
#include "PacketProcedure.h"

#define SERVERPORT 1589
using namespace std;

int g_ID = -1;
SOCKET g_ListenSocket; // 전역으로 Listen 소켓 생성
list<SESSION*> g_ClientList;

int main(void)
{
	int bind_retval;
	int listen_retval;
	int sock_retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		cout << "윈속 초기화 에러" << endl;
		DebugBreak();
	}

	// 소켓 생성
	g_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (g_ListenSocket == INVALID_SOCKET)
	{
		cout << "리슨 소켓 생성 에러" << endl;
		DebugBreak();
	}

	// bind
	SOCKADDR_IN ServerAddr;
	ZeroMemory(&ServerAddr, sizeof(ServerAddr));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	ServerAddr.sin_port = htons(SERVERPORT);
	bind_retval = bind(g_ListenSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
	if (bind_retval == SOCKET_ERROR)
	{
		cout << "바인드 에러" << endl;
		DebugBreak();
	}

	// listen
	listen_retval = listen(g_ListenSocket, SOMAXCONN);
	if (listen_retval == SOCKET_ERROR)
	{
		cout << "리슨 소켓 설정 에러" << endl;
		DebugBreak();
	}

	// 논블로킹 소켓으로 전환
	u_long on = 1;
	sock_retval = ioctlsocket(g_ListenSocket, FIONBIO, &on);
	if (sock_retval == SOCKET_ERROR)
	{
		cout << "논블로킹 소켓 전환 에러" << endl;
		DebugBreak();
	}

	SESSION* pSession;

	FD_SET rset;
	FD_SET wset;

	FD_ZERO(&rset);
	FD_ZERO(&wset);

	FD_SET(g_ListenSocket, &rset);
	for (auto& entry : g_ClientList)
	{
		FD_SET(entry->ClientSocket, &rset);
		if (entry->SendQ.GetUseSize() > 0)
		{
			FD_SET(entry->ClientSocket, &wset);
		}

	}

	timeval Time;
	Time.tv_sec = 0;
	Time.tv_usec = 0;

	int iSelect_retval;
	iSelect_retval = select(0, &rset, &wset, 0, &Time);

	if (iSelect_retval == SOCKET_ERROR)
	{
		cout << "Select 소켓 에러" << WSAGetLastError() << endl;
	}
	else
	{
		if (FD_ISSET(g_ListenSocket, &rset))
		{
			netProc_Accept();
		}

		for (auto& entry : g_ClientList)
		{
			if (FD_ISSET(entry->ClientSocket, &rset))
			{
				--iSelect_retval;
				netProc_Recv(entry);
			}
			if (FD_ISSET(entry->ClientSocket, &wset))
			{
				--iSelect_retval;
				netProc_Send(entry);
			}

			if (iSelect_retval == 0)
			{
				//cout << "Select 타임 아웃" << endl;
				break;
			}
		}
	}
}

void netProc_Recv(SESSION* pSession)
{
	// 링버퍼 사이즈의 Direct Enqueue 가능한 사이즈로 최대 크기 설정해서 수신
	int recv_retval;
	recv_retval = recv(pSession->ClientSocket, pSession->RecvQ.GetRearBufferPtr(), pSession->RecvQ.DirectEnqueueSize(), 0);
	if (recv_retval == SOCKET_ERROR || recv_retval == 0)
	{
		//cout << "recv 에러로 소켓 끊기" << endl;
		Disconnect(pSession);
	}
	else
	{
		pSession->RecvQ.MoveRear(recv_retval);
		if (pSession->RecvQ.WritePos == 0) // 받아온 데이터가 링버퍼 경계에 걸렸을 때 한 번 더 받아보기
		{
			recv_retval = recv(pSession->ClientSocket, pSession->RecvQ.GetRearBufferPtr(), pSession->RecvQ.DirectEnqueueSize(), 0);
			if (recv_retval == SOCKET_ERROR)
			{
				//cout << "recv 에러2" << endl;
				Disconnect(pSession);
			}
			else
			{
				pSession->RecvQ.MoveRear(recv_retval);
			}
		}
	}

	BYTE tempPacketHeader[3]; // 임시 헤더버퍼
	char tempPacketBuffer[150]; // 임시 패킷버퍼
	// 완료패킷 처리 부분
	while (1)
	{
		if (pSession->RecvQ.GetUseSize() >= 3) // RecvQ에 최소한의 사이즈가 있는지 확인 (헤더사이즈 초과)
		{
			pSession->RecvQ.Peek((char*)tempPacketHeader, sizeof(stPacketHeader)); // 헤더 Peek
			if (tempPacketHeader[0] != 0x89)
			{
				printf("패킷 헤더 코드 오류: %x", tempPacketHeader[0]);
			}
			else
			{
				// 헤더의 Len 값과 RecvQ의 데이터 사이즈 비교 - 완성 패킷 사이즈 : 헤더 + Len
				if (tempPacketHeader[1] + sizeof(stPacketHeader) > pSession->RecvQ.GetUseSize())
				{
					break; // RecvQ에 완성 패킷 사이즈보다 적은 데이터가 들어있어서 패스
				}
				else
				{
					pSession->RecvQ.MoveFront(sizeof(stPacketHeader)); // 데이터 Peek 한 만큼 ReadPos 이동
					pSession->RecvQ.Dequeue((char*)tempPacketBuffer, tempPacketHeader[1]); // RecvQ에서 Len만큼 임시 패킷버퍼로 뽑음
					PacketProc(pSession, tempPacketHeader[2], (char*)tempPacketBuffer); // 헤더의 타입에 따른 분기를 위해 패킷 프로시저 호출
				}
			}
		}
		else // RecvQ에 헤더사이즈보다 적은 데이터가 들어있어서 패스
		{
			break;
		}
	}
}

void netProc_Send(SESSION* pSession)
{
	while (pSession->SendQ.GetUseSize() != 0)
	{
		int send_retval;
		send_retval = send(pSession->ClientSocket, pSession->SendQ.GetFrontBufferPtr(), pSession->SendQ.DirectDequeueSize(), 0);
		if (send_retval == SOCKET_ERROR || send_retval == 0)
		{
			if (WSAGetLastError() == 10053 || WSAGetLastError() == 10054)
			{
				if (pSession->ClientSocket != INVALID_SOCKET)
				{
					Disconnect(pSession);
				}
			}
			else
			{
				cout << "send 에러1" << endl;
			}
		}
		else
		{
			pSession->SendQ.MoveFront(send_retval);
			if (pSession->SendQ.ReadPos == 0) // 보낸 데이터가 링버퍼 경계에 걸렸을 때 한 번 더 보내보기
			{
				send_retval = send(pSession->ClientSocket, pSession->SendQ.GetFrontBufferPtr(), pSession->SendQ.DirectDequeueSize(), 0);
				if (send_retval == SOCKET_ERROR)
				{
					cout << "send 에러" << endl;
					if (WSAGetLastError() == 10053 || WSAGetLastError() == 10054)
					{
						if (pSession->ClientSocket != INVALID_SOCKET)
						{
							Disconnect(pSession);
						}
					}
				}
				else
				{
					pSession->SendQ.MoveFront(send_retval);
				}
			}
		}
	}
}

int netProc_Accept()
{
	// 새로운 세션 생성
	SESSION* NewSession = new SESSION;
	ZeroMemory(NewSession, sizeof(NewSession));

	// 클라 정보 받을 변수 선언
	SOCKADDR_IN ClientAddr;
	int addrlen = sizeof(ClientAddr);
	int recv_retval;

	// 메시지 전달용 패킷 선언
	stPacketHeader Header;
	stCreate Create;
	stCreateOther CreateOther;
	stMessage Message;
	char buf[150];

	NewSession->ClientSocket = accept(g_ListenSocket, (SOCKADDR*)&ClientAddr, &addrlen);
	if (NewSession->ClientSocket == INVALID_SOCKET)
	{
		cout << "Accpet 에러: " << WSAGetLastError() << endl;
		return 0;
	}
	else
	{
		recv_retval = recv(NewSession->ClientSocket, buf, sizeof(buf), 0);

		NewSession->ID = ++g_ID;
		strcpy(NewSession->NickName, buf + 3);
		g_ClientList.push_back(NewSession);

		// 캐릭터 생성 메시지 전달
		mpCreate(&Header, &Create, NewSession->NickName);
		SendUniCast(NewSession, &Header, (char*)&Create);

		// 다른 캐릭터에게 내 캐릭터 생성 메시지 전달
		mpCreateOther(&Header, &CreateOther, NewSession->NickName);
		SendBroadCast(NewSession, &Header, (char*)&CreateOther);

		// 다른 캐릭터의 생성 메시지를 나에게 전달
		/*for (auto& entry : g_ClientList)
		{
			if (entry != NewSession)
			{
				mpCreateOther(&Header, &CreateOtherCharacter, entry->ID, entry->Direction, entry->xPos, entry->yPos, 100);
				SendUniCast(NewSession, &Header, (char*)&CreateOtherCharacter);
			}
		}*/
	}
}

void SendUniCast(SESSION* pSession, stPacketHeader* Header, char* pPacket)
{
	pSession->SendQ.Enqueue((char*)Header, sizeof(stPacketHeader));
	pSession->SendQ.Enqueue((char*)pPacket, (int)Header->bySize);
}

void SendBroadCast(SESSION* pSession, stPacketHeader* Header, char* pPacket)
{
	for (auto& entry : g_ClientList)
	{
		if (pSession == entry)
		{
			continue;
		}
		else
		{
			entry->SendQ.Enqueue((char*)Header, sizeof(stPacketHeader));
			entry->SendQ.Enqueue((char*)pPacket, (int)Header->bySize);
		}
	}
}

void Disconnect(SESSION* pSession)
{
	stPacketHeader Header;
	stDelete Delete;
	mpDelete(&Header, &Delete, pSession->NickName);
	SendBroadCast(NULL, &Header, (char*)&Delete);

	closesocket(pSession->ClientSocket);
	pSession->ClientSocket = INVALID_SOCKET;
	delete(pSession);
}