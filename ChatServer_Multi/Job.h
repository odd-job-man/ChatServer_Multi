#pragma once
#include "CTlsObjectPool.h"
#include "Packet.h"
#include "CMessageQ.h"

#include "list"

enum class en_JOB_TYPE
{
	en_JOB_CS_CHAT_REQ_SECTOR_MOVE_REMOVE = 0,
	en_JOB_CS_CHAT_REQ_SECTOR_MOVE_ADD,
	en_JOB_CS_CHAT_REQ_MESSAGE,
	en_JOB_ON_RELEASE_REMOVE_PLAYER_AT_SECTOR,
};

struct JobLog
{
	en_JOB_TYPE event;
	WORD sectorX_;
	WORD sectorY_;
	int order;
};

struct Job
{
public:
	en_JOB_TYPE jobType_;
	WORD sectorX_;
	WORD sectorY_;
	Packet* pPacket_;
	ULONGLONG sessionId_;
	LONG refCnt_;
	static CMessageQ messageQ[4];
	static inline CTlsObjectPool<Job,true> jobPool_;

	Job(en_JOB_TYPE jobType, WORD sectorX, WORD sectorY, ULONGLONG sessionId, Packet* pPacket = nullptr)
		:jobType_{ jobType }, sectorX_{ sectorX }, sectorY_{ sectorY }, sessionId_{ sessionId }, pPacket_{ pPacket }, refCnt_{ 0 }
	{}

	static Job* Alloc(en_JOB_TYPE jobType, WORD sectorX, WORD sectorY, ULONGLONG sessionId, Packet* pPacket = nullptr);
	static void Free(Job* pJob);
	static void Enqueue(Job* pJob, int order);
	static Job* Dequeue(int order);
	static void Swap(int order);

	//static void WRITE_JOB_LOG(en_JOB_TYPE event, WORD sectorX, WORD sectorY, WORD accountNo)
	//{
	//	JobListArr[accountNo].push_back(JobLog{event, sectorX, sectorY, (sectorY / 25) * 2 + sectorX / 25});
	//}
};
