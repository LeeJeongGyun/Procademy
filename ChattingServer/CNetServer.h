#pragma once

#include "pch.h"

#pragma comment(lib, "ws2_32.lib")
using namespace std;

enum enTimeOut
{
	eBEFORE_LOGIN_TIMEOUT = 5000,
	eAFTER_LOGIN_TIMEOUT = 40000
};

class Packet;

#pragma pack(push, 1)
typedef struct stPacketHeader
{
	uint8 code;
	uint16 len;
	uint8 randKey;
	uint8 checkSum;
};
#pragma pack(pop)

enum eTYPE
{
	RECV = 0,
	SEND = 1
};

// SessionId ���� 2����Ʈ(���� �迭�� �ε���), ���� 6����Ʈ(������ Unique Id)
// ������ ��������ν� ������ ���ǹ迭�� �˻�
typedef union
{
	struct
	{
		uint64 id : 48;
		uint64 index : 16;
	}DUMMYSTRUCTNAME;

	uint64 sessionId;
}SessionId;

class CSLock
{
public:
	CSLock(CRITICAL_SECTION& lock)
		:_lock(lock)
	{
		EnterCriticalSection(&_lock);
	}
	~CSLock()
	{
		LeaveCriticalSection(&_lock);
	}
private:
	CRITICAL_SECTION& _lock;
};

class Lock
{
public:
	Lock(SRWLOCK& lock)
		:_lock(lock)
	{
		AcquireSRWLockExclusive(&_lock);
	}
	~Lock()
	{
		ReleaseSRWLockExclusive(&_lock);
	}
private:
	SRWLOCK& _lock;
};

class SharedLock
{
public:
	SharedLock(SRWLOCK& lock)
		:_lock(lock)
	{
		AcquireSRWLockShared(&_lock);
	}
	~SharedLock()
	{
		ReleaseSRWLockShared(&_lock);
	}
private:
	SRWLOCK& _lock;
};

class CNetServer
{
private:
	typedef struct stMyOverlapped
	{
		WSAOVERLAPPED overlapped;
		eTYPE type;
	};

	typedef struct stSession
	{
		stSession() :ioRefCount(0x01000000), bCancelFlag(false), maxTimeOutTick(50000), /*TEMP*/bSendDisconnectFlag(false)
		{
		}

		SOCKET					clntSock;
		WCHAR					szIP[dfIP_LEN];
		uint16					port;
		uint64					sessionId;
		
		// TimeOut �ð� ������
		uint16					maxTimeOutTick;
		
		// Content���� Disconnect�� �� �ľ� �뵵
		bool					bCancelFlag;

		// SendPacketAndDisconnect �ľ� �뵵
		bool					bSendDisconnectFlag;

		// �� �������� ���� load�� �Ͼ���� 
		// �Ʒ� �������� load, store�� ���� �Ͼ�� ������
		// ĳ�� �̽����� ���߱� ���� ioRefCount�� alignas�� �����

		// ���� 1����Ʈ ���� Disconnect Ȯ��
		// ���� 3����Ʈ ������ IO ���� ī��Ʈ
		alignas(64) uint32		ioRefCount;

#ifdef dfSMART_PACKET_PTR
		LockFreeQueue<PacketPtr> sendQ;
#else
		LockFreeQueue<Packet*>	sendQ;
#endif
		RingBuffer				recvQ;

		stMyOverlapped			recvOverlapped;
		stMyOverlapped			sendOverlapped;

		// SendQ���� ���� Packet*�� ����
		uint16					sendPacketCnt;
		uint64					timeOutTick;

#ifdef dfSMART_PACKET_PTR
		PacketPtr sendPacketPtrArr[200];
#else
		Packet* sendPacketArr[200];
#endif
		// ioRefCount�� ���� ĳ�ö��ο� �����Ѵٸ� alignas ����ؾߵȴ�.
		// Send ���� ���� �÷���
		bool		sendFlag;
	};

public:
	CNetServer();
	virtual ~CNetServer();

	//////////////////////////////////////////////////////////////////////////
	// Server ���� �Լ�
	//
	// Parameters: (WCHAR*) ���� IP (uint16) ���� Port (uint8) GQCS���� ����� ������ �� (uint8) Running�� ������ �� (int32) ���� �ִ� ����� ��, (bool) timeOut ���� (bool) Nagle ���� (bool) IOpending ����
	// Return: (bool) ���� ���� ����
	//////////////////////////////////////////////////////////////////////////
	bool Start(const WCHAR* szIP, uint16 port, uint8 numOfWorkerThread, uint8 numOfRunningThread, int32 iMaxSessionCount, bool onTimeOut, bool onNagle, bool bZeroCopy);

	//////////////////////////////////////////////////////////////////////////
	// ���� ���� �Լ�
	//
	// Parameters: ����.
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void Stop();

	//////////////////////////////////////////////////////////////////////////
	// ���� ����� ���� ��
	//
	// Parameters: ����.
	// Return: (int32) ���� ��
	//////////////////////////////////////////////////////////////////////////
	int32 GetSessionCount() { return _sessionCount; }

	//////////////////////////////////////////////////////////////////////////
	// ���������� �ش� ���� ���� �� ����ϴ� �Լ�
	//
	// Parameters: (uint64) ���� ���� ���̵�
	// Return: (bool) ���� ����
	//////////////////////////////////////////////////////////////////////////
	bool Disconnect(uint64 sessionId);


#ifdef dfSMART_PACKET_PTR
	//////////////////////////////////////////////////////////////////////////
	// ���������� ������ ���� ������ ���� �� ȣ���ϴ� �Լ�
	//
	// Parameters: (uint64) �ش� ���� ���̵� (PacketPtr) ����ȭ���� ����Ʈ ������
	// Return: (bool) ���� ����
	//////////////////////////////////////////////////////////////////////////
	bool SendPacket(uint64 sessionId, PacketPtr packetPtr);
#else
	//////////////////////////////////////////////////////////////////////////
	// ���������� ������ ���� ������ ���� �� ȣ���ϴ� �Լ�
	//
	// Parameters: (uint64) �ش� ���� ���̵� (Packet*) ����ȭ���� ������
	// Return: (bool) ���� ����
	//////////////////////////////////////////////////////////////////////////
	bool SendPacket(uint64 sessionId, Packet* pPacket);
#endif

	// TEMP
	bool SendPacketPostQueue(uint64 sessionId, Packet* pPacket);

	// TEMP
	bool SendPacketNotPostQueue(uint64 sessionId, Packet* pPacket);

	//////////////////////////////////////////////////////////////////////////
	// ���������� ������ ������ �ش� ���� ���� �� ȣ���ϴ� �Լ�
	//
	// Parameters: (uint64) �ش� ���� ���̵� (Packet*) ����ȭ���� ������
	// Return: (bool) ���� ����
	//////////////////////////////////////////////////////////////////////////
	bool SendPacketAndDisconnect(uint64 sessionId, Packet* pPacket);

	//////////////////////////////////////////////////////////////////////////
	// ��Ʈ��ũ ���̺귯�� ���ο��� ����� Ip�� Port�� �������� ������
	// ���� �¶����θ� Ȯ�ι޴� �Լ�
	//
	// Parameters: (WCHAR*) ����� IP (uint16) ����� ��Ʈ 
	// Return: (bool) ���� �¶����� ��
	//////////////////////////////////////////////////////////////////////////
	virtual bool OnConnectionRequest(WCHAR* szIp, uint16 port) abstract;

	//////////////////////////////////////////////////////////////////////////
	// ��Ʈ��ũ ���̺귯�� ���ο��� ����� ������ �������� �˷��ִ� �Լ�
	//
	// Parameters: (uint64) ����� ���� ���̵� 
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnClientJoin(uint64 sessionId) abstract;

	//////////////////////////////////////////////////////////////////////////
	// ��Ʈ��ũ ���̺귯�� ���ο��� ���� ������ �������� �˷��ִ� �Լ�
	//
	// Parameters: (uint64) ���� ���� ���̵� 
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnClientLeave(uint64 sessionId) abstract;

#ifdef dfSMART_PACKET_PTR
	//////////////////////////////////////////////////////////////////////////
	// ��Ʈ��ũ ���̺귯�� ���ο��� ���� �����͸� �������� �ִ� �Լ� 
	//
	// Parameters: (uint64) �����͸� ���� ���� ���̵� (PacketPtr) ����ȭ ���� ������
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnRecv(uint64 sessionId, PacketPtr packetPtr) abstract;
#else
	//////////////////////////////////////////////////////////////////////////
	// ��Ʈ��ũ ���̺귯�� ���ο��� ���� �����͸� �������� �ִ� �Լ� 
	//
	// Parameters: (uint64) �����͸� ���� ���� ���̵� (Packet*) ����ȭ ���� ������
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnRecv(uint64 sessionId, Packet* pPacket) abstract;
#endif

	//////////////////////////////////////////////////////////////////////////
	// ������ �ð��� �ʰ��� ������ �������� �˷��ִ� �Լ� 
	//
	// Parameters: (uint64) ������ �ð��� �ʰ��� ������ id
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnTimeOut(uint64 sessionId) abstract;

	//////////////////////////////////////////////////////////////////////////
	// ��Ʈ��ũ ���̺귯�� ���ο��� ���� �������� �������� �ִ� �Լ�
	//
	// Parameters: (int32) �ش� ���� �ڵ�
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnError(int32 iErrCode) abstract;

	//////////////////////////////////////////////////////////////////////////
	// Server IP ��ȯ �Լ�
	//
	// Parameters: ����.
	// Return: (WCHAR*) Server IP
	//////////////////////////////////////////////////////////////////////////
	WCHAR* GetServerIP() { return _szIP; }

	//////////////////////////////////////////////////////////////////////////
	// Server Port ��ȯ �Լ�
	//
	// Parameters: ����.
	// Return: (uint16) Server Port
	//////////////////////////////////////////////////////////////////////////
	uint16 GetServerPort() { return _port; }

	//////////////////////////////////////////////////////////////////////////
	// Listen Socket ��ȯ �Լ�
	//
	// Parameters: ����.
	// Return: (uint8) ������ ����
	//////////////////////////////////////////////////////////////////////////
	SOCKET GetListenSocket() { return _listenSock; }

	//////////////////////////////////////////////////////////////////////////
	// Thread ���� ��ȯ �Լ�
	// ��Ŀ ������ ���� �����ϰ� main Thread ���Ḧ ���� ������ ������ �����ϴ� �Լ�
	//
	// Parameters: ����.
	// Return: (uint8) ������ ����
	//////////////////////////////////////////////////////////////////////////
	uint8 GetThreadNum() { return _threads.size(); }

	//////////////////////////////////////////////////////////////////////////
	// Thread Handle ���� �Լ�
	// ��Ŀ ������ ���� �����ϰ� main Thread ���Ḧ ���� �Լ�
	//
	// Parameters: ����.
	// Return: (HANDLE*) Thread �ڵ� ������
	//////////////////////////////////////////////////////////////////////////
	HANDLE* GetThreadHandle() { return _threads.data(); }

	//////////////////////////////////////////////////////////////////////////
	// Iocp Handle ��ȯ �Լ�
	//
	// Parameters: ����.
	// Return: (HANDLE) IOCP �ڵ�
	//////////////////////////////////////////////////////////////////////////
	HANDLE GetIocpHandle() { return _iocpHandle; }

	//////////////////////////////////////////////////////////////////////////
	// Accept Thread TPS ��ȯ �Լ�
	//
	// Parameters: ����.
	// Return: (ulong) Accept Thread TPS ��
	//////////////////////////////////////////////////////////////////////////
	inline ulong GetAcceptTPS() { return _lastAcceptTPS; }

	//////////////////////////////////////////////////////////////////////////
	// Send Message TPS ��ȯ �Լ�
	//
	// Parameters: ����.
	// Return: (ulong) Send Message TPS ��
	//////////////////////////////////////////////////////////////////////////
	inline ulong GetSendTPS() { return _lastSendTPS; }

	//////////////////////////////////////////////////////////////////////////
	// Recv Message TPS ��ȯ �Լ�
	//
	// Parameters: ����.
	// Return: (ulong) Recv Message TPS ��
	//////////////////////////////////////////////////////////////////////////
	inline ulong GetRecvTPS() { return _lastRecvTPS; }

	//////////////////////////////////////////////////////////////////////////
	// �ʴ� Send �۽ŷ�
	//
	// Parameters: ����.
	// Return: (ulong) Recv Message TPS ��
	//////////////////////////////////////////////////////////////////////////
	inline ulong GetSendBytes() { return _lastSendBytes; }

	//////////////////////////////////////////////////////////////////////////
	// Accept Total Count �� Ȯ��
	//
	// Parameters: ����.
	// Return: �� Accept ��.
	//////////////////////////////////////////////////////////////////////////
	inline uint64 GetAcceptTotalCnt() { return _acceptTotalCnt; }

	//////////////////////////////////////////////////////////////////////////
	// Content���� Session Timeout �� ���� �Լ�
	//
	// Parameters: (uint64) ���� id, (uint32) timeOut ��
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void SetTimeOut(uint64 sessionId, uint32 timeOutValue);

	//////////////////////////////////////////////////////////////////////////
	// Content���� �ش� �������� �ִ� ��Ŷ ������ ����
	//
	// Parameters: (int32) packet�� �ִ� ũ��
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void SetPacketMaxSize(int32 packetMaxSize) { _packetMaxSize = packetMaxSize; }


private:
	static uint32 WINAPI StaticTimeoutThread(LPVOID pServer)
	{
		((CNetServer*)pServer)->TimeoutThread();
		return 0;
	}
	
	static uint32 WINAPI StaticTPSThread(LPVOID pServer)
	{
		((CNetServer*)pServer)->TPSThread();
		return 0;
	}
	
	static uint32 WINAPI StaticAcceptThread(LPVOID pServer)
	{
		((CNetServer*)pServer)->AcceptThread();
		return 0;
	}

	static uint32 WINAPI StaticWorkerThread(LPVOID pServer)
	{
		((CNetServer*)pServer)->WorkerThread();
		return 0;
	}
	
	//������ ���� User ����� ���� Timeout üũ ������
	void TimeoutThread();

	// Send Message, Recv Message TPS ó���ϴ� ������
	void TPSThread();

	// Accept Thread	
	void AcceptThread();

	// Worker Thread
	void WorkerThread();

	//////////////////////////////////////////////////////////////////////////
	// ���� API ����ϱ� ���� �ʱ�ȭ �Լ�
	//
	// Parameters: ����.
	// Return: (bool) ���� API �ʱ�ȭ ��������
	//////////////////////////////////////////////////////////////////////////
	bool NetworkInit();

	//////////////////////////////////////////////////////////////////////////
	// WSARecv ���� �Լ�
	//
	// Parameters: (stSession*) ���� ����� �Ǵ� ���� ������
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void RecvPost(stSession* pSession);

	//////////////////////////////////////////////////////////////////////////
	// WSASend ���� �Լ�
	//
	// Parameters: (stSession*) �۽� ����� �Ǵ� ���� ������
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void SendPost(stSession* pSession);

	//////////////////////////////////////////////////////////////////////////
	// Session Release �Լ�
	//
	// Parameters: (stSession*) ������� ���� ������
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void DisconnectSession(stSession* pSession);

	//////////////////////////////////////////////////////////////////////////
	// Session �ʱ�ȭ �Լ�
	//
	// Parameters: (uint16) ����ִ� ���ǹ迭�� �ε��� (SOCKET) socket (uint64) ���� ���̵� (WCHAR*) Ip (uint16) Port
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void SetSession(uint16 index, SOCKET sock, uint64 sessionId, WCHAR* ip, uint16 port);

	//////////////////////////////////////////////////////////////////////////
	// Error ó�� �Լ�
	//
	// Parameters: (int32) Error Code
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void HandleError(int32 errCode);

	//////////////////////////////////////////////////////////////////////////
	// ������ �����ϰ� ȹ���ߴ��� Ȯ���ϴ� �Լ�
	//
	// Parameters: ����.
	// Return: (bool) ���� ȹ�� ���� ����
	//////////////////////////////////////////////////////////////////////////
	bool AcquireSession(uint64 sessionId, stSession* pSession);

	//////////////////////////////////////////////////////////////////////////
	// ������ ������ �� �� �ִ��� Ȯ���ϴ� �Լ�
	//
	// Parameters: ����
	// Return: (bool) ���� ������ ���� ����
	//////////////////////////////////////////////////////////////////////////
	inline bool ReleaseSession(stSession* pSession);

	//////////////////////////////////////////////////////////////////////////
	// ioRefCount�κ��� releaseFlag ��� �Լ�
	//
	// Parameters: (uint32) ioRefCount
	// Return: (bool) ������ �÷��� ��
	//////////////////////////////////////////////////////////////////////////
	inline bool GetReleaseFlag(uint32& ioRefCount) { return ioRefCount >> 24; }

private:
	WCHAR								_szIP[32];
	uint16								_port;
	SOCKET								_listenSock;
	HANDLE								_iocpHandle;
	bool								_bOnNagle;
	bool								_bOnZeroCopy;
	int32								_packetMaxSize;

	// SendPacketThread�� �̺�Ʈ
	stSession*							_sessionArr;

	// �ִ� ������ ��
	uint32								_maxSessionCount;

	// �� �������� load�� �Ͼ���� 
	// �Ʒ� �������� load, store�� ���� �Ͼ�� ������
	// ĳ�� �̽����� ���߱� ���� _uniqueSesisonId�� alignas�� �����

	alignas(64) uint64					_uniqueSessionId;
	uint32								_sessionCount;
	vector<HANDLE>						_threads;

	// ����ִ� ���ǹ迭�� �ε��� ������ ����
	LockFreeStack<uint16>				_indexStack;

	uint64								_acceptTotalCnt = 0;
	ulong								_lastAcceptTPS = 0;
	ulong								_lastSendTPS = 0;
	ulong								_lastRecvTPS = 0;
	ulong								_lastSendBytes = 0;

	ulong								_currentAcceptTPS = 0;
	ulong								_currentSendTPS = 0;
	alignas(64) ulong					_currentRecvTPS = 0;
	alignas(64) ulong					_currentSendBytes = 0;
};

inline bool CNetServer::ReleaseSession(stSession* pSession)
{
	// NOTE
	// ioRefCount�� ���� 1Byte�� ���� DisconnectSession�� ����ƴ��� �Ǵ�
	if (InterlockedCompareExchange(&pSession->ioRefCount, 0x01000000, 0) != 0)
		return false;

	return true;
}