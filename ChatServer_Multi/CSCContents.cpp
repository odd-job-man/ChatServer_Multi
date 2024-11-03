#include <WinSock2.h>

#include "Packet.h"
#include "Sector.h"
#include "ChatServer.h"
#include "CMessageQ.h"
#include "Job.h"
#include "CSCContents.h"
#include "SCCContents.h"

thread_local int g_threadNumber;

extern ChatServer g_ChatServer;

SECTOR_AROUND g_sectorAround[50][50];

void CS_CHAT_REQ_LOGIN(Player* pPlayer, SmartPacket& sp)
{
	if (g_ChatServer.lPlayerNum >= Player::MAX_PLAYER_NUM)
	{
		g_ChatServer.Disconnect(pPlayer->sessionId_);
		return;
	}

	INT64 AccountNo;
	*sp >> AccountNo;
	WCHAR* pID = (WCHAR*)sp->GetPointer(sizeof(WCHAR) * Player::ID_LEN);
	WCHAR* pNickName = (WCHAR*)sp->GetPointer(sizeof(WCHAR) * Player::NICK_NAME_LEN);
	char* pSessionKey = (char*)sp->GetPointer(sizeof(char) * Player::SESSION_KEY_LEN);

	pPlayer->LastRecvedTime_ = GetTickCount64();
	pPlayer->accountNo_ = AccountNo;
	pPlayer->bLogin_ = true;

	wcscpy_s(pPlayer->ID_, Player::ID_LEN, pID);
	wcscpy_s(pPlayer->nickName_, Player::NICK_NAME_LEN, pNickName);

	SmartPacket sendPacket = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_LOGIN(en_PACKET_CS_CHAT_RES_LOGIN, 1, AccountNo, sendPacket);
	g_ChatServer.SendPacket(pPlayer->sessionId_, sendPacket);
	InterlockedIncrement(&g_ChatServer.lPlayerNum);
}

void CS_CHAT_REQ_SECTOR_MOVE(Player* pPlayer, SmartPacket& sp)
{
	INT64 accountNo;
	WORD sectorX;
	WORD sectorY;
	*sp >> accountNo >> sectorX >> sectorY;

	if (sp->IsBufferEmpty() == false)
	{
		g_ChatServer.Disconnect(pPlayer->sessionId_);
		return;
	}

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

	Packet* pPacket = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_SECTOR_MOVE(pPlayer->accountNo_, pPlayer->sectorX_, pPlayer->sectorY_, pPacket);

	Job* pREQ_SECTOR_MOVE_ADD_JOB = Job::Alloc(en_JOB_TYPE::en_JOB_CS_CHAT_REQ_SECTOR_MOVE_ADD, sectorX, sectorY, pPlayer->sessionId_, pPacket);
	Job* pREQ_SECTOR_MOVE_REMOVE_JOB;

	
	Job::Enqueue(pREQ_SECTOR_MOVE_ADD_JOB, GetOrder(sectorX, sectorY));

	if (pPlayer->bRegisterAtSector_ == false)
		pPlayer->bRegisterAtSector_ = true;
	else
	{
		pREQ_SECTOR_MOVE_REMOVE_JOB = Job::Alloc(en_JOB_TYPE::en_JOB_CS_CHAT_REQ_SECTOR_MOVE_REMOVE, pPlayer->sectorX_, pPlayer->sectorY_, pPlayer->sessionId_, nullptr);
		Job::Enqueue(pREQ_SECTOR_MOVE_REMOVE_JOB, GetOrder(pPlayer->sectorX_, pPlayer->sectorY_));
	}

	// 플레이어 섹터좌표 수정
	pPlayer->sectorX_ = sectorX;
	pPlayer->sectorY_ = sectorY;

}

void CS_CHAT_REQ_SECTOR_MOVE_JOB_ADD(Job* pJob)
{
	RegisterClientAtSector(pJob->sectorX_, pJob->sectorY_, pJob->sessionId_);
	SmartPacket sp = std::move(pJob->pPacket_);
	g_ChatServer.SendPacket(pJob->sessionId_, sp);
}

void CS_CHAT_REQ_MESSAGE(Player* pPlayer, SmartPacket& sp)
{
	INT64 accountNo;
	*sp >> accountNo;
	WORD messageLen;
	*sp >> messageLen;
	WCHAR* pMessage = (WCHAR*)sp->GetPointer(messageLen);

	if (sp->IsBufferEmpty() == false)
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

	// 잡 처리용 스레드들에게 뿌릴 직렬화버퍼를 만든다 
	Packet* pResPacket = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_MESSAGE(accountNo, pPlayer->ID_, pPlayer->nickName_, messageLen, pMessage, pResPacket);

	// Job에 직렬화버퍼를 등록함
	Job* pJob = Job::Alloc(en_JOB_TYPE::en_JOB_CS_CHAT_REQ_MESSAGE, pPlayer->sectorX_, pPlayer->sectorY_, pPlayer->sessionId_, pResPacket);

	SECTOR_AROUND* pSectorAround = &g_sectorAround[pPlayer->sectorY_][pPlayer->sectorX_];
	bool bQuadrantArr[4] = { false };
	for (int i = 0; i < pSectorAround->sectorCount; ++i)
	{
		int order = GetOrder(pSectorAround->Around[i].sectorX, pSectorAround->Around[i].sectorY);
		if (bQuadrantArr[order] == false)
		{
			bQuadrantArr[order] = true;
			Job::Enqueue(pJob, order);
		}
	}
}

__forceinline int GetPosArr(std::pair<WORD, WORD>* pOutPosArr, SECTOR_AROUND* pSectorAround, int order)
{
	int len = 0;
	WORD quadrantY = order / 2;
	WORD quadrantX = order % 2;

	for (int i = 0; i < pSectorAround->sectorCount; ++i)
	{
		if (pSectorAround->Around[i].sectorY / SECTOR_DENOMINATOR == quadrantY && pSectorAround->Around[i].sectorX / SECTOR_DENOMINATOR == quadrantX)
		{
			pOutPosArr[len] = { pSectorAround->Around[i].sectorY,pSectorAround->Around[i].sectorX };
			++len;
		}
	}

	if (len == 0)
		__debugbreak();

	return len;
}

void CS_CHAT_REQ_MESSAGE_JOB(Job* pJob, int order)
{
	SmartPacket sp = std::move(pJob->pPacket_);
	SECTOR_AROUND* pSectorAround = &g_sectorAround[pJob->sectorY_][pJob->sectorX_];
	std::pair<WORD, WORD> pPosArr[9];
	int len = GetPosArr(pPosArr, pSectorAround, order);
	SendPacket_Sector_Multiple(pJob->sessionId_, pPosArr, len, sp);
}

bool PacketProc_JOB(Job* pJob, int order)
{
	switch (pJob->jobType_)
	{
	case en_JOB_TYPE::en_JOB_CS_CHAT_REQ_SECTOR_MOVE_REMOVE:
		RemoveClientAtSector(pJob->sectorX_, pJob->sectorY_, pJob->sessionId_);
		break;
	case en_JOB_TYPE::en_JOB_CS_CHAT_REQ_SECTOR_MOVE_ADD:
		CS_CHAT_REQ_SECTOR_MOVE_JOB_ADD(pJob);
		break;
	case en_JOB_TYPE::en_JOB_CS_CHAT_REQ_MESSAGE:
		CS_CHAT_REQ_MESSAGE_JOB(pJob, order);
		break;
	case en_JOB_TYPE::en_JOB_ON_RELEASE_REMOVE_PLAYER_AT_SECTOR:
		RemoveClientAtSector(pJob->sectorX_, pJob->sectorY_, pJob->sessionId_);
		break;
	default:
		break;
	}
	return true;
}

void PQCS(int order)
{
	if (order < 0 || order >= 4)
		__debugbreak();

	Job::Swap(order);
	while (true)
	{
		Job* pJob = Job::Dequeue(order);
		if (pJob == nullptr)
			return;

		PacketProc_JOB(pJob, order);
		InterlockedIncrement(&g_ChatServer.PQCS_UPDATE_CNT_);

		if (InterlockedDecrement(&pJob->refCnt_))
			Job::Free(pJob);
	}
}

void CS_CHAT_REQ_HEARTBEAT(Player* pPlayer)
{
	pPlayer->LastRecvedTime_ = GetTickCount64();
}