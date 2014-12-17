#ifndef __NETWORKDLL_H__
#define __NETWORKDLL_H__

#ifdef NETWORKDLL_EXPORTS
#define NETWORKDLL_DLL extern "C" _declspec(dllexport)
#else
#define NETWORKDLL_DLL extern "C" _declspec(dllimport)
#endif

NETWORKDLL_DLL int Init();
NETWORKDLL_DLL int Start();
NETWORKDLL_DLL int Stop();
NETWORKDLL_DLL int UnInit();

#endif