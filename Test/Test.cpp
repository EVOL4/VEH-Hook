// Test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <windows.h>

int main()
{
	MessageBox(NULL, L"the fact infor222", L"test SEH hook", MB_OK);

	HMODULE lib = LoadLibrary(L"..\\Debug\\VehHookDll.dll");	//小Demo,不去考虑注入时修正函数地址之类的问题
	Sleep(2000);

	MessageBox(NULL, L"the fact infor222", L"test SEH hook", MB_OK);
}

