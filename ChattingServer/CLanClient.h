#pragma once

#include "pch.h"
#include<Windows.h>

#define dfIP_LEN	32

typedef struct stPacketHeaderLan
{
	short len;
};

enum enTYPE
{
	eRECV = 0,
	eSEND = 1
};

class CLanClient
{
private:

	typedef struct stMyOverlapped
	{
		WSAOVERLAPPED overlapped;
		enTYPE type;
	};

	typedef struct stSession
	{
		stSession() :ioRefCount(0), bCancelFlag(false)
		{
		}

		SOCKET					clntSock;
		WCHAR					szIP[dfIP_LEN];
		WORD					port;

		// Content���� Disconnect�� �� �ľ� �뵵
		bool					bCancelFlag;

		// �� �������� ���� load�� �Ͼ���� 
		// �Ʒ� �������� load, store�� ���� �Ͼ�� ������
		// ĳ�� �̽����� ���߱� ���� ioRefCount�� alignas�� �����

		// ���� 1����Ʈ ���� Disconnect Ȯ��
		// ���� 3����Ʈ ������ IO ���� ī��Ʈ
		unsigned int			ioRefCount;

		RingBuffer				sendQ;
		RingBuffer				recvQ;

		stMyOverlapped			recvOverlapped;
		stMyOverlapped			sendOverlapped;

		// ioRefCount�� ���� ĳ�ö��ο� �����Ѵٸ� alignas ����ؾߵȴ�.
		// Send ���� ���� �÷���
		alignas(64) bool		sendFlag;
	};

public:
	CLanClient();
	virtual ~CLanClient();

	bool Connect(WCHAR* szBindIP, WCHAR* szServerIP, unsigned short port, int numOfWorkerThread, bool bNagleOn);
	bool Disconnect();
	bool SendPacket(Packet* pPacket);

	virtual void OnEnterJoinServer() abstract;
	virtual void OnLeaveServer() abstract;

	virtual void OnRecv(Packet* pPacket) abstract;
	virtual void OnSend(int iSendSize) abstract;

	virtual void OnError(int iErrCode, WCHAR* szStr) abstract;
	virtual void OnSetLoginPacket(Packet* pPacket) abstract;

public:
	static unsigned int WINAPI StaticWorkerThread(LPVOID pServer)
	{
		((CLanClient*)pServer)->WorkerThread();
		return 0;
	}

	static unsigned int WINAPI StaticConnectThread(LPVOID lpParam)
	{
		((CLanClient*)lpParam)->ConnectThread();
		return 0;
	}

	void WorkerThread();

	void ConnectThread();

	bool NetworkInit(const WCHAR* szIP);

	//////////////////////////////////////////////////////////////////////////
	// Session �ʱ�ȭ �Լ�
	//
	// Parameters: (stSession*) ���� ������ (SOCKET) socket (WCHAR*) Ip (uint16) Port
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void SetSession(stSession* pSession, SOCKET sock, WCHAR* ip, WORD port);

	//////////////////////////////////////////////////////////////////////////
	// WSARecv ���� �Լ�
	//
	// Parameters: ����.
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void RecvPost();

	//////////////////////////////////////////////////////////////////////////
	// WSASend ���� �Լ�
	//
	// Parameters: ����.
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void SendPost();

	//////////////////////////////////////////////////////////////////////////
	// Session Release �Լ�
	//
	// Parameters: (stSession*) ������� ���� ������
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void DisconnectSession(stSession* pSession);

	//////////////////////////////////////////////////////////////////////////
	// Error ó�� �Լ�
	//
	// Parameters: (int32) Error Code
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void HandleError(int errCode);

	//////////////////////////////////////////////////////////////////////////
	// SendPacket ���� SendPost Flag �Լ�
	//
	// Parameters: ����.
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	inline void ActiveSendPacket() { _bSendFlag = true; }

private:
	SOCKET				_clntSock;
	HANDLE				_iocpHandle;
	
	WCHAR				_szIP[dfIP_LEN];
	unsigned short		_port;
	WCHAR				_szServerIP[dfIP_LEN];
	unsigned short		_serverPort;
	int32				_numOfWorkerThread;

	bool				_bNagle;
	bool				_bStart;
	bool				_bConnect;
	bool				_bLoginPacket;

	bool				_bSendFlag;

	std::vector<HANDLE>	_threads;
	stSession*			_pSession;
};

