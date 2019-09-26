// TestServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "IOCPSvr.h"
#pragma comment(lib,"ws2_32.lib")
using namespace Net::IOCPServer;

static int pack = 0;


class CServer : public CIOCPSvr
{
public:
	virtual void OnHandleMsg(LPVOID pAddr,BYTE *data,int dataLen)
	{


	
		bool b=SendMsg(pAddr,data,dataLen);
		if (b==false)
		{
			exit(
				1);
		}
		pack++;
	}
	virtual void OnClientClose(LPVOID pAddr)
	{
	//	printf("OnClientClose\n");
	}
	virtual void OnClientConnect(LPVOID pAddr)
	{
		//printf("OnClientConnect\n");
	}
protected:
private:
};
int _tmain(int argc, _TCHAR* argv[])
{
	CServer * svr = new CServer();

	svr->Start(6001,3000);
	
	
	while (1)
	{
		system("cls");
		printf("数据包已处理:%d\n",pack);
		printf("当前连接:%d",svr->GetUserNumber());
		Sleep(1500);
	}
	return 0;
}

