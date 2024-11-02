#pragma once
#include "Packet.h"
#include "Player.h"
#include "SCCContents.h"

void CS_CHAT_REQ_LOGIN(Player* pPlayer, INT64 AccountNo, const WCHAR* pID, const WCHAR* pNickName, const char* pSessionKey);
void CS_CHAT_REQ_SECTOR_MOVE(Player* pPlayer, INT64 accountNo, WORD sectorX, WORD sectorY);
void CS_CHAT_REQ_MESSAGE(Player* pPlayer, SmartPacket& sp);
void CS_CHAT_REQ_HEARTBEAT(Player* pPlayer);

__forceinline void CS_CHAT_REQ_LOGIN_RECV(Player* pPlayer, SmartPacket& sp)
{
	INT64 accountNo;
	*sp >> accountNo;

	WCHAR* pID = (WCHAR*)sp->GetPointer(sizeof(WCHAR) * Player::ID_LEN);
	WCHAR* pNickName = (WCHAR*)sp->GetPointer(sizeof(WCHAR) * Player::NICK_NAME_LEN);
	char* pSessionKey = (char*)sp->GetPointer(sizeof(char) * Player::SESSION_KEY_LEN);
	CS_CHAT_REQ_LOGIN(pPlayer, accountNo, pID, pNickName, pSessionKey);
}

__forceinline void CS_CHAT_REQ_SECTOR_MOVE_RECV(Player* pPlayer, SmartPacket& sp)
{
	INT64 accountNo;
	WORD sectorX;
	WORD sectorY;
	*sp >> accountNo >> sectorX >> sectorY;
	CS_CHAT_REQ_SECTOR_MOVE(pPlayer, accountNo, sectorX, sectorY);
}

__forceinline void CS_CHAT_REQ_MESSAGE_RECV(Player* pPlayer, SmartPacket& sp)
{
	INT64 accountNo;
	*sp >> accountNo;
	WORD messageLen;
	*sp >> messageLen;
	WCHAR* pMessage = (WCHAR*)sp->GetPointer(messageLen);

	if (sp->IsBufferEmpty())
	{
		g_ChatServer.Disconnect(pPlayer->sessionId_);
		return;
	}
	CS_CHAT_REQ_MESSAGE(pPlayer, sp);
}

__forceinline void CS_CHAT_REQ_HEARTBEAT_RECV(Player* pPlayer, SmartPacket& sp)
{
	CS_CHAT_REQ_HEARTBEAT(pPlayer);
}


