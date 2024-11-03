#pragma once
#include <cstdint>
#include "CAddressTranslator.h"
#include "CTlsObjectPool.h"

class CMessageQ;
struct Job;
#include "Job.h"

#define NO_LOCK
class CMessageQ
{
public:
	struct Node
	{
		Job* data_;
		Node* pNext_;

#pragma warning(disable : 26495)
		Node()
			:pNext_{ nullptr }
		{}
#pragma warning(default : 26495)

		Node(Job* data)
			:data_{ data },
			pNext_{ nullptr }
		{}
	};

	static CTlsObjectPool<Node, true> packetPool_;
	Node* pTailForWorker_;
	Node* pHeadForWorker_;

	Node* pTailForSingle_;
	Node* pHeadForSingle_;
public:
	unsigned long long BuffersToProcessThisFrame_ = 0;
	unsigned long long workerEnqueuedBufferCnt_ = 0;

#ifdef NO_LOCK
	static constexpr uint64_t SWAP_FLAG = 0x8000000000000000;
	uint64_t SWAP_GUARD;
#else
	SRWLOCK SWAP_GUARD;
#endif
public:
	CMessageQ();
	// meta�� ������ ���� 17��Ʈ�� ���۵� ��
	// real�� ������ ���� 17��Ʈ�� ������ ��
	void Enqueue(Job* data);
	Job* Dequeue();
	void Swap();
	void ClearAll();
};
