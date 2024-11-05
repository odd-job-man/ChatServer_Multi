#pragma once
#include <NetServer.h>
#include "HMonitor.h"

class ChatServer : public NetServer
{
public:
	ChatServer();
	static inline HMonitor monitor;
	virtual BOOL OnConnectionRequest();
	virtual void* OnAccept(ULONGLONG id);
	virtual void OnRelease(ULONGLONG id);
	virtual void OnRecv(ULONGLONG id, Packet* pPacket);
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket);
	virtual void OnPost(int order);
	void Monitoring();
	void DisconnectAll();
	LONG REQ_MESSAGE_TPS = 0;
	LONG RES_MESSAGE_TPS = 0;
	LONG PQCS_UPDATE_CNT_ = 0;
	HANDLE hUpdateThreadEvent_;
	LONG updateThreadWakeCount_ = 0;
	LONG TICK_PER_FRAME_;
	ULONGLONG SESSION_TIMEOUT_;
	ULONGLONG PLAYER_TIMEOUT_;
};
