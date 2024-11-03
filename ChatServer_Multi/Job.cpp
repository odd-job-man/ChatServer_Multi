#include <WinSock2.h>
#include "Job.h"
#include "CMessageQ.h"

CMessageQ Job::messageQ[4];

Job* Job::Alloc(en_JOB_TYPE jobType, WORD sectorX, WORD sectorY, ULONGLONG sessionId, Packet* pPacket)
{
	return jobPool_.Alloc(jobType, sectorX, sectorY, sessionId, pPacket);
}

void Job::Free(Job* pJob)
{
	return jobPool_.Free(pJob);
}

void Job::Enqueue(Job* pJob, int order)
{
	InterlockedIncrement(&pJob->refCnt_);
	messageQ[order].Enqueue(pJob);
}

Job* Job::Dequeue(int order)
{
	Job* pJob = messageQ[order].Dequeue();
	return pJob;
}

void Job::Swap(int order)
{
	messageQ[order].Swap();
}
