#pragma once
#include <synchapi.h>
#include <processthreadsapi.h>
struct TimeOutLock
{
	SRWLOCK srw_;
	DWORD timeOutThreadId_;

	void AcquireExclusive()
	{
		AcquireSRWLockExclusive(&srw_);
		if (GetCurrentThreadId() != timeOutThreadId_)
			__debugbreak();
	}

	void ReleaseExclusive()
	{
		if (GetCurrentThreadId() != timeOutThreadId_)
			__debugbreak();

		ReleaseSRWLockExclusive(&srw_);
	}

	void AcquireShared()
	{
		if (GetCurrentThreadId() != timeOutThreadId_)
		{
			AcquireSRWLockShared(&srw_);
		}
	}

	void ReleaseShared()
	{
		if (GetCurrentThreadId() != timeOutThreadId_)
		{
			ReleaseSRWLockShared(&srw_);
		}
	}
};
