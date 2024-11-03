#include <WinSock2.h>
#include "CommonProtocol.h"
#include "Job.h"
#include "CMessageQ.h"
#include "Packet.h"
#include "Player.h"
#include "CSCContents.h"
#include "ErrType.h"
#include "ChatServer.h"

extern ChatServer g_ChatServer;

void Update()
{
	static unsigned long long timeOutCheck = GetTickCount64();
	static unsigned long long firstTimeOutCheck = timeOutCheck;

	PostQueuedCompletionStatus(g_ChatServer.hcp_, 1, 1, 0);
	PostQueuedCompletionStatus(g_ChatServer.hcp_, 1, 1, (LPOVERLAPPED)1);
	PostQueuedCompletionStatus(g_ChatServer.hcp_, 1, 1, (LPOVERLAPPED)2);
	PostQueuedCompletionStatus(g_ChatServer.hcp_, 1, 1, (LPOVERLAPPED)3);

	//unsigned long long currentTime = GetTickCount64();
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
}

bool PacketProc_PACKET(Player* pPlayer, SmartPacket& rcvdPacket)
{
	WORD type;
	*rcvdPacket >> type;
	switch (type)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		CS_CHAT_REQ_LOGIN(pPlayer, rcvdPacket);
		break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		CS_CHAT_REQ_SECTOR_MOVE(pPlayer, rcvdPacket);
		break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		CS_CHAT_REQ_MESSAGE(pPlayer, rcvdPacket);
		break;
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		CS_CHAT_REQ_HEARTBEAT(pPlayer);
		break;
	default:
		g_ChatServer.OnError(pPlayer->sessionId_, (int)PACKET_PROC_RECVED_PACKET_INVALID_TYPE, rcvdPacket.GetPacket());
		g_ChatServer.Disconnect(pPlayer->sessionId_);
		break;
	}
	return true;
}

