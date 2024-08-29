#pragma once
#include "MyChatServer.h"

bool PacketProc(SESSION* pSession, BYTE byPacketType, char* pPacket);
bool netPacketProc_Message(SESSION* pSession, char* pPacket);