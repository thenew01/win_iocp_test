#include "StdAfx.h"
#include ".\iobuffer.h"

CIOBuffer::CIOBuffer(void)
{
	memset(this,0,sizeof(CIOBuffer));
}

CIOBuffer::~CIOBuffer(void)
{
}

void CIOBuffer::SetupReadZero()
{
	m_wsaBuf.buf = (char*)m_data;
	m_wsaBuf.len = 0;
}

void CIOBuffer::SetupRead()
{
	if (m_nUsed==0)
	{
		m_wsaBuf.buf = (char*)m_data;
		m_wsaBuf.len = IO_BUFFER_LEN;
	}
	else
	{
		m_wsaBuf.buf = (char*)m_data + m_nUsed;
		m_wsaBuf.len = IO_BUFFER_LEN - m_nUsed;
	}
}

void CIOBuffer::SetupWrite()
{
	m_wsaBuf.buf = (char*)m_data;
	m_wsaBuf.len = m_nUsed;
}

bool	CIOBuffer::Flush(UINT nLen)
{
	if (nLen > IO_BUFFER_LEN || nLen > m_nUsed)
	{
		return false;
	}

	m_nUsed -= nLen;

	memmove(m_data,m_data+nLen,m_nUsed);

	return true;
}

long	CIOBuffer::GetRef()
{
	return m_ref;
}

void CIOBuffer::AddRef()
{
	::InterlockedIncrement(&m_ref);
}

bool CIOBuffer::ReleaseRef()
{
	if (m_ref == 0)
		return false;

	if (0 == ::InterlockedDecrement(&m_ref))
		return true;

	return false;
}
