#pragma once
#include "MsgQueue.h"

#include "iotype.h"

#define IO_BUFFER_LEN 1024// * 8

class  CIOBuffer
{
public:
	CIOBuffer(void);
	~CIOBuffer(void);
public:
	OVERLAPPED m_overlapped;
	BYTE		m_data[IO_BUFFER_LEN];
	IOType		m_ioType;
	UINT		m_nUsed;			// π”√
	UINT		m_iSequenceNumber;	//À≥–Ú
	WSABUF		m_wsaBuf;
private:
	//garbage collection reference
	long		m_ref;
public:
	void	SetupRead();
	void	SetupReadZero();
	void	SetupWrite();
public:
	bool	Flush(UINT nLen);
	long	GetRef();
	void	AddRef();
	bool	ReleaseRef();
};

typedef list<CIOBuffer*> IOBufferList;
typedef IOBufferList::iterator IOBufferListIt;