#include <WinSock2.h>
#include <time.h>
#include <stdio.h>

#include "ChatServer.h"

#include "Player.h"

#pragma comment(lib,"Winmm.lib")

int g_iOldFrameTick;
int g_iFpsCheck;
int g_iTime;
int g_iFPS;
int g_iFirst;

constexpr int TICK_PER_FRAME = 10;
constexpr int FRAME_PER_SECONDS = (1000) / TICK_PER_FRAME;

extern ChatServer g_ChatServer;

void Update();

int main()
{
	timeBeginPeriod(1);
	srand((unsigned)time(nullptr));

	g_iFirst = timeGetTime();
	g_iOldFrameTick = g_iFirst;
	g_iTime = g_iOldFrameTick;
	g_iFPS = 0;
	g_iFpsCheck = g_iOldFrameTick;


	InitializeSRWLock(&Player::timeOutLock_.srw_);
	Player::timeOutLock_.timeOutThreadId_ = GetCurrentThreadId();
	int TICK_PER_FRAME = g_ChatServer.TICK_PER_FRAME_;

	while (true)
	{
		Update();
		g_iTime = timeGetTime();
		++g_iFPS;
		// 프레임 밀렷을때 
		if (g_iTime - g_iFpsCheck >= 1000)
		{
			printf("FPS : %d\n", g_iFPS);
			g_ChatServer.Monitoring();
			g_iFPS = 0;
			g_iFpsCheck += 1000;
		}

		if (g_iTime - g_iOldFrameTick >= TICK_PER_FRAME)
		{
			g_iOldFrameTick = g_iTime - ((g_iTime - g_iFirst) % TICK_PER_FRAME);
			continue;
		}

		Sleep(TICK_PER_FRAME - (g_iTime - g_iOldFrameTick));
		g_iOldFrameTick += TICK_PER_FRAME;
	}
	return 0;
}