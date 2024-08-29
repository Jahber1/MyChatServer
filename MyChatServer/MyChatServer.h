#pragma once
#include <iostream>
#include <list>
#include <winsock2.h>
#include "RingBuffer.h"

using namespace std;

struct SESSION
{
	SOCKET ClientSocket;
	int ID;
	char NickName[50];
	CRingBuffer RecvQ;
	CRingBuffer SendQ;
};

extern list<SESSION*> g_ClientList;
extern SOCKET g_ListenSocket;
extern int g_ID;

int netProc_Accept();
void netProc_Recv(SESSION* pSession);
void netProc_Send(SESSION* pSession);
void SendUniCast(SESSION* pSession, stPacketHeader* Header, char* pPacket);
void SendBroadCast(SESSION* pSession, stPacketHeader* Header, char* pPacket);
void Disconnect(SESSION* pSession);