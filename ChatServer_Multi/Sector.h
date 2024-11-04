#pragma once
#include "Player.h"

static constexpr auto NUM_OF_SECTOR_VERTICAL = 50;
static constexpr auto NUM_OF_SECTOR_HORIZONTAL = 50;
static constexpr auto SECTOR_DENOMINATOR = NUM_OF_SECTOR_VERTICAL / 2;


struct SECTOR_AROUND
{
	struct
	{
		WORD sectorX;
		WORD sectorY;
	}Around[9];
	BYTE sectorCount;
};

void GetSectorAround(SHORT sectorX, SHORT sectorY, SECTOR_AROUND* pOutSectorAround);
void SendPacket_AROUND(ULONGLONG sessionId, SECTOR_AROUND* pSectorAround, SmartPacket& sp);
void SendPacket_Sector_One(ULONGLONG sessionId, WORD sectorX, WORD sectorY, SmartPacket& sp);
void SendPacket_Sector_Multiple(ULONGLONG sessionId, std::pair<WORD, WORD>* pPosArr, int len, Packet* pPacket);
void RegisterClientAtSector(WORD sectorX, WORD sectorY, ULONGLONG sesssionId);
void RemoveClientAtSector(WORD sectorX, WORD sectorY, ULONGLONG sessionId);

__forceinline bool IsNonValidSector(WORD sectorX, WORD sectorY)
{
	return !((0 <= sectorX) && (sectorX <= NUM_OF_SECTOR_VERTICAL)) && ((0 <= sectorY) && (sectorY <= NUM_OF_SECTOR_HORIZONTAL));
}

__forceinline int GetOrder(WORD sectorX, WORD sectorY)
{
	return (sectorY / SECTOR_DENOMINATOR) * 2 + (sectorX / SECTOR_DENOMINATOR);
}
