#pragma once
#include<windows.h>
#include <unordered_map>
#include "TimeOutLock.h"

struct Player
{
	static constexpr int ID_LEN = 20;
	static constexpr int NICK_NAME_LEN = 20;
	static constexpr int SESSION_KEY_LEN = 64;
	static inline int MAX_PLAYER_NUM;
	static inline Player* pPlayerArr;
	static inline SRWLOCK playerArrLock;
	static inline SRWLOCK accountNoMapRock_[4];
	static inline TimeOutLock timeOutLock_;
	static constexpr int INITIAL_SECTOR_VALUE = 51;

	bool bUsing_;
	bool bLogin_;
	bool bRegisterAtSector_;
	WORD sectorX_;
	WORD sectorY_;
	ULONGLONG sessionId_;
	INT64 accountNo_;
	ULONGLONG LastRecvedTime_;
	WCHAR ID_[ID_LEN];
	WCHAR nickName_[NICK_NAME_LEN];

	Player()
		:bUsing_{ false }, bLogin_{ false }, bRegisterAtSector_{ false }
	{
	}

	static WORD MAKE_PLAYER_INDEX(ULONGLONG sessionId)
	{
		return sessionId & 0xFFFF;
	}
};
