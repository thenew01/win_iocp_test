// TestClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")



#pragma  pack(1)
struct pack 
{
	int check;
	int id;	
	int len;		//only body len
	char buf[256];
};
#pragma  pack()

static int clientnum;
static int recvpacknum;
static int sendPacknum;

DWORD WINAPI workThread(PVOID param)
{
	sockaddr* addr=(sockaddr*)param;
	SOCKET m_sock=INVALID_SOCKET;
	do 
	{
		m_sock = socket(AF_INET,SOCK_STREAM,0);
	} while (m_sock==INVALID_SOCKET);
	int error;
	do 
	{
		int len = sizeof(sockaddr);
		error = connect(m_sock,addr,len);
	} while (error==SOCKET_ERROR);
	clientnum++;

	int connect_id = clientnum;
	int len = 0;
	pack msg;
	while(1)
	{
		msg.check=0x1234;
		msg.len=250;
		msg.id= connect_id;
		memset(msg.buf,1,msg.len);
		len = send(m_sock, (char*)& msg, sizeof(int) * 3 + msg.len, 0);
		if( len <0 )
			break;
		sendPacknum++;
		memset(&msg,0,sizeof(pack));
		len = recv(m_sock,(char*)&msg,sizeof(pack),0);
		if( len <= 0 )
			break;
		recvpacknum++;
		Sleep(10);
	}
	

	return 0;
}
#define CONFIG_FILE	".\\config.ini"
int _tmain(int argc, _TCHAR* argv[])
{
	WSAData wsaData;
	int i=0;
	
	DWORD dwThreadNum = 0;
	DWORD dwPackNum = 1000;
	char chIP[250];
	UINT nPort = 0;
	if (WSAStartup(MAKEWORD(2,2),&wsaData)!=0)
	{
		printf("%d\n",WSAGetLastError());
		return false;
	}

	GetPrivateProfileString("config", "ip", "127.0.0.1", chIP, 250, CONFIG_FILE);
	nPort = GetPrivateProfileInt("config", "Port", 6001, CONFIG_FILE);
	dwThreadNum = GetPrivateProfileInt("config", "ThreadNum", 1000, CONFIG_FILE);
	dwPackNum = GetPrivateProfileInt("config", "PackNum", 100000000, CONFIG_FILE);
	
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr(chIP);
	addr.sin_port = htons(nPort);

	int len = sizeof(sockaddr_in);

	for (int i=0;i<dwThreadNum;i++)
	{
		CreateThread(NULL,0,workThread,(LPVOID)&addr,0,NULL);
	}
	while(1)
	{
		system("cls");
		printf("连接成功%d\n",clientnum);
		printf("接收数据包%d\n",recvpacknum);
		printf("发送数据包%d",sendPacknum);
		if (sendPacknum>dwPackNum)
		{
			exit(0);
		}
		Sleep(1000);
	}
	WSACleanup();

	return 0;
}

