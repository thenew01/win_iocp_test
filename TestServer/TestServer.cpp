// TestServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <map>
#include <condition_variable>
using namespace std;

#include "IOCPSvr.h"
#include "MsgQueue.h"
using namespace Net;
#pragma comment(lib,"ws2_32.lib")
using namespace Net::IOCPServer;

static int pack = 0;

std::condition_variable cond_connections;
std::mutex mutex_connections;
std::condition_variable cond_msg;
std::mutex mutex_msg;

std::map<char*, char*> gmapMsg;
std::map<void*, void*> gmapConnections;

class CServer : public CIOCPSvr
{
public:
	virtual void OnHandleMsg(LPVOID pAddr,BYTE *data,int dataLen)
	{
		CClientContext* pClient = (CClientContext*)pAddr;

		/*mutex_connections.lock();
		std::map<void*, void*>::iterator it = gmapConnections.find(pAddr);
		if (it != gmapConnections.end())
		{
			it->second = data;
		}
		mutex_connections.unlock();*/

		bool b=SendMsg(pAddr,data,dataLen);
		if (b==false)
		{
			exit(1);
		}
		pack++;		
		
	}

	virtual void OnClientClose(LPVOID pAddr)
	{
		mutex_connections.lock();
		std::map<void*, void*>::iterator it = gmapConnections.find(pAddr);
		if (it != gmapConnections.end())
		{
			gmapConnections.erase(it);
		}
		mutex_connections.unlock();
	}
	virtual void OnClientConnect(LPVOID pAddr)
	{
		mutex_connections.lock();
		std::map<void*, void*>::iterator it = gmapConnections.find(pAddr);
		if (it == gmapConnections.end())
		{
			gmapConnections.insert(std::pair<void*, void*>(pAddr, 0));
		}
		mutex_connections.unlock();

	}
protected:
private:
};
int _tmain(int argc, _TCHAR* argv[])
{
	CServer * svr = new CServer();
	svr->Start(5001,3000);	
	
	while (1)
	{
		system("cls");
		printf("数据包已处理:%d\n",pack);
		printf("当前连接:%d\n",svr->GetUserNumber());
		Sleep(1000);
	}
	return 0;
}

