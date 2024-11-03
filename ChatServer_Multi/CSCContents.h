#pragma once
#include "Packet.h"
#include "Player.h"
#include "SCCContents.h"

void CS_CHAT_REQ_LOGIN(Player* pPlayer, SmartPacket& sp);
void CS_CHAT_REQ_SECTOR_MOVE(Player* pPlayer, SmartPacket& sp);
void CS_CHAT_REQ_MESSAGE(Player* pPlayer, SmartPacket& sp);
void CS_CHAT_REQ_HEARTBEAT(Player* pPlayer);

// Job스레드가 수행하는 Job
void CS_CHAT_REQ_SECTOR_MOVE_JOB_ADD(Job* pJob);
void CS_CHAT_REQ_MESSAGE_JOB(Job* pJob, int order);



