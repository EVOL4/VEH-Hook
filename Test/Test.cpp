// Test.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <windows.h>

int main()
{
	MessageBox(NULL, L"the fact infor222", L"test SEH hook", MB_OK);

	HMODULE lib = LoadLibrary(L"..\\Debug\\VehHookDll.dll");	//СDemo,��ȥ����ע��ʱ����������ַ֮�������
	Sleep(2000);

	MessageBox(NULL, L"the fact infor222", L"test SEH hook", MB_OK);
}

