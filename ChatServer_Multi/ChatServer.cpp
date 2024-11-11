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
	FILETIME ftCreationTime, ftExitTime, ftKernelTime, ftUsertTime;
	FILETIME ftCurTime;
	GetProcessTimes(GetCurrentProcess(), &ftCreationTime, &ftExitTime, &ftKernelTime, &ftUsertTime);
	GetSystemTimeAsFileTime(&ftCurTime);

	ULARGE_INTEGER start, now;
	start.LowPart = ftCreationTime.dwLowDateTime;
	start.HighPart = ftCreationTime.dwHighDateTime;
	now.LowPart = ftCurTime.dwLowDateTime;
	now.HighPart = ftCurTime.dwHighDateTime;


	ULONGLONG ullElapsedSecond = (now.QuadPart - start.QuadPart) / 10000 / 1000;

	ULONGLONG temp = ullElapsedSecond;

	ULONGLONG ullElapsedMin = ullElapsedSecond / 60;
	ullElapsedSecond %= 60;

	ULONGLONG ullElapsedHour = ullElapsedMin / 60;
	ullElapsedMin %= 60;

	ULONGLONG ullElapsedDay = ullElapsedHour / 24;
	ullElapsedHour %= 24;

	int cap = 0;
	unsigned long long workerEnqueuedBufCnt = 0;

	for (int i = 0; i < 4; ++i)
	{
		cap = Job::messageQ[i].packetPool_.capacity_;
		workerEnqueuedBufCnt = Job::messageQ[i].workerEnqueuedBufferCnt_;
	}

	monitor.UpdateCpuTime(&PROCESS_CPU_TICK_ELAPSED, &PROCESS_CPU_TICK_TIME_DIFF);


	ULONGLONG acceptTPS = _InterlockedExchange(&acceptCounter_, 0);
	acceptTotal_ += acceptTPS;
	ULONGLONG disconnectTps = InterlockedExchange(&disconnectTPS_, 0);
	ULONGLONG recvTps = InterlockedExchange(&recvTPS_, 0);
	RECV_TOTAL_ += recvTps;
	LONG lSendTps = InterlockedExchange(&lSendTPS_, 0);

	printf(
		"Elapsed Time : %02lluD-%02lluH-%02lluMin-%02lluSec\n"
		"Packet Pool Alloc Capacity : %d\n"
		"MessageQ Capacity : %d\n"
		"MessageQ Queued By Worker : %llu\n"
		"SendQ Pool Capacity : %d\n"
		"Accept TPS: %llu\n"
		"Accept Total : %llu\n"
		"Disconnect TPS: %llu\n"
		"Recv Msg TPS: %llu\n"
		"Send Msg TPS: %d\n"
		"Session Num : %d\n"
		"Player Num : %d\n"
		"RECV TOTAL : %llu\n"
		"RECV_AVR : %.2f\n"
		"----------------------\n"
		"Process Private MBytes : %.2lf\n"
		"Process NP Pool KBytes : %.2lf\n"
		"Memory Available MBytes : %.2lf\n"
		"Machine NP Pool MBytes : %.2lf\n"
		"Processor CPU Time : %.2f\n"
		"Process CPU Time : %.2f\n"
		"Process CPU Time AVR : %.2f\n\n",
		ullElapsedDay, ullElapsedHour, ullElapsedMin, ullElapsedSecond,
		Packet::packetPool_.capacity_,
		cap,
		workerEnqueuedBufCnt,
		pSessionArr_[0].sendPacketQ_.nodePool_.capacity_,
		acceptTPS,
		acceptTotal_,
		disconnectTPS_,
		recvTps,
		lSendTps,
		lSessionNum_,
		lPlayerNum,
		RECV_TOTAL_,
		RECV_TOTAL_ / (float)temp,
		monitor.GetPPB() / (1024 * 1024),
		monitor.GetPNPB() / 1024,
		monitor.GetAB(),
		monitor.GetNPB() / (1024 * 1024),
		monitor._fProcessorTotal,
		monitor._fProcessTotal,
		(float)(PROCESS_CPU_TICK_ELAPSED / (double)monitor._iNumberOfProcessors / (double)PROCESS_CPU_TICK_TIME_DIFF) * 100.0f
	);

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
