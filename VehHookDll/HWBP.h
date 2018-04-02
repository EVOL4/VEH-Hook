#pragma once
#include <windows.h>
#include <stdio.h>
#include<stdbool.h>


enum HWBP_TYPE
{
	HWBP_TYPE_CODE,
	HWBP_TYPE_READWRITE,
	HWBP_TYPE_WRITE
};

enum HWBP_SIZE
{
	HWBP_SIZE_1,
	HWBP_SIZE_2,
	HWBP_SIZE_4,
	HWBP_SIZE_8
};

enum HWBP_DRX
{
	HWBP_DR0,
	HWBP_DR1,
	HWBP_DR2,
	HWBP_DR3
};
typedef struct _HWBP_
{
	enum HWBP_TYPE BpType;
	enum HWBP_SIZE Size;
	enum HWBP_DRX Drx;
	HANDLE hTargetThread;
	ULONG_PTR Address;
	bool bRemove;
}HWBP;
typedef HWBP* PHWBP;

static DWORD WINAPI BpThread(PVOID Parameter);
void SetBits(PULONG_PTR dw, int lowBit, int bits, int newValue);
HANDLE SetHardWareBreakPoint(DWORD ThreadId, enum HWBP_TYPE Type, enum HWBP_SIZE Size, ULONG_PTR Address);
bool RemoveHareWareBreakPoint(HANDLE hBreakPoint);

