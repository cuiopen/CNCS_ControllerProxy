#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <Windows.h>
#include "NetworkDll.h"

#pragma comment(lib, "NetworkDLL.lib")

int main()
{
	Init();
	Start();
	Sleep(1000*10);
	Stop();
	UnInit();
	return 0;
}