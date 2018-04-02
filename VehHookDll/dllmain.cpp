// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
DWORD __TargetAddress = 0;
DWORD __OFFSET = 0;
HANDLE hBP = NULL;
WCHAR* _string = L"Hello Hook";

//1.断点针对某个地址及某个行为(执行代码,读写内存)
//2.异常处理函数与线程相关
//3.利用断点进行HOOK,原理就是在让目标线程在访问特定地址时产生一个断点,从而进入VEH异常处理函数,异常处理函数中判断异常类型为单步断点后,对线程的指令指针寄存器进行修改,从而执行我们的函数
//4.VEH在SEH前执行
//5.由于利用了DR0~DR3进行断点设置,所以这种方法一次只能下4个hook
//6.在调用与线程Context有关的函数时,应将函数挂起
//7.总结如下: 一次性可下4个HOOK,HOOK需下断点,需添加异常处理函数,而这两者都与线程绑定,线程的生命周期不方便确定,而且在多线程中,如何确定目标线程能访问到断点地址也是个问题,以及线程同步的问题
void FakeFunc()
{
	wprintf(L"fake\r\n");//输出至黑窗口
}

void __declspec(naked) ChangeCaption(void) {
	__asm {
		push eax
		mov  eax, _string
		mov[esp + 0ch], eax; 此时函数未开栈, 即mov ebp, esp 所以用esp获得参数地址
		mov[esp + 10h], eax
		pop eax
		jmp[__OFFSET]; 越过断点 由于无条件跳转, 不方便使用寄存器计算__TargetAddress + 2的值, 所以使用了全局变量__OFFSET
	}
}

LONG WINAPI ExceptionFilter(PEXCEPTION_POINTERS ExceptionInfo) 
{//异常过滤函数的形式
	if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
		if ((DWORD)ExceptionInfo->ExceptionRecord->ExceptionAddress == __TargetAddress) //目标处发生了中断
		{
			ExceptionInfo->ContextRecord->Eip = (DWORD)&ChangeCaption;
			return EXCEPTION_CONTINUE_EXECUTION;
		}
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

DWORD WINAPI ThreadPorcedure(PVOID Pamameter)
{
	VEH_HOOK();
	return 0;
}
VOID VEH_HOOK()//HOOK primary thread
{
	HANDLE hToolSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);//CurrentProcess
	if (hToolSnap == INVALID_HANDLE_VALUE)
	{
		return;
	}
	FILETIME  tCreate, tExit, tUser, tKernel, tPreCreate;
	tPreCreate.dwHighDateTime = 0XFFFFFFFF;
	tPreCreate.dwLowDateTime = 0x7FFFFFFF;
	HANDLE hMainThread = NULL;
	THREADENTRY32 thread_entry;
	thread_entry.dwSize = sizeof(THREADENTRY32);
	if (Thread32First(hToolSnap, &thread_entry))
	{
		do {
			if (thread_entry.th32OwnerProcessID == GetCurrentProcessId())
			{
				HANDLE hThread = OpenThread(THREAD_SET_CONTEXT | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME, FALSE, thread_entry.th32ThreadID);//WOW64必须还有THREAD_QUERY_INFORMATION标志

				if (GetThreadTimes(hThread, &tCreate, &tExit, &tKernel, &tUser))
				{
					if (CompareFileTime(&tCreate, &tPreCreate) == -1)//find primary thread
					{
						tPreCreate = tCreate;
						if (hMainThread != NULL) { CloseHandle(hMainThread); }
						hMainThread = hThread;
					}
					else
					{
						CloseHandle(hThread);
					}
				}
				else
				{
					continue;
				}
			}
		} while (Thread32Next(hToolSnap, &thread_entry));

		hBP = SetHardWareBreakPoint(GetThreadId(hMainThread), HWBP_TYPE_CODE, HWBP_SIZE_1, __TargetAddress);
		CloseHandle(hMainThread);
	}
	CloseHandle(hToolSnap);
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);
		HMODULE dll = GetModuleHandle(L"user32.dll");
		__TargetAddress = (DWORD)GetProcAddress(dll, "MessageBoxW");//HOOK MessageBoxW
		__OFFSET = __TargetAddress + 2;								//越过mov edi,edi 8b ff 同时也越过断点地址,问:要是开头不是mov edi,edi呢
		AddVectoredExceptionHandler(1, ExceptionFilter);			//插入线程异常处理链表头部,首先执行该过滤函数
																	//注意要确保在执行目标线程时调用该函数,这里DllMain()由LoadLibrary()调用,所以依然在主线程中
		HANDLE hThread = CreateThread(NULL, 0, ThreadPorcedure, NULL, 0, NULL);
	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH)
	{
		if (hBP)
		{
			bool ok = RemoveHareWareBreakPoint(hBP);
		}
	}
	return TRUE;
}