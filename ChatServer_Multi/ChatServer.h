#pragma once
#include <NetServer.h>
#include "HMonitor.h"

class ChatServer : public NetServer
{
public:
	ChatServer();
	virtual BOOL OnConnectionRequest();
	virtual void* OnAccept(ULONGLONG id);
	virtual void OnRelease(ULONGLONG id);
	virtual void OnRecv(ULONGLONG id, Packet* pPacket);
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket);
	virtual void OnPost(int order);
	void Monitoring();
	void DisconnectAll();
	HANDLE hUpdateThreadEvent_;
	LONG updateThreadWakeCount_ = 0;
	LONG TICK_PER_FRAME_;
	ULONGLONG SESSION_TIMEOUT_;
	ULONGLONG PLAYER_TIMEOUT_;
	ULONGLONG RECV_TOTAL_ = 0;
	ULONGLONG PROCESS_CPU_TICK_ELAPSED = 0;
	ULONGLONG PROCESS_CPU_TICK_TIME_DIFF = 0;
	static inline HMonitor monitor;
};
