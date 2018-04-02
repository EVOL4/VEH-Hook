#include "HWBP.h"

void test()
{
	char c[100] = { 0 };
	lstrcpyA(c, "hello ");
	HANDLE bp = NULL;

	bp = SetHardWareBreakPoint(GetCurrentThreadId(), HWBP_TYPE_READWRITE, HWBP_SIZE_1, (ULONG_PTR)c);

	_try
	{
		volatile CHAR x = c[1];
	}
		_except(GetExceptionCode() == STATUS_SINGLE_STEP)
	{
		MessageBoxA(NULL, "HIT", "HardWareBreakPoint", MB_OK);
	}

	if (!RemoveHareWareBreakPoint(bp))
	{
		printf("Remove failed\r\n");
	}
	_try
	{
		volatile CHAR x = c[1];
	}
		_except(GetExceptionCode() == STATUS_SINGLE_STEP)
	{
		MessageBoxA(NULL, "HIT", "HardWareBreakPoint", MB_OK);//never occur
	}
	return;
}

void SetBits(PULONG_PTR dw, int lowBit, int bits, int newValue)//将dw从lowBit开始的bits长度的值置为newValue
{
	DWORD_PTR mask = (1 << bits) - 1;
	*dw = (*dw & ~(mask << lowBit)) | (newValue << lowBit);
}
HANDLE SetHardWareBreakPoint(DWORD ThreadId, enum HWBP_TYPE Type, enum HWBP_SIZE Size, ULONG_PTR Address)
{
	PHWBP hwbp = (PHWBP)malloc(sizeof(HWBP));
	ZeroMemory(hwbp, sizeof(HWBP));

	hwbp->Address = Address;
	hwbp->BpType = Type;
	hwbp->bRemove = false;
	hwbp->Size = Size;

	DWORD DesiredAccess = THREAD_SUSPEND_RESUME | THREAD_SET_CONTEXT | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION;//WOW64 必须 THREAD_QUERY_INFORMATION
	hwbp->hTargetThread = OpenThread(DesiredAccess, FALSE, ThreadId);
	if (hwbp->hTargetThread == NULL)
	{
		printf("OpenThread ERROR: %x\r\n", GetLastError());
		return NULL;
	}

	HANDLE hThread = CreateThread(NULL, 0, BpThread, (PVOID)hwbp, 0, NULL);
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);

	return (HANDLE)hwbp;
}

bool RemoveHareWareBreakPoint(HANDLE hBreakPoint)
{
	PHWBP pHwbp = (PHWBP)hBreakPoint;
	pHwbp->bRemove = true;

	HANDLE hThread = CreateThread(NULL, 0, BpThread, (PVOID)pHwbp, 0, NULL);
	if (hThread == NULL)
	{
		return false;
	}
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	if (pHwbp->hTargetThread)
	{
		CloseHandle(pHwbp->hTargetThread);
		pHwbp->hTargetThread = NULL;
	}
	free(pHwbp);
	pHwbp = NULL;
	return true;
}

static DWORD WINAPI BpThread(PVOID Parameter)
{
	PHWBP hwbp = (PHWBP)Parameter;
	CONTEXT context = { 0 };
	context.ContextFlags = CONTEXT_DEBUG_REGISTERS;

	if (SuspendThread(hwbp->hTargetThread) == -1)
	{
		return 0;
	}

	if (!GetThreadContext(hwbp->hTargetThread, &context))
	{
		return 0;
	}
	context.Dr6 = 0;

	if (hwbp->bRemove)
	{
		// Remove

		int FlagBit = 0;

		switch (hwbp->Drx)
		{
		case HWBP_DR0:
			FlagBit = 0;
			context.Dr0 = 0;
			break;
		case HWBP_DR1:
			FlagBit = 2;
			context.Dr1 = 0;
			break;
		case HWBP_DR2:
			FlagBit = 4;
			context.Dr2 = 0;
			break;
		case HWBP_DR3:
			FlagBit = 6;
			context.Dr3 = 0;
			break;
		default:
			break;
		}
		context.Dr7 &= ~(1 << FlagBit);
	}
	else
	{
		bool Dr0Busy = (context.Dr7 & 1);
		bool Dr1Busy = (context.Dr7 & 4);
		bool Dr2Busy = (context.Dr7 & 16);
		bool Dr3Busy = (context.Dr7 & 64);

		if (!Dr0Busy)
		{
			hwbp->Drx = HWBP_DR0;
			context.Dr0 = (ULONG_PTR)hwbp->Address;
		}
		else if (!Dr1Busy)
		{
			hwbp->Drx = HWBP_DR1;
			context.Dr1 = (ULONG_PTR)hwbp->Address;
		}
		else if (!Dr2Busy)
		{
			hwbp->Drx = HWBP_DR2;
			context.Dr2 = (ULONG_PTR)hwbp->Address;
		}
		else if (!Dr3Busy)
		{
			hwbp->Drx = HWBP_DR3;
			context.Dr3 = (ULONG_PTR)hwbp->Address;
		}
		else
		{
			//No Debug Registers Available
			ResumeThread(hwbp->hTargetThread);
			return 0;
		}
		UINT RW = 0;
		switch (hwbp->BpType)
		{
		case HWBP_TYPE_CODE:
			RW = 0;
			break;
		case HWBP_TYPE_WRITE:
			RW = 1;
			break;
		case HWBP_TYPE_READWRITE:
			RW = 3;
			break;
		default:
			break;
		}

		UINT Len = 0;
		switch (hwbp->Size)
		{
		case HWBP_SIZE_1:
			Len = 0;
			break;
		case HWBP_SIZE_2:
			Len = 1;
			break;
		case HWBP_SIZE_4:
			Len = 3;
			break;
		case HWBP_SIZE_8:
			Len = 2;
			break;
		default:
			break;
		}

		SetBits(&context.Dr7, 16 + hwbp->Drx * 4, 2, RW);
		SetBits(&context.Dr7, 18 + hwbp->Drx * 4, 2, Len);
		SetBits(&context.Dr7, hwbp->Drx * 2, 1, 1);//Lx = 1
	}

	context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	if (!SetThreadContext(hwbp->hTargetThread, &context))
	{
		return 0;
	}

#ifdef _DEBUG
	context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	GetThreadContext(hwbp->hTargetThread, &context);		//Have a look;
#endif

	ResumeThread(hwbp->hTargetThread);

	return 0;
}