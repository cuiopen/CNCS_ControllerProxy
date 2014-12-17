#include <iostream>
#include <WinSock2.h>
#include "NetworkDll.h"

using namespace std;

struct gmem_slot_t {
	unsigned short flag;
	unsigned short addr;
	char data[32]; 
};

struct sync_time_t{
	unsigned short flag;
	unsigned short time[9];
};

#define TSK_RUNNING	0
#define TSK_STOP		1
volatile int task;

#define GSLOT_COUNT 32
#define IN_SIZE 32*1024
#define OUT_SIZE sizeof(struct gmem_slot_t) * GSLOT_COUNT
char buf_in[IN_SIZE];
char buf_out[OUT_SIZE];
char in_mem[IN_SIZE];
char out_mem[OUT_SIZE];

#define F8_DBSYNCD_PORT 20120
#define SYNC_TIME 0x55bb
#define GSLOT_VALID 0x55aa
#define DEF_FREQ 250

HANDLE hMemReader, hMemWriter, hMemUpdate;

#pragma comment(lib,"ws2_32.lib")

int reader_handle(SOCKET s)
{
	char heap[1500];
	int mlen;
	struct sockaddr_in addr;
	int alen = sizeof(addr);
	struct gmem_slot_t *p_msg;
	char *p_data;
	struct sync_time_t *p_time;//wdt_new
	mlen = recvfrom(s, heap, sizeof(heap), 0, (struct sockaddr*)&addr, &alen);
	p_time = (struct sync_time_t*)heap;
	if (mlen != OUT_SIZE) {
		//perror("reader handle");
		if (mlen != 20) {
			return 0;
		} else if (p_time->flag == SYNC_TIME){
			//sync_time((int)p_time->time[0], (int)p_time->time[1], (int)p_time->time[2], (int)p_time->time[3], (int)p_time->time[4], 
			//	(int)p_time->time[5], (int)p_time->time[6], (int)p_time->time[7], (unsigned long)p_time->time[8]);
		}
		return 0;
	}
	p_msg = (struct gmem_slot_t*)heap;
	for (int i = 0; i < GSLOT_COUNT; i++) {
		if (p_msg->flag == GSLOT_VALID) {
			if (p_msg->addr < IN_SIZE / 32){
				p_data = buf_in + p_msg->addr * 32;
				memcpy(p_data, p_msg->data, 32);
			}
		}
		p_msg++;
	}
	return 0;
}

DWORD WINAPI MemUpdate(LPVOID p)
{
	cout <<"Start Thread MemUpdate!\n";
	while (task != TSK_STOP) {
		memcpy(buf_out, out_mem, OUT_SIZE);
		memset(out_mem, 0, sizeof(out_mem));
		memcpy(in_mem, buf_in, IN_SIZE);
		Sleep(100);
	}
	cout <<"Exit Thread MemUpdate...\n";
	return 0;
}


DWORD WINAPI MemWriter(LPVOID p)
{
	cout <<"Start Thread MemWriter!\n";
	SOCKET s;
	SOCKADDR_IN addr[2];
	bool opt;
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char FAR *>(&opt), sizeof(opt));
	memset(addr, 0, sizeof(addr));

	addr[0].sin_addr.s_addr = addr[1].sin_addr.s_addr = INADDR_BROADCAST;
	addr[0].sin_family = addr[1].sin_family = AF_INET;
	addr[0].sin_port = htons(F8_DBSYNCD_PORT);
	addr[1].sin_port = htons(F8_DBSYNCD_PORT + 1);
	int nlen = sizeof(addr[0]);

	while (task != TSK_STOP) {
		if (sendto(s, buf_out, OUT_SIZE, 0, (struct sockaddr*)&addr[0], nlen)<0){
			//perror("gmem writer - sendto 1");
		}
		if (sendto(s, buf_out, OUT_SIZE, 0, (struct sockaddr*)&addr[1], nlen)<0) {
			//perror("gmem writer - sendto 2");
		}
		Sleep(DEF_FREQ);
	}
	closesocket(s);
	cout <<"Exit Thread MemWriter...\n";
	return 0;
}

DWORD WINAPI MemReader(LPVOID p)
{
	cout <<"Start Thread MemReader!\n";
	SOCKET s[2];
	SOCKADDR_IN addr[2];
	FD_SET sset;
	
	addr[0].sin_family = addr[1].sin_family = AF_INET;
	addr[0].sin_port = htons(F8_DBSYNCD_PORT);
	addr[1].sin_port = htons(F8_DBSYNCD_PORT+1);
	addr[0].sin_addr.s_addr = addr[1].sin_addr.s_addr = INADDR_ANY;

	bool opt = true;
	s[0] = socket(AF_INET, SOCK_DGRAM, 0);
	s[1] = socket(AF_INET, SOCK_DGRAM, 0);
	setsockopt(s[0], SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char FAR *>(&opt), sizeof(opt));
	setsockopt(s[1], SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char FAR *>(&opt), sizeof(opt));
	bind(s[0], (struct sockaddr *)&addr[0], sizeof(addr[0]));
	bind(s[1], (struct sockaddr *)&addr[1], sizeof(addr[1]));
	listen(s[0], 20);
	listen(s[1], 20);
	
	TIMEVAL tout;
	tout.tv_sec = 0;
	tout.tv_usec = 100000;

	while (task != TSK_STOP) {
		int scode, scode0 = -1, scode1 = -1;
		FD_ZERO(&sset);
		FD_SET(s[0], &sset);
		FD_SET(s[1], &sset);
		if ((scode = select(s[0]>s[1]?s[0]+1:s[1]+1, &sset, 0, 0, &tout)) < 0) {
			//perror
			continue;
		}
		if (FD_ISSET(s[0], &sset)) {
			if ((scode0 = reader_handle(s[0])) >= 0)
				cout <<"Reading port " <<F8_DBSYNCD_PORT <<" success!\n";
			//perror
		}
		if (FD_ISSET(s[1], &sset)) {
			if ((scode1 = reader_handle(s[1])) >= 0)
				cout <<"Reading port " <<F8_DBSYNCD_PORT+1 <<" success!\n";
			//perror
		}
		Sleep(1);
	}
	closesocket(s[0]);
	closesocket(s[1]);
	cout <<"Exit Thread MemReader...\n";
	return 0;
}

NETWORKDLL_DLL int Init()
{
	cout << "Startup!\n";
	WSAData wsaData;
	WSAStartup(MAKEWORD(2,2), &wsaData);
	memset(buf_in, 0, sizeof(buf_in));
	memset(buf_out, 0, sizeof(buf_out));
	memset(in_mem, 0, sizeof(in_mem));
	memset(out_mem, 0, sizeof(out_mem));
	return 0;
}

NETWORKDLL_DLL int Start()
{
	task = TSK_RUNNING;
	hMemReader = CreateThread(NULL, NULL, MemReader, NULL, NULL, NULL);
	hMemUpdate = CreateThread(NULL, NULL, MemUpdate, NULL, NULL, NULL);
	hMemWriter = CreateThread(NULL, NULL, MemWriter, NULL, NULL, NULL);
	return 0;
}

NETWORKDLL_DLL int Stop()
{
	task = TSK_STOP;
	WaitForSingleObject(hMemReader, 1000);
	WaitForSingleObject(hMemWriter, 1000);
	WaitForSingleObject(hMemUpdate, 1000);
	return 0;
}

NETWORKDLL_DLL int UnInit()
{
	cout <<"Exit...\n";
	return 0;
}