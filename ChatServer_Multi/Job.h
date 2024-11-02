#pragma once
#include "CTlsObjectPool.h"
enum class en_JOB_TYPE
{
	en_JOB_CS_CHAT_REQ_SECTOR_MOVE_REMOVE = 0,
	en_JOB_CS_CHAT_REQ_SECTOR_MOVE_ADD,
	en_JOB_CS_CHAT_REQ_MESSAGE,
	en_JOB_ON_RELEASE_REMOVE_PLAYER_AT_SECTOR,
};

struct Job
{
public:
	en_JOB_TYPE jobType_;
	WORD sectorX_;
	WORD sectorY_;
	Packet* pPacket_;
	ULONGLONG sessionId_;
	Job(en_JOB_TYPE jobType, WORD sectorX, WORD sectorY, ULONGLONG sessionId, Packet* pPacket = nullptr, INT64 accountNo = 0)
		:jobType_{ jobType }, sectorX_{ sectorX }, sectorY_{ sectorY }, sessionId_{ sessionId }, pPacket_{ pPacket }, refCnt_{ 0 }
	{}

	static inline CTlsObjectPool<Job,true> jobPool_;
	LONG refCnt_;
	static Job* Alloc(en_JOB_TYPE jobType, WORD sectorX, WORD sectorY, ULONGLONG sessionId, Packet* pPacket = nullptr)
	{
		return jobPool_.Alloc(jobType, sectorX, sectorY, sessionId, pPacket);
	}

	static void Free(Job* pJob)
	{
		return jobPool_.Free(pJob);
	}
};
