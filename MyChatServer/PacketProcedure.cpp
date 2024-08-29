#include <iostream>
#include "Message.h"
#include "PacketProcedure.h"
#include "PacketMessage.h"
#include "MyChatServer.h"

bool PacketProc(SESSION* pSession, BYTE byPacketType, char* pPacket)
{
    switch (byPacketType)
    {
    case dfPACKET_MESSAGE:
        return netPacketProc_Message(pSession, pPacket);
        break;
    }
    return TRUE;
}

bool netPacketProc_Message(SESSION* pSession, char* pPacket)
{
    stMessage* pMessage = (stMessage*)pPacket;

    // 자신을 포함한 현재 접속중인 모든 사용자에게 메시지 전달
    stPacketHeader Header;
    stMessage SendMsg;
    mpMessage(&Header, &SendMsg, pMessage->Message);
    SendBroadCast(NULL, &Header, (char*)&SendMsg);

    return true;
}