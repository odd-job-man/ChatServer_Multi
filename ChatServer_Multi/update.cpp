#include <WinSock2.h>
#include "CommonProtocol.h"
#include "Job.h"
#include "CMessageQ.h"
#include "Packet.h"
#include "Player.h"
#include "CSCContents.h"
#include "ErrType.h"
#include "ChatServer.h"

CMessageQ g_MQ[2][2];
extern ChatServer g_ChatServer;

bool PacketProc_PACKET(ULONGLONG id, SmartPacket& sp);
bool PacketProc_JOB(SmartPacket& sp);

unsigned long long Update()
{
	static unsigned long long timeOutCheck = GetTickCount64();
	static unsigned long long firstTimeOutCheck = timeOutCheck;

	g_MQ.Swap();
	unsigned long long ret = g_MQ.BuffersToProcessThisFrame_;
	while (true)
	{
		SmartPacket sp = g_MQ.Dequeue();
		if (sp.GetPacket() == nullptr)
			break;

		if (sp->recvType_ == RECVED_PACKET)
		{
			PacketProc_PACKET(sp);
		}
		else
		{
			PacketProc_JOB(sp);
		}
	}
	unsigned long long currentTime = GetTickCount64();
	// 3초에 한번씩 타임아웃 체크함(우선 하드코딩)
	//if (currentTime - timeOutCheck <= 30000)
	//	return ret;

	//for (int i = 0; i < g_ChatServer.maxSession_; ++i)
	//{
	//	Player* pPlayer = Player::pPlayerArr + i;
	//	if (pPlayer->bUsing_ == false)
	//		continue;

	//	//40초 지나면 타임 아웃 처리
	//	if (currentTime - pPlayer->LastRecvedTime_ > g_ChatServer.TIME_OUT_MILLISECONDS_)
	//		g_ChatServer.Disconnect(pPlayer->sessionId_);
	//}

	//timeOutCheck += timeOutCheck - ((timeOutCheck - firstTimeOutCheck) % 30000);
	return ret;
}

void CS_CHAT_REQ_LOGIN(Player* pPlayer, SmartPacket& sp)
{
	if (g_ChatServer.lPlayerNum >= Player::MAX_PLAYER_NUM)
	{
		g_ChatServer.Disconnect(pPlayer->sessionId_);
		return;
	}

	pPlayer->LastRecvedTime_ = GetTickCount64();

	INT64 AccountNo;
	*sp >> AccountNo;
	WCHAR* pID = (WCHAR*)sp->GetPointer(sizeof(WCHAR) * Player::ID_LEN);
	WCHAR* pNickName = (WCHAR*)sp->GetPointer(sizeof(WCHAR) * Player::NICK_NAME_LEN);
	char* pSessionKey = (char*)sp->GetPointer(sizeof(char) * Player::SESSION_KEY_LEN);

	wcscpy_s(pPlayer->ID_, Player::ID_LEN, pID);
	wcscpy_s(pPlayer->nickName_, Player::NICK_NAME_LEN, pNickName);

	SmartPacket sp = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_LOGIN(en_PACKET_CS_CHAT_RES_LOGIN, 1, AccountNo, sp);
	g_ChatServer.SendPacket(pPlayer->sessionId_, sp);
	InterlockedIncrement(&g_ChatServer.lPlayerNum);
}

bool PacketProc_PACKET(Player* pPlayer, SmartPacket& sp)
{
	WORD type;
	*sp >> type;
	switch (type)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		CS_CHAT_REQ_LOGIN(pPlayer, sp);
		break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		CS_CHAT_REQ_SECTOR_MOVE_RECV(pPlayer, sp);
		break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		CS_CHAT_REQ_MESSAGE_RECV(pPlayer, sp);
		break;
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		CS_CHAT_REQ_HEARTBEAT_RECV(pPlayer, sp);
		break;
	default:
		g_ChatServer.OnError(pPlayer->sessionId_, (int)PACKET_PROC_RECVED_PACKET_INVALID_TYPE, sp.GetPacket());
		g_ChatServer.Disconnect(pPlayer->sessionId_);
		break;
	}
	return true;
}

bool PacketProc_JOB(SmartPacket& sp)
{
	WORD type;
	*sp >> type;
	switch (type)
	{
	case en_JOB_ON_ACCEPT:
		JOB_ON_ACCEPT_RECV(sp);
		break;
	case en_JOB_ON_RELEASE:
		JOB_ON_RELEASE_RECV(sp);
		break;
	default:
		__debugbreak();
		break;
	}
	return true;
}
