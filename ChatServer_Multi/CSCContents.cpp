#include <WinSock2.h>
#include "CSCContents.h"
#include "Sector.h"
#include "ChatServer.h"
#include "CMessageQ.h"
#include "Job.h"

thread_local int g_threadNumber;

extern CMessageQ g_MQ[2][2];

extern ChatServer g_ChatServer;

SECTOR_AROUND g_sectorAround[50][50];

void CS_CHAT_REQ_LOGIN(Player* pPlayer, INT64 AccountNo, const WCHAR* pID, const WCHAR* pNickName, const char* pSessionKey)
{
	// 플레이어수가 일정수 넘어가면 로그인 실패시킴
	if (g_ChatServer.lPlayerNum >= Player::MAX_PLAYER_NUM)
	{
		g_ChatServer.Disconnect(pPlayer->sessionId_);
		return;
	}

	pPlayer->LastRecvedTime_ = GetTickCount64();
	pPlayer->accountNo_ = AccountNo;
	pPlayer->bLogin_ = true;

	wcscpy_s(pPlayer->ID_, Player::ID_LEN, pID);
	wcscpy_s(pPlayer->nickName_, Player::NICK_NAME_LEN, pNickName);

	SmartPacket sp = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_LOGIN(en_PACKET_CS_CHAT_RES_LOGIN, 1, AccountNo, sp);
	g_ChatServer.SendPacket(pPlayer->sessionId_, sp);
	++g_ChatServer.lPlayerNum;
}

void CS_CHAT_REQ_SECTOR_MOVE(Player* pPlayer, INT64 accountNo, WORD sectorX, WORD sectorY)
{
	if (pPlayer->accountNo_ != accountNo)
	{
		g_ChatServer.Disconnect(pPlayer->sessionId_);
		return;
	}

	// 클라가 유효하지 않은 좌표로 요청햇다면 끊어버린다
	if (IsNonValidSector(sectorX, sectorY))
	{
		g_ChatServer.Disconnect(pPlayer->sessionId_);
		return;
	}

	CMessageQ* pADD_JOBQ = &g_MQ[sectorY / SECTOR_DENOMINATOR][sectorX / SECTOR_DENOMINATOR];
	CMessageQ* pREMOVE_JOBQ = &g_MQ[pPlayer->sectorY_ / SECTOR_DENOMINATOR][pPlayer->sectorX_ / SECTOR_DENOMINATOR];

	Job* pREQ_SECTOR_MOVE_ADD_JOB = Job::Alloc(en_JOB_TYPE::en_JOB_CS_CHAT_REQ_SECTOR_MOVE_ADD, sectorX, sectorY, pPlayer->sessionId_);
	Job* pREQ_SECTOR_MOVE_REMOVE_JOB = Job::Alloc(en_JOB_TYPE::en_JOB_CS_CHAT_REQ_SECTOR_MOVE_REMOVE, pPlayer->sectorX_, pPlayer->sectorY_, pPlayer->sessionId_);

	// 플레이어 섹터좌표 수정
	pPlayer->sectorX_ = sectorX;
	pPlayer->sectorY_ = sectorY;

	InterlockedIncrement(&pREQ_SECTOR_MOVE_ADD_JOB->refCnt_);
	InterlockedIncrement(&pREQ_SECTOR_MOVE_REMOVE_JOB->refCnt_);

	pADD_JOBQ->Enqueue(pREQ_SECTOR_MOVE_ADD_JOB);
	pREMOVE_JOBQ->Enqueue(pREQ_SECTOR_MOVE_REMOVE_JOB);
}

void CS_CHAT_REQ_SECTOR_MOVE_JOB_REMOVE(const Player* pPlayer, WORD sectorX, WORD sectorY)
{
	RemoveClientAtSector(sectorX, sectorY, pPlayer);
}

void CS_CHAT_REQ_SECTOR_MOVE_JOB_ADD(const Player* pPlayer, INT64 accountNo, ULONGLONG sessionId, WORD sectorX, WORD sectorY)
{
	RegisterClientAtSector(sectorX, sectorY, pPlayer);
	SmartPacket sp = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_SECTOR_MOVE(accountNo, sectorX, sectorY, sp);
	g_ChatServer.SendPacket(sessionId, sp);
}

void CS_CHAT_REQ_MESSAGE(Player* pPlayer, SmartPacket& sp)
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


	// 디버깅용
	if (pPlayer->accountNo_ != accountNo)
	{
		g_ChatServer.Disconnect(pPlayer->sessionId_);
		return;
	}

	pPlayer->LastRecvedTime_ = GetTickCount64();

	WORD sectorX = pPlayer->sectorX_;
	WORD sectorY = pPlayer->sectorY_;

	Packet* pResPacket = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_MESSAGE(accountNo, pPlayer->ID_, pPlayer->nickName_, messageLen, pMessage, pResPacket);
	Job* pJob = Job::Alloc(en_JOB_TYPE::en_JOB_CS_CHAT_REQ_MESSAGE, sectorX, sectorY, pPlayer->sessionId_, pResPacket);

	// 해당함수 종료까지 Job의 수명이 종료되지 않아야해서 InterlockedIncrement
	InterlockedIncrement(&pJob->refCnt_);

	bool bBoundary = false;
	switch (sectorY)
	{
	case 23:
	{
		switch (sectorX)
		{
		case 24:
		case 25:
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			g_MQ[0][0].Enqueue(pJob);
			g_MQ[0][1].Enqueue(pJob);
			bBoundary = true;
			break;
		default:
			break;
		}
		break;
	}
	case 24:
	{
		switch (sectorX)
		{
		case 23:
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			g_MQ[0][0].Enqueue(pJob);
			g_MQ[1][0].Enqueue(pJob);
			bBoundary = true;
			break;
		case 24:
		case 25:
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			g_MQ[0][0].Enqueue(pJob);
			g_MQ[0][1].Enqueue(pJob);
			g_MQ[1][0].Enqueue(pJob);
			g_MQ[1][1].Enqueue(pJob);
			bBoundary = true;
			break;
		case 26:
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			g_MQ[0][1].Enqueue(pJob);
			g_MQ[1][1].Enqueue(pJob);
			bBoundary = true;
			break;
		default:
			break;
		}
		break;
	}
	case 25:
	{
		switch (sectorX)
		{
		case 23:
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			g_MQ[0][0].Enqueue(pJob);
			g_MQ[1][0].Enqueue(pJob);
			bBoundary = true;
			break;
		case 24:
		case 25:
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			g_MQ[0][0].Enqueue(pJob);
			g_MQ[0][1].Enqueue(pJob);
			g_MQ[1][0].Enqueue(pJob);
			g_MQ[1][1].Enqueue(pJob);
			bBoundary = true;
			break;
		case 26:
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			g_MQ[0][1].Enqueue(pJob);
			g_MQ[1][1].Enqueue(pJob);
			bBoundary = true;
			break;
		default:
			break;
		}
		break;
	}
	case 26:
	{
		switch (sectorX)
		{
		case 24:
		case 25:
			InterlockedIncrement(&pJob->refCnt_);
			InterlockedIncrement(&pJob->refCnt_);
			g_MQ[1][0].Enqueue(pJob);
			g_MQ[1][1].Enqueue(pJob);
			bBoundary = true;
			break;
		default:
			break;
		}
		break;
	}
	default:
		break;
	}

	if (bBoundary == false)
	{
		InterlockedIncrement(&pJob->refCnt_);
		g_MQ[sectorY / SECTOR_DENOMINATOR][sectorX / SECTOR_DENOMINATOR].Enqueue(pJob);
	}

	if (InterlockedDecrement(&pJob->refCnt_) == 0)
		Job::Free(pJob);
}

void SendPacketThread0(WORD sectorX, WORD sectorY)
{
	switch (sectorY)
	{
	case 23:
		break;
	case 24:
		break;
	case 25:
		break;
	case 26:
		break;
	default:
		break;
	}
}

void SendPacketThread1()
{

}

void SendPacketThread2()
{
	
}

void SendPacketThread3()
{
}

void CS_CHAT_REQ_MESSAGE_JOB(Job* pJob)
{
	WORD sectorY = pJob->sectorY_;
	WORD sectorX = pJob->sectorX_;

	bool bBoundary = false;
	SmartPacket sp = std::move(pJob->pPacket_);
	Player* pPlayer = Player::pPlayerArr + Player::MAKE_PLAYER_INDEX(pJob->sessionId_);

	if (bBoundary == false)
	{
		SendPacket_AROUND(pPlayer, pJob->sessionId_, &g_sectorAround[sectorY][sectorX], sp);
	}
}

void CS_CHAT_REQ_HEARTBEAT(Player* pPlayer)
{
	pPlayer->LastRecvedTime_ = GetTickCount64();
}