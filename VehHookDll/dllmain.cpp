// dllmain.cpp : ���� DLL Ӧ�ó������ڵ㡣
#include "stdafx.h"
DWORD __TargetAddress = 0;
DWORD __OFFSET = 0;
HANDLE hBP = NULL;
WCHAR* _string = L"Hello Hook";

//1.�ϵ����ĳ����ַ��ĳ����Ϊ(ִ�д���,��д�ڴ�)
//2.�쳣���������߳����
//3.���öϵ����HOOK,ԭ���������Ŀ���߳��ڷ����ض���ַʱ����һ���ϵ�,�Ӷ�����VEH�쳣������,�쳣���������ж��쳣����Ϊ�����ϵ��,���̵߳�ָ��ָ��Ĵ��������޸�,�Ӷ�ִ�����ǵĺ���
//4.VEH��SEHǰִ��
//5.����������DR0~DR3���жϵ�����,�������ַ���һ��ֻ����4��hook
//6.�ڵ������߳�Context�йصĺ���ʱ,Ӧ����������
//7.�ܽ�����: һ���Կ���4��HOOK,HOOK���¶ϵ�,������쳣������,�������߶����̰߳�,�̵߳��������ڲ�����ȷ��,�����ڶ��߳���,���ȷ��Ŀ���߳��ܷ��ʵ��ϵ��ַҲ�Ǹ�����,�Լ��߳�ͬ��������
void FakeFunc()
{
	wprintf(L"fake\r\n");//������ڴ���
}

void __declspec(naked) ChangeCaption(void) {
	__asm {
		push eax
		mov  eax, _string
		mov[esp + 0ch], eax; ��ʱ����δ��ջ, ��mov ebp, esp ������esp��ò�����ַ
		mov[esp + 10h], eax
		pop eax
		jmp[__OFFSET]; Խ���ϵ� ������������ת, ������ʹ�üĴ�������__TargetAddress + 2��ֵ, ����ʹ����ȫ�ֱ���__OFFSET
	}
}

LONG WINAPI ExceptionFilter(PEXCEPTION_POINTERS ExceptionInfo) 
{//�쳣���˺�������ʽ
	if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
		if ((DWORD)ExceptionInfo->ExceptionRecord->ExceptionAddress == __TargetAddress) //Ŀ�괦�������ж�
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
				HANDLE hThread = OpenThread(THREAD_SET_CONTEXT | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME, FALSE, thread_entry.th32ThreadID);//WOW64���뻹��THREAD_QUERY_INFORMATION��־

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
		__OFFSET = __TargetAddress + 2;								//Խ��mov edi,edi 8b ff ͬʱҲԽ���ϵ��ַ,��:Ҫ�ǿ�ͷ����mov edi,edi��
		AddVectoredExceptionHandler(1, ExceptionFilter);			//�����߳��쳣��������ͷ��,����ִ�иù��˺���
																	//ע��Ҫȷ����ִ��Ŀ���߳�ʱ���øú���,����DllMain()��LoadLibrary()����,������Ȼ�����߳���
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