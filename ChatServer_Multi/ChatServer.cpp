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

#pragma comment(lib,"pdh.lib")

bool PacketProc_PACKET(Player* pPlayer, SmartPacket& rcvdPacket);
extern SECTOR_AROUND g_sectorAround[50][50];

ChatServer g_ChatServer;

ChatServer::ChatServer()
{
	char* pStart;
	PARSER psr = CreateParser(L"ServerConfig.txt");

	GetValue(psr, L"USER_MAX", (PVOID*)&pStart, nullptr);
	Player::MAX_PLAYER_NUM = (short)_wtoi((LPCWSTR)pStart);

	GetValue(psr, L"TICK_PER_FRAME", (PVOID*)&pStart, nullptr);
	TICK_PER_FRAME_ = _wtoi((LPCWSTR)pStart);

	GetValue(psr, L"SESSION_TIMEOUT", (PVOID*)&pStart, nullptr);
	SESSION_TIMEOUT_ = (ULONGLONG)_wtoi64((LPCWSTR)pStart);
	GetValue(psr, L"PLAYER_TIMEOUT", (PVOID*)&pStart, nullptr);
	PLAYER_TIMEOUT_ = (ULONGLONG)_wtoi64((LPCWSTR)pStart);
	ReleaseParser(psr);

	Player::pPlayerArr = new Player[maxSession_];

	for (short y = 0; y < NUM_OF_SECTOR_VERTICAL; ++y)
	{
		for (short x = 0; x < NUM_OF_SECTOR_HORIZONTAL; ++x)
		{
			GetSectorAround(x, y, &g_sectorAround[y][x]);
		}
	}

	hUpdateThreadEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (hUpdateThreadEvent_ == NULL)
		__debugbreak();

	InitializeSRWLock(&Player::playerArrLock);

	for (int i = 0; i < _countof(Player::accountNoMapRock_); ++i)
		InitializeSRWLock(&Player::accountNoMapRock_[i]);

	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
		ResumeThread(hIOCPWorkerThreadArr_[i]);

	ResumeThread(hAcceptThread_);
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
	Player::timeOutLock_.AcquireShared();
	pPlayer->bUsing_ = true;
	pPlayer->bLogin_ = false;
	pPlayer->bRegisterAtSector_ = false;
	pPlayer->sessionId_ = id;
	pPlayer->LastRecvedTime_ = GetTickCount64();
	Player::timeOutLock_.ReleaseShared();
	return nullptr;
}

void ChatServer::OnRelease(ULONGLONG id)
{
	Player* pPlayer = Player::pPlayerArr + Player::MAKE_PLAYER_INDEX(id);
	Player::timeOutLock_.AcquireShared();
	pPlayer->bUsing_ = false;
	if (pPlayer->bLogin_ == true)
	{
		// 로그인 이후 REQUEST_SECTOR_MOVE가 한번이라도 왓다면 곧 SECTOR 스레드에 의해서 섹터에 해당 플레이어가 등록될것이므로 제거 JOB을 보내줘야함
		if (pPlayer->bRegisterAtSector_ == true)
		{
			Job* pJob = Job::Alloc(en_JOB_TYPE::en_JOB_ON_RELEASE_REMOVE_PLAYER_AT_SECTOR, pPlayer->sectorX_, pPlayer->sectorY_, pPlayer->sessionId_);
			Job::Enqueue(pJob, GetOrder(pPlayer->sectorX_, pPlayer->sectorY_));
		}
		InterlockedDecrement(&lPlayerNum);
	}
	Player::timeOutLock_.ReleaseShared();
}

void ChatServer::OnRecv(ULONGLONG id, Packet* pPacket)
{
	Player::timeOutLock_.AcquireShared();
	SmartPacket sp = std::move(pPacket);
	Player* pPlayer = Player::pPlayerArr + Player::MAKE_PLAYER_INDEX(id);
	PacketProc_PACKET(pPlayer, sp);
	Player::timeOutLock_.ReleaseShared();
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

void PQCS(int order);

void ChatServer::OnPost(int order)
{
	PQCS(order);
}

void ChatServer::Monitoring()
{
	int cap = 0;
	unsigned long long workerEnqueuedBufCnt = 0;

	for (int i = 0; i < 4; ++i)
	{
		cap = Job::messageQ[i].packetPool_.capacity_;
		workerEnqueuedBufCnt = Job::messageQ[i].workerEnqueuedBufferCnt_;
	}

	monitor.UpdateCpuTime();
	printf(
		"update Count : %d\n"
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
		"RES_MESSAGE_TPS : %d\n"
		"----------------------\n"
		"Process Private MBytes : %.2lf\n"
		"Process NP Pool KBytes : %.2lf\n"
		"Memory Available MBytes : %.2lf\n"
		"Machine NP Pool MBytes : %.2lf\n"
		"Processor CPU Time : %.2f\n"
		"Process CPU Time : %.2f\n\n",
		PQCS_UPDATE_CNT_,
		Packet::packetPool_.capacity_,
		cap,
		workerEnqueuedBufCnt,
		pSessionArr_[0].sendPacketQ_.nodePool_.capacity_,
		lAcceptTotal_ - lAcceptTotal_PREV,
		lAcceptTotal_,
		lDisconnectTPS_,
		lRecvTPS_,
		lSendTPS_,
		lSessionNum_,
		lPlayerNum,
		REQ_MESSAGE_TPS,
		RES_MESSAGE_TPS,
		monitor.GetPPB() / (1024 * 1024),
		monitor.GetPNPB() / 1024,
		monitor.GetAB(),
		monitor.GetNPB() / (1024 * 1024),
		monitor._fProcessorTotal,
		monitor._fProcessTotal
	);

	InterlockedExchange(&lAcceptTotal_PREV, lAcceptTotal_);
	InterlockedExchange(&PQCS_UPDATE_CNT_, 0);
	InterlockedExchange(&lDisconnectTPS_, 0);
	InterlockedExchange(&lRecvTPS_, 0);
	InterlockedExchange(&lSendTPS_, 0);
	InterlockedExchange(&REQ_MESSAGE_TPS, 0);
	InterlockedExchange(&RES_MESSAGE_TPS, 0);
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
