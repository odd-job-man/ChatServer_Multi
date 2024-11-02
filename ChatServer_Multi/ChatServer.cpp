#include <WinSock2.h>
#include <stdio.h>
#include "Windows.h"
#include "ChatServer.h"
#include "Packet.h"
#include "Player.h"
#include "Job.h"
#include "CMessageQ.h"
#include "ErrType.h"
#include "Logger.h"

#include "Sector.h"

#include "Parser.h"

bool PacketProc_PACKET(Player* pPlayer, SmartPacket& sp);
extern CMessageQ g_MQ[2][2];
extern SECTOR_AROUND g_sectorAround[50][50];

ChatServer g_ChatServer;

ChatServer::ChatServer()
{
	char* pStart;
	PARSER psr = CreateParser(L"ServerConfig.txt");

	GetValue(psr, L"USER_MAX", (PVOID*)&pStart, nullptr);
	Player::MAX_PLAYER_NUM = (short)_wtoi((LPCWSTR)pStart);
	ReleaseParser(psr);

	Player::pPlayerArr = new Player[maxSession_];

	for (short y = 0; y < NUM_OF_SECTOR_VERTICAL; ++y)
	{
		for (short x = 0; x < NUM_OF_SECTOR_HORIZONTAL; ++x)
		{
			GetSectorAround(y, x, &g_sectorAround[y][x]);
		}
	}
}

BOOL ChatServer::OnConnectionRequest()
{
    if (lSessionNum_ + 1 >= maxSession_ || lPlayerNum + 1 >= Player::MAX_PLAYER_NUM)
        return FALSE;

    return TRUE;
}

void* ChatServer::OnAccept(ULONGLONG id)
{
	Player* pPlayer = Player::pPlayerArr + Player::MAKE_PLAYER_INDEX(id);
	// 컨텐츠에서 해당 플레이어 포인터를 사용하던 중에 재활용 되는것을 막기
	AcquireSRWLockExclusive(&pPlayer->playerLock_);

	if (pPlayer->bUsing_ == true)
		__debugbreak();

	pPlayer->bUsing_ = true;
	pPlayer->bLogin_ = false;
	pPlayer->bRegisterAtSector_ = false;
	pPlayer->sessionId_ = id;
	pPlayer->LastRecvedTime_ = GetTickCount64();

	ReleaseSRWLockExclusive(&pPlayer->playerLock_);
	return nullptr;
}

void ChatServer::OnRelease(ULONGLONG id)
{
	Player* pPlayer = Player::pPlayerArr + Player::MAKE_PLAYER_INDEX(id);
	// 컨텐츠에서 해당 플레이어 포인터를 사용하던 중에 재활용 되는것을 막기
	AcquireSRWLockExclusive(&pPlayer->playerLock_);

	if (pPlayer->bUsing_ == false)
		__debugbreak();

	pPlayer->bUsing_ = false;
	if (pPlayer->bLogin_ == true)
	{
		// 로그인 이후 REQUEST_SECTOR_MOVE가 한번이라도 왓다면 곧 SECTOR 스레드에 의해서 섹터에 해당 플레이어가 등록될것이므로 제거 JOB을 보내줘야함
		if (pPlayer->bRegisterAtSector_ == true)
		{
			Job* pJob = Job::Alloc(en_JOB_TYPE::en_JOB_ON_RELEASE_REMOVE_PLAYER_AT_SECTOR, pPlayer->sectorX_, pPlayer->sectorY_, pPlayer->sessionId_);
			g_MQ[pPlayer->sectorY_ / SECTOR_DENOMINATOR][pPlayer->sectorX_ / SECTOR_DENOMINATOR].Enqueue(pJob);
		}
		InterlockedDecrement(&lPlayerNum);
	}

	ReleaseSRWLockExclusive(&pPlayer->playerLock_);
}

void ChatServer::OnRecv(ULONGLONG id, Packet* pPacket)
{
	SmartPacket sp = std::move(pPacket);
	Player* pPlayer = Player::pPlayerArr + Player::MAKE_PLAYER_INDEX(id);
	AcquireSRWLockShared(&pPlayer->playerLock_);
	PacketProc_PACKET(pPlayer, sp);
	ReleaseSRWLockShared(&pPlayer->playerLock_);
}

void ChatServer::OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket)
{
	switch ((ErrType)errorType)
	{
	case PACKET_PROC_RECVED_PACKET_INVALID_TYPE:
	{
		pRcvdPacket->MoveReadPos(-(int)sizeof(WORD));
		WORD type;
		*pRcvdPacket >> type;
		LOG(L"PACKET_PROC_RECVED_PACKET_INVALID_TYPE", SYSTEM, TEXTFILE, L"Session ID : %d Recved Type : %x", id, type);
		break;
	}
	default:
		__debugbreak();
		break;
	}
}

void ChatServer::Monitoring(int updateCnt, unsigned long long BuffersProcessAtThisFrame)
{
	printf(
		"update Count : %llu\n"
		"Packet Pool Alloc Capacity : %d\n"
		"MessageQ Capacity : %d\n"
		"MessageQ Queued By Worker : %llu\n"
		"SendQ Pool Capacity : %d\n"
		"Accept TPS: %d\n"
		"Accept Total : %d\n"
		"Disconnect TPS: %d\n"
		"Recv Msg TPS: %d\n"
		"Send Msg TPS: %d\n"
		"Session Num : %d\n"
		"Player Num : %d\n"
		"REQ_MESSAGE_TPS : %d\n"
		"RES_MESSAGE_TPS : %d\n\n",
		BuffersProcessAtThisFrame,
		Packet::packetPool_.capacity_,
		g_MQ.packetPool_.capacity_,
		g_MQ.workerEnqueuedBufferCnt_,
		pSessionArr_[0].sendPacketQ_.nodePool_.capacity_,
		lAcceptTotal_ - lAcceptTotal_PREV,
		lAcceptTotal_,
		lDisconnectTPS_,
		lRecvTPS_,
		lSendTPS_,
		lSessionNum_,
		lPlayerNum,
		REQ_MESSAGE_TPS,
		RES_MESSAGE_TPS);

	lAcceptTotal_PREV = lAcceptTotal_;
	InterlockedExchange(&lDisconnectTPS_, 0);
	InterlockedExchange(&lRecvTPS_, 0);
	InterlockedExchange(&lSendTPS_, 0);
	REQ_MESSAGE_TPS = 0;
	RES_MESSAGE_TPS = 0;
}

void ChatServer::DisconnectAll()
{
	closesocket(hListenSock_);
	WaitForSingleObject(hAcceptThread_, INFINITE);
	CloseHandle(hAcceptThread_);

	LONG MaxSession = maxSession_;
	for (LONG i = 0; i < MaxSession; ++i)
	{
		// RELEASE 중이 아니라면 Disconnect
		pSessionArr_[i].bDisconnectCalled_ = true;
		CancelIoEx((HANDLE)pSessionArr_[i].sock_, nullptr);
	}

	while (lSessionNum_ != 0);
}
