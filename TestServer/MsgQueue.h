#pragma once
#include <list>
using namespace std;

#include "winsock2.h"


#define MAX_MSG_LEN	(1024 * 8)

#pragma pack(1)

namespace Net
{
	struct CMsgHead
	{
	public:
		CMsgHead()
		{
			check=0x1234;
		}
		int	check;
		int id;
		int len;		//only body len
	};
	
	class CMsg
	{
	public:
		BYTE data[MAX_MSG_LEN];
	public:
		CMsg()
		{
			memset(this,0,sizeof(CMsg));
		}
		CMsgHead GetMsgHead()
		{
			CMsgHead head;
			memcpy(&head,data,sizeof(CMsgHead));
			return head;
		}
	protected:
	private:
	};


	template<typename T>
	class CMsgT
	{
	public:
		CMsgHead head;
		T msg;
	public:
		CMsg GetMsg()
		{
			CMsg tmp;
			memcpy(tmp.data,&head,sizeof(CMsgHead));
			memcpy(tmp.data+sizeof(CMsgHead),&msg,sizeof(T));
			return tmp;
		}
	protected:
	private:
	};

	struct null_type
	{

	};

	template<>
	class CMsgT<null_type>
	{
	public:
		CMsgHead head;
	public:
		CMsg GetMsg()
		{
			CMsg tmp;
			memcpy(&tmp,&head,sizeof(CMsgHead));
			return tmp;
		}
	};

	typedef list<CMsg> MsgList;

	class  CMsgQueue
	{
	public:
		CMsgQueue(void);
		virtual~CMsgQueue(void);
	public:
		CMsg PopMsg();
		void PushMsg(CMsg msg);
		void Destory();
	private:
		MsgList m_msgList;
		HANDLE m_sema;
	};

}

#pragma pack()