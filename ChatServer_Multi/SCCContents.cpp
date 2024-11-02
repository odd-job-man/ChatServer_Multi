#include "WinSock2.h"
#include "Packet.h"
#include "Session.h"
#include "Player.h"
#include "SCCContents.h"

void MAKE_CS_CHAT_RES_LOGIN(WORD type, BYTE status, INT64 accountNo, SmartPacket& sp)
{
	*sp << type << status << accountNo;
}

void MAKE_CS_CHAT_RES_SECTOR_MOVE(INT64 accountNo, WORD sectorX, WORD sectorY, SmartPacket& sp)
{
	*sp << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << accountNo << sectorX << sectorY;
}

void MAKE_CS_CHAT_RES_MESSAGE(INT64 accountNo, WCHAR* pID, WCHAR* pNickName, WORD messageLen, WCHAR* pMessage, SmartPacket& sp)
{
	*sp << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE << accountNo;
	sp->PutData((char*)pID, Player::ID_LEN * 2);
	sp->PutData((char*)pNickName, Player::NICK_NAME_LEN * 2);
	*sp << messageLen;
	sp->PutData((char*)pMessage, messageLen);
}

void MAKE_CS_CHAT_RES_MESSAGE(INT64 accountNo, WCHAR* pID, WCHAR* pNickName, WORD messageLen, WCHAR* pMessage, Packet* pPacket)
{
	*pPacket << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE << accountNo;
	pPacket->PutData((char*)pID, Player::ID_LEN * 2);
	pPacket->PutData((char*)pNickName, Player::NICK_NAME_LEN * 2);
	*pPacket << messageLen;
	pPacket->PutData((char*)pMessage, messageLen);
}
