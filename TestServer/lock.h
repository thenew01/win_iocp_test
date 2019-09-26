#pragma once


template<class T>
class autoLock{
public:
	autoLock(T* pLock):m_pLock(pLock){
		Lock();
	}

	~autoLock(){
		Unlock();
	}

	void Lock(){
		if(m_pLock != NULL)
			m_pLock->Lock();
	}

	void Unlock(){
		if(m_pLock != NULL)
			m_pLock->Unlock();
	}

private:
	typedef T*  LPLOCK;

	LPLOCK	m_pLock;
};

class CLock
{
private:
	CRITICAL_SECTION m_CritSect;
public:
	CLock() { InitializeCriticalSection(&m_CritSect); }
	~CLock() { DeleteCriticalSection(&m_CritSect); }

	void Lock() { EnterCriticalSection(&m_CritSect); }
	void Unlock() { LeaveCriticalSection(&m_CritSect); }
	LONG GetLockedThreadCount(){ return ( (- m_CritSect.LockCount -1 ) >> 2 ); }
};

typedef autoLock<CLock> AutoLock;