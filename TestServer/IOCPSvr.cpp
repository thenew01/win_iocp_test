#include "StdAfx.h"
#include ".\iocpsvr.h"
#include <algorithm>

#define NET_BUFFER_SIZE 4096*4

bool DisableNagle(SOCKET pSock)
{
	//// ����Nagle�㷨
	//char bNagleValue = 1;
	////char bNagleValue = 0;
	//if(SOCKET_ERROR == setsockopt(pSock,IPPROTO_TCP,TCP_NODELAY,(char*)&bNagleValue,sizeof(bNagleValue)))
	//{
	//	return false;
	//}
	//// ���û�����
	//int nBufferSize = NET_BUFFER_SIZE;
	//if(SOCKET_ERROR == setsockopt(pSock,SOL_SOCKET,SO_SNDBUF,(char*)&nBufferSize,sizeof(nBufferSize)))
	//{
	//	return false;
	//}
	//nBufferSize = NET_BUFFER_SIZE;
	//if(SOCKET_ERROR == setsockopt(pSock,SOL_SOCKET,SO_RCVBUF,(char*)&nBufferSize,sizeof(nBufferSize)))
	//{
	//	return false;
	//}

	//nBufferSize = 0;
	//if(SOCKET_ERROR == setsockopt(pSock,SOL_SOCKET,SO_RCVTIMEO,(char*)&nBufferSize,sizeof(nBufferSize)))
	//{
	//	return false;
	//}

	//if(SOCKET_ERROR == setsockopt(pSock,SOL_SOCKET,SO_SNDTIMEO,(char*)&nBufferSize,sizeof(nBufferSize)))
	//{
	//	return false;
	//}

	return true;
}

namespace Net
{
	namespace IOCPServer
	{
		CIOCPSvr::CIOCPSvr(void):m_iNowOLUser(0),m_iMaxOLUser(0)
		{
		}

		CIOCPSvr::~CIOCPSvr(void)
		{
		}

		bool CIOCPSvr::Stop()
		{
			m_bSvrRunning = false;

			printf("iocpsvr will stop...\n");
			return true;
		}

		bool CIOCPSvr::Start(int port,int maxOLClient,bool sendOrder/*=false*/,bool readOrder/*=false*/)
		{
			//init net
			m_iMaxOLUser = maxOLClient;
			WSAData wsaData;
			if (WSAStartup(MAKEWORD(2,2),&wsaData)!=0)
			{
				printf("%d\n",WSAGetLastError());
				return false;
			}

			//createiocp
			SOCKET sock = socket(AF_INET,SOCK_STREAM,0);
			if (sock == INVALID_SOCKET)
			{
				printf("%d\n",WSAGetLastError());
				return false;
			}

			m_hIOCP = CreateIoCompletionPort((HANDLE)sock,NULL,0,0);
			if (m_hIOCP == NULL)
			{
				printf("%d\n",WSAGetLastError());
				closesocket(sock);
				return false;
			}
			closesocket(sock);

			//create worker
			SYSTEM_INFO si;
			GetSystemInfo(&si);
			int processNum = si.dwNumberOfProcessors;
			if (processNum==1)
			{
				processNum=2;
			}
			HANDLE hThread;
			DWORD dwThreadID;
			for (int i=0;i<processNum;i++)
			{
				hThread = CreateThread(NULL,0,Worker,this,0,&dwThreadID);
				if (!hThread)
				{
					printf("%d\n",WSAGetLastError());
					return false;
				}
			}
			//create listen
			m_SockListen = WSASocket(AF_INET,SOCK_STREAM,0,NULL,0,WSA_FLAG_OVERLAPPED);
			if (m_SockListen == INVALID_SOCKET)
			{
				printf("%d\n",WSAGetLastError());
				return false;
			}
			sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);
			addr.sin_addr.s_addr = htonl(INADDR_ANY);

			if (bind(m_SockListen,(sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR)
			{
				printf("%d\n",WSAGetLastError());
				return false;
			}

			if (listen(m_SockListen,1024)==SOCKET_ERROR)
			{
				printf("%d\n",WSAGetLastError());
				return false;
			}

			hThread = CreateThread(NULL,0,Listen,this,0,&dwThreadID);
			if (!hThread)
			{
				printf("%d\n",WSAGetLastError());
				return false;
			}

			m_bSvrRunning = true;

			m_bReadOrder = readOrder;
			m_bSendOrder = sendOrder;
			

			printf("IOCPSvr will running...\n");
			return true;

		}

		HANDLE CIOCPSvr::GetIOCPHandle()
		{
			return m_hIOCP;
		}
		
		bool CIOCPSvr::IsSvrRunning()
		{
			return m_bSvrRunning;
		}

		void CIOCPSvr::ReleaseContext(CClientContext*pContext)
		{
			if (!pContext)
			{
				return;
			}

			//out inter
			OnClientClose(pContext);

			if (1)
			{
				AutoLock _lock(&m_ContextMapLock);
				ClientContextMapIt it = m_UsedContextMap.find(pContext->m_sock);
				if (it!=m_UsedContextMap.end())
				{
					m_UsedContextMap.erase(it);
					m_iNowOLUser--;
				}

				if (pContext->m_sock != INVALID_SOCKET)
				{
					closesocketex(pContext->m_sock);
				}
			}

			if (1)
			{
				AutoLock _lock(&m_ContextLock);
				pContext->m_sock = INVALID_SOCKET;
				//pContext->m_IOArray.Clear();				
				pContext->m_sendBuf.clear();
				pContext->m_recvBuf.clear();
				pContext->m_iSendSequence = 0;
				pContext->m_iRecvSequence = 0;
				pContext->m_iRecvSequenceCurrent = 0;
				pContext->m_iSendSequenceCurrent = 0;
				m_FreeContextList.push_front(pContext);
			}

		}

		void CIOCPSvr::ReleaseIOBuffer(CIOBuffer*pBuffer)
		{
			if (!pBuffer)
			{
				return;
			}

			if (!pBuffer->ReleaseRef())
			{
			//	printf("pBuffer reference err =%d\n",pBuffer->GetRef());
				return;
			}

			if (1)
			{
				AutoLock _lock(&m_UsedIOBufferLock);

				IOBufferListIt it = find(m_UsedIOBuffer.begin(),m_UsedIOBuffer.end(),pBuffer);
				if (it!=m_UsedIOBuffer.end())
				{
					m_UsedIOBuffer.erase(it);
				}
			}

			if (1)
			{
				AutoLock _lock(&m_FreeIOBufferLock);

				m_FreeIOBuffer.push_front(pBuffer);
			}

			//printf("pBuffer�ͷ�ok\n");
		}

		CClientContext*	CIOCPSvr::AllocateClient(SOCKET sock)
		{
			CClientContext * pContext = NULL;

			if (1)
			{
				AutoLock _lock(&m_ContextLock);		

				if (m_FreeContextList.empty())
				{
					pContext = new CClientContext();
				}
				else
				{
					pContext = m_FreeContextList.front();
					m_FreeContextList.pop_front();
				}

			}

			if (pContext)
			{
				AutoLock _lock(&pContext->m_lock);

				pContext->m_sock = sock;
				pContext->m_iSendSequence = 0;
				pContext->m_iRecvSequence = 0;
				pContext->m_iSendSequenceCurrent = 0;
				pContext->m_iRecvSequenceCurrent = 0;
				//pContext->m_IOArray.Clear();
				pContext->m_sendBuf.clear();
				pContext->m_recvBuf.clear();
			}

			if (pContext)
			{
				AutoLock _lock(&m_ContextMapLock);

				ClientContextMapIt it = m_UsedContextMap.find(sock);

				if (it != m_UsedContextMap.end())
				{
					ReleaseContext(pContext);
					return NULL;
				}

				m_UsedContextMap.insert(ClientContextMap::value_type(sock,pContext));
				m_iNowOLUser++;

				pContext->m_loopBuffer.Init(IO_BUFFER_LEN);
			}

			return pContext;
		}
		CIOBuffer * CIOCPSvr::AllocateBuffer(IOType type)
		{
			CIOBuffer*pBuffer = NULL;

			if (1)
			{
				AutoLock _lock(&m_FreeIOBufferLock);

				if (m_FreeIOBuffer.empty())
				{
					pBuffer = new CIOBuffer();
				}
				else
				{
					pBuffer = m_FreeIOBuffer.front();
					m_FreeIOBuffer.pop_front();
				}
			}

			if (pBuffer)
			{
				memset(pBuffer,0,sizeof(CIOBuffer));
				pBuffer->m_ioType = type;

				pBuffer->AddRef();

				AutoLock _lock(&m_UsedIOBufferLock);
				m_UsedIOBuffer.push_front(pBuffer);
			}

			return pBuffer;

		}


		DWORD WINAPI CIOCPSvr::Worker(LPVOID param)
		{
			CIOCPSvr *pThis = (CIOCPSvr*)param;

			HANDLE hIOCP = pThis->GetIOCPHandle();

			DWORD dwIOSize;
			CClientContext*pContext;
			CIOBuffer*pBuffer;
			LPOVERLAPPED lpOverlapped;

			bool bError = false;

			while (!bError)//pThis->IsSvrRunning())
			{
				pBuffer = NULL;
				pContext = NULL;
				BOOL bRet = GetQueuedCompletionStatus(
					hIOCP,
					&dwIOSize,
					(PULONG_PTR)&pContext,
					&lpOverlapped,
					INFINITE);

				if (!bRet)
				{
					DWORD dwIOError = WSAGetLastError();
					//printf("Worker errcode=%d\n",dwIOError);
					if (pContext)
					{
						pThis->ReleaseContext(pContext);
					}					
					if (lpOverlapped)
					{
						pBuffer = CONTAINING_RECORD(lpOverlapped, CIOBuffer, m_overlapped);
					}
					if (pBuffer)
					{
						pThis->ReleaseIOBuffer(pBuffer);
					}
					continue;

				}
				else if(lpOverlapped && pContext)
				{
					pBuffer = CONTAINING_RECORD(lpOverlapped,CIOBuffer,m_overlapped); //data per io, not data per handle
					//if the buffer is a member of context,so data per handle
					if (pBuffer)
					{
						pThis->ProcessIOMessage(pContext,dwIOSize,pBuffer);
					}
				}

				if (!pContext && !lpOverlapped && !pThis->IsSvrRunning())
				{
					bError = true;
					break;
				}			
			}			
			return 0;
		}

		DWORD WINAPI CIOCPSvr::Listen(LPVOID param)
		{

			CIOCPSvr *pThis = (CIOCPSvr*)param;

			HANDLE hIOCP = pThis->GetIOCPHandle();

			while (pThis->IsSvrRunning())
			{
				//printf("new coming...\n");

				SOCKET sock = WSAAccept(pThis->m_SockListen,NULL,NULL,NULL,0);
				if (pThis->m_iNowOLUser>=pThis->m_iMaxOLUser)
				{
					closesocketex(sock);
				}
				if (sock == INVALID_SOCKET)
				{
					printf("%d\n",WSAGetLastError());
					continue;
				}

				DisableNagle(sock);

				//bind ClientContext
				CClientContext * pClient = pThis->AllocateClient(sock);

				if (!pClient)
				{
					continue;
				}

				if (NULL == CreateIoCompletionPort((HANDLE)sock,pThis->m_hIOCP,(DWORD)pClient,NULL))
				{
					printf("%d\n",WSAGetLastError());
					continue;
				}

				//bind iobuffer
				CIOBuffer * pBuffer = pThis->AllocateBuffer(itInit);				

				if (!pBuffer)
				{
					pThis->ReleaseContext(pClient);
					continue;
				}

				//pBuffer->AddRef();

				if (!PostQueuedCompletionStatus(pThis->m_hIOCP,0,(DWORD)pClient,&pBuffer->m_overlapped))
				{
					if(WSAGetLastError() != ERROR_IO_PENDING)
					{
						pThis->ReleaseContext(pClient);
						pThis->ReleaseIOBuffer(pBuffer);
						continue;
					}
				}
			}

			return 0;
		}

		void CIOCPSvr::ProcessIOMessage(CClientContext*pContext,DWORD dwSize,CIOBuffer*pBuffer)
		{
			//handle net msg

			if (pContext->m_sock == INVALID_SOCKET)
			{
				ReleaseContext(pContext);
				ReleaseIOBuffer(pBuffer);
				return;
			}

			if(!pBuffer)
				return;
			/*
			 �����Ƿ�������,һ��Ӧ�ò��ڴ汻��ס,����ڴ�Ͳ��ܴ������ڴ��ҳ��ȥ.
			 ����ϵͳ��������Щ����ס���ڴ������,һ���ﵽ�������,�ͻ᷵��WSAENOBUFS����.
			 ���Ӧ�ò���ÿһ�������Ϸ�������ص�IO����,����������������,
			 �ܿ��ܾʹﵽ������Ƶ�ֵ.һ��������Ϊ�ص�IO���������ϵ�����,
			 ��һ��������Ϊ��ǰϵͳ�ķ�ҳ��λ�ǹ̶���,
			 ��ʹӦ�ò�ֻ��һ���ֽڵĲ�������,����ϵͳ��Ȼ��Ҫ����һҳ(һ����4K)�Ĵ���.
			 ���������ϣ���ܴ���ǳ��ಢ������,������ÿ�����ӵĶ�����ʱͶ��һ��0�ֽڵĶ�����,
			 ����WSARecv��ʱ��ΪlpBuffers��dwBufferCount�ֱ𴫵�NULL��0����.
			 �������Ͳ�������ڴ�������������Դ��������,
			 ��Ϊû���ڴ���Ҫ������,һ�������ݱ��յ�,����ϵͳ�ͻ�Ͷ�����֪ͨ.
			 ���ʱ�����˾Ϳ���ȥ�׽��ֽ��ܻ�����ȡ������.
			 �����ַ������Ե�֪�����ж������ݿ��Զ�,һ����ͨ��ioctlsocket���FIONREAD����ȥ"��ѯ",
			 ��һ�־���һֱ��,ֱ���õ�WSAEWOULDBLOCK����,�ͱ�ʾû�����ݿɶ���.
			 ��һ�����ڷ������ݵ�ʱ��,��Ȼ���Բ������ַ���,ԭ�����ڶԶ˵�Ӧ�ÿ���Ч�ʷǳ�����,
			 ����������ĳ����ѭ��,���¶Է�������IO��ٳٲ�����recv/WSARecv.��TCPЭ�鱾�������,
			 �������Ҫ���͵����ݾͻ�һֱPENDING,���������ڴ汻�ں���ס.����0�ֽڷ��ͷ�ʽ��,
			 Ӧ�ò���Ͷ��һ���յ�WSASend,��ʾϣ����������,����ϵͳһ���ж�������ӿ���д��,
			 ��Ͷ��һ�����֪ͨ,��ʱ����Է���Ͷ������,���ҷ��ͻ������Ĵ�С�ǿ�֪��,��������ڴ�����������.*/
			 /*
			 OnZeroByteRead(ClientContext *pContext) the workaround
			 the WSAENOBUFS error problem.


			 This Bug was a very difficult one.. When I stress-tested this server code the
			 server hung after a while. I first thought that this was a memleak problem or
			 deadlock problem. But after a some hours I found that it is because of the system
			 WSAENOBUFS error.

			 With every overlapped send or receive operation, it is probable that
			 the data buffers submitted will be locked. When memory is locked, it
			 cannot be paged out of physical memory. The operating system imposes
			 a limit on the amount of memory that may be locked. When this limit
			 is reached, overlapped operations will fail with the WSAENOBUFS error.
			 If a server posts many overlapped receives on each connection, this
			 limit will be reached as the number of connections grow. If a server
			 anticipates handling a very high number of concurrent clients, the server
			 can post a single zero byte receive on each connection. Because there is
			 no buffer associated with the receive operation, no memory needs to be
			 locked. With this approach, the per-socket receive buffer should be left
			 intact because once the zero-byte receive operation completes, the server
			 can simply perform a non-blocking receive to retrieve all the data buffered
			 in the socket's receive buffer. There is no more data pending when the
			 non-blocking receive fails with WSAEWOULDBLOCK. This design would be for
			 servers that require the maximum possible concurrent connections while
			 sacrificing the data throughput on each connection.
			 Of course, the more you are aware of how the clients will be interacting
			 with the server, the better. In the previous example, a non-blocking receive
			 is performed once the zero-byte receive completes to retrieve the buffered
			 data. If the server knows that clients send data in bursts, then once the
			 zero-byte receive completes, it may post one or more overlapped receives
			 in case the client sends a substantial amount of data
			 (greater than the per-socket receive buffer that is 8 KB by default). */
			switch (pBuffer->m_ioType)
			{
			case itInit:
				OnInitialize(pContext,dwSize,pBuffer);
				break;
			case itReadZero:
				OnReadZero(pContext,dwSize,pBuffer);
				break;
			case itReadZeroComplete:
				OnReadZeroComplete(pContext,dwSize,pBuffer);
				break;
			case itRead:
				OnRead(pContext,dwSize,pBuffer);
				break;
			case itReadComplete:
				OnReadComplete(pContext,dwSize,pBuffer);
				break;
			case itWrite:
				OnWrite(pContext,dwSize,pBuffer);
				break;
			case itWriteComplete:
				OnWriteComplete(pContext,dwSize,pBuffer);
				break;
			default:
				ReleaseIOBuffer(pBuffer);
				break;
			}
		}

		void CIOCPSvr::OnInitialize(CClientContext*pContext,DWORD dwSize,CIOBuffer*pBuffer)
		{
			if (!IsSvrRunning())
			{
				return;
			}

			OnClientConnect(pContext);

			if (!pBuffer)
			{
				pBuffer = AllocateBuffer(itReadZero);
				if (!pBuffer)
				{
					ReleaseContext(pContext);
					return;
				}
			}
			
			pBuffer->m_ioType = itReadZero;						
			memset(&pBuffer->m_overlapped,0,sizeof(OVERLAPPED));

			if (!PostQueuedCompletionStatus(m_hIOCP,0,(DWORD)pContext,&pBuffer->m_overlapped))
			{
				if (WSAGetLastError() != ERROR_IO_PENDING)
				{
					ReleaseIOBuffer(pBuffer);
					ReleaseContext(pContext);
					return;
				}
			}
		}

		void CIOCPSvr::OnReadZero(CClientContext*pContext,DWORD dwSize,CIOBuffer*pBuffer)
		{
			DWORD dwIOSize = 0;
			DWORD dwFlags = 0;

			if (!pBuffer)
			{
				pBuffer = AllocateBuffer(itReadZeroComplete);
				if (!pBuffer)
				{
					ReleaseContext(pContext);
					return;
				}
			}

			pBuffer->m_ioType = itReadZeroComplete;
			memset(&pBuffer->m_overlapped,0,sizeof(OVERLAPPED));
			pBuffer->SetupReadZero();			

			BOOL ret = WSARecv(pContext->m_sock,&pBuffer->m_wsaBuf,1,&dwIOSize,
				&dwFlags,&pBuffer->m_overlapped,NULL);

			int errCode = WSAGetLastError();

			if (SOCKET_ERROR ==  ret && errCode != WSA_IO_PENDING)
			{
				printf("errid=%d\n",errCode);

				ReleaseIOBuffer(pBuffer);
				ReleaseContext(pContext);
				return;
			}

		}

		void CIOCPSvr::OnReadZeroComplete(CClientContext*pContext,DWORD dwSize,CIOBuffer*pBuffer)
		{
			if (!pBuffer)
			{
				pBuffer = AllocateBuffer(itRead);
				if (!pBuffer)
				{
					ReleaseContext(pContext);
					return;
				}
			}

			pBuffer->m_ioType = itRead;
			pBuffer->SetupRead();
			memset(&pBuffer->m_overlapped,0,sizeof(OVERLAPPED));			
			if (!PostQueuedCompletionStatus(m_hIOCP,0,(DWORD)pContext,&pBuffer->m_overlapped))
			{
				if (WSAGetLastError() != ERROR_IO_PENDING)
				{
					ReleaseContext(pContext);
					ReleaseIOBuffer(pBuffer);
					return;
				}
			}
		}
		
		void CIOCPSvr::OnRead(CClientContext*pContext,DWORD dwSize,CIOBuffer*pBuffer)
		{
			if (!pBuffer)
			{
				pBuffer = AllocateBuffer(itReadComplete);
				if (!pBuffer)
				{
					ReleaseIOBuffer(pBuffer);
					ReleaseContext(pContext);
					return;
				}			
			}

			pBuffer->m_ioType = itReadComplete;
			memset(&pBuffer->m_overlapped,0,sizeof(OVERLAPPED));
			pBuffer->SetupRead();
			DWORD dwIOSize = 0;
			DWORD dwFlags = 0;
			if (SOCKET_ERROR == WSARecv(pContext->m_sock, &pBuffer->m_wsaBuf, 1, &dwIOSize,
				&dwFlags, &pBuffer->m_overlapped, NULL))
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					ReleaseContext(pContext);
					ReleaseIOBuffer(pBuffer);
				}
				return;
			}
		}
		void CIOCPSvr::OnReadComplete(CClientContext*pContext,DWORD dwSize,CIOBuffer*pBuffer)
		{
			if (dwSize == 0 || pBuffer == NULL)
			{
				ReleaseIOBuffer(pBuffer);
				ReleaseContext(pContext);
				return;
			}

			bool msgErr = false;

			while (pBuffer)
			{
				if (!msgErr)
				{
					pBuffer->m_nUsed += dwSize;

					OnHandleMsg(pContext, (BYTE*)pBuffer->m_data, pBuffer->m_nUsed);

					////handle msg				
					//if (!pContext->m_loopBuffer.PushBack((const char*)pBuffer->m_data, pBuffer->m_nUsed))
					//{
					//	//loop buffer not enough, һ��û��ȡ�ߣ�error
					//	//assert(0);
					//	return;
					//}
					//
					//char buffer[sizeof(CMsgHead)+MAX_MSG_LEN];
					//if (!pContext->m_loopBuffer.PopFront( buffer, sizeof(CMsgHead)))
					//	return;// not enough a packet header

					//CMsgHead* tMsg = (CMsgHead*)buffer;
					//while (tMsg)
					//{
					//	if (tMsg->check!=0x1234)
					//	{
					//		ReleaseIOBuffer(pBuffer);
					//		ReleaseContext(pContext);
					//		return;
					//	}					
					//
					//	if (tMsg->len > MAX_MSG_LEN)
					//	{
					//		ReleaseIOBuffer(pBuffer);
					//		ReleaseContext(pContext);
					//		return;
					//	}
					//							
					//	if ( !pContext->m_loopBuffer.PopFront(buffer+sizeof(CMsgHead), tMsg->len))
					//	{
					//		msgErr = true;
					//		break; //not a full packet, exit loop
					//	}
					//	
					//	//post msg
					//	OnHandleMsg(pContext,(BYTE*)tMsg,tMsg->len + sizeof(CMsgHead));											
					//	
					//	//next msg
					//	if( !pContext->m_loopBuffer.PopFront( buffer, sizeof(CMsgHead) ) )
					//	{
					//		msgErr = true;
					//		break;
					//	}
					//}
				}

				pContext->m_iRecvSequenceCurrent = (pContext->m_iRecvSequenceCurrent+1)%5001;

				ReleaseIOBuffer(pBuffer);
				pBuffer = NULL;
			}

			if (!pBuffer)
			{
				pBuffer = AllocateBuffer(itReadZero);						
				if (!pBuffer)
				{
					ReleaseContext(pContext);
					ReleaseIOBuffer(pBuffer);
					return;
				}
			}			
			else
			{
				pBuffer->m_ioType = itReadZero;				
			}
			memset(&pBuffer->m_overlapped,0,sizeof(OVERLAPPED));

			if (!PostQueuedCompletionStatus(m_hIOCP,0,(DWORD)pContext,&pBuffer->m_overlapped))
			{
				int errCode = WSAGetLastError();
				if ( errCode!= ERROR_IO_PENDING)
				{
					printf("getlasterr code=%d\n",errCode);
					ReleaseContext(pContext);
					ReleaseIOBuffer(pBuffer);
					return;
				}
			}
		}

		void CIOCPSvr::OnWrite(CClientContext*pContext,DWORD dwSize,CIOBuffer*pBuffer)
		{
			if (!pBuffer)
			{
				ReleaseIOBuffer(pBuffer);
				ReleaseContext(pContext);
				return;
			}

			if (pBuffer)
			{
				pBuffer->m_ioType = itWriteComplete;
				memset(&pBuffer->m_overlapped,0,sizeof(OVERLAPPED));
				pBuffer->SetupWrite();

				DWORD dwFlags = 0;
				DWORD dwIOSize = 0;
				if (SOCKET_ERROR == WSASend(pContext->m_sock,&pBuffer->m_wsaBuf,1,&dwIOSize,dwFlags,
					&pBuffer->m_overlapped,
					NULL))
				{
					if (WSAGetLastError() != WSA_IO_PENDING)
					{
						ReleaseIOBuffer(pBuffer);
						pBuffer = NULL;
						pContext->m_iSendSequenceCurrent = (pContext->m_iSendSequenceCurrent+1)%5001;
						ReleaseContext(pContext);
					}					
				}				
			}
		}

		void CIOCPSvr::OnWriteComplete(CClientContext*pContext,DWORD dwSize,CIOBuffer*pBuffer)
		{
			if (!pBuffer || dwSize == 0)
			{
				return;
			}

			if (pBuffer->m_nUsed != dwSize)
			{
				if (dwSize < pBuffer->m_nUsed && dwSize > 0)
				{
					//not send finish
					if (pBuffer->Flush(dwSize))
					{
						//û����ɰɣ�����ɾbuffer
						pBuffer->m_ioType = itWrite;
						memset(&pBuffer->m_overlapped,0,sizeof(OVERLAPPED));						
						if (!PostQueuedCompletionStatus(m_hIOCP,pBuffer->m_nUsed,(DWORD)pContext,&pBuffer->m_overlapped))
						{
							if (WSAGetLastError() != WSA_IO_PENDING)
							{
								ReleaseContext(pContext);
								ReleaseIOBuffer(pBuffer);
								return;
							}							
						}
					}
				}
			}
			else // + [9/26/2019 Administrator]
			{
				//send finish
				ReleaseIOBuffer(pBuffer);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		//virtual Function

		void CIOCPSvr::OnClientClose(LPVOID pAddr)
		{
			//printf("virtual OnClientClose\n");
		}

		void CIOCPSvr::OnClientConnect(LPVOID pAddr)
		{
			//printf("virtual OnClientConnect\n");
		}

		bool CIOCPSvr::SendMsg( LPVOID pAddr,BYTE *data,int dataLen )
		{
			CClientContext *pClien=(CClientContext *)pAddr;
			CIOBuffer  *pBuffer = AllocateBuffer(itWrite);
			if (!pBuffer)
			{
				ReleaseIOBuffer(pBuffer);
				ReleaseContext(pClien);
				
				return false;
			}
			pBuffer->m_ioType=itWrite;
			pBuffer->m_nUsed=dataLen;			
			memcpy(pBuffer->m_data,data,dataLen);
			memset(&pBuffer->m_overlapped,0,sizeof(OVERLAPPED));
			if (!PostQueuedCompletionStatus(m_hIOCP,pBuffer->m_nUsed,(DWORD)pClien,&pBuffer->m_overlapped))
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					ReleaseContext(pClien);
					ReleaseIOBuffer(pBuffer);
					return false;
				}
			}
			return true;
		}

		void CIOCPSvr::OnHandleMsg(LPVOID pAddr,BYTE *data,int dataLen)
		{
			printf("virtual recv msg %d\n",dataLen);
			//should  push to  recv queue
		}

		void	  uZip(CClientContext* pContext, BYTE* data, int dataLen)
		{

		}
	}
}


