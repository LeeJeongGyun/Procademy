#pragma once
#include<Windows.h>
#include "MemoryPoolTLS.h"

class Packet;

class PacketOutputException : public std::exception
{
public:
	PacketOutputException(int iSize, char* buffer, int iBufferSize, int iLine)
		:_iSize(iSize), _iLine(iLine), _bufferSize(iBufferSize)
	{
		_buffer = new char[iBufferSize + 1];
		memcpy(_buffer, buffer, iBufferSize);
		_buffer[iBufferSize] = '\0';
	}

	~PacketOutputException()
	{
		delete _buffer;
	}
public:
	int _iSize;
	int _iLine;
	int _bufferSize;
	char* _buffer;
};


class Packet
{
	friend class MemoryPoolTLS<Packet>;
	friend class Chunk<Packet>;
	friend class MemoryPool<Packet>;
	enum ePACKET
	{
		eBUFFER_DEFAULT = 0x1000
	};

	enum en_LOG
	{
		enUCHAR = 1,
		enCHAR = 2,
		enUSHORT = 3,
		enSHORT = 4,
		enUINT = 5,
		enINT = 6,
		enINT64 = 7,
		enUINT64 = 8,
		enLONG = 9,
		enULONG = 10,
		enFLOAT = 11,
		enDOUBLE = 12,
		enDEFAULT = 30
	};

private:
	Packet();
	Packet(int iBufferSize);

public:
	~Packet();

	inline void Init() {}
	inline void PlacementNewInit() {}

	//////////////////////////////////////////////////////////////////////////
	// ������ ������ ��ȣȭ
	//
	// Parameters: ����.
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void Encode();

	//////////////////////////////////////////////////////////////////////////
	// ������ ������ ��ȣȭ
	//
	// Parameters: ����.
	// Return: (bool) ��ȣȭ ����
	//////////////////////////////////////////////////////////////////////////
	bool Decode();

	//////////////////////////////////////////////////////////////////////////
	// ��Ŷ  �ı�.
	//
	// Parameters: ����.
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void	Release(void);

	//////////////////////////////////////////////////////////////////////////
	// ��Ŷ û��.
	//
	// Parameters: ����.
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	inline void	Clear(void);

	//////////////////////////////////////////////////////////////////////////
	// ���� ������ ���.
	//
	// Parameters: ����.
	// Return: (int)��Ŷ ���� ������ ���.
	//////////////////////////////////////////////////////////////////////////
	int		GetBufferSize(void) { return _iBufferSize; }

	//////////////////////////////////////////////////////////////////////////
	// ���� ������� ������ ���.
	//
	// Parameters: ����.
	// Return: (int)������� ����Ÿ ������.
	//////////////////////////////////////////////////////////////////////////
	inline int	GetUseSize(void) { return _iUseSize; }

	//////////////////////////////////////////////////////////////////////////
	// ���� ������ ���.
	//
	// Parameters: ����.
	// Return: (char *)���� ������.
	//////////////////////////////////////////////////////////////////////////
	inline char* GetBufferPtr(void) { return _buffer; }


	//////////////////////////////////////////////////////////////////////////
	// ���� Pos �̵�. (�����̵��� �ȵ�)
	// GetBufferPtr �Լ��� �̿��Ͽ� �ܺο��� ������ ���� ������ ������ ��� ���. 
	//
	// Parameters: (int) �̵� ������.
	// Return: (int) �̵��� ������.
	//////////////////////////////////////////////////////////////////////////
	int		MoveWritePos(int iSize);
	int		MoveReadPos(int iSize);

	//////////////////////////////////////////////////////////////////////////
	// ����Ÿ ���.
	//
	// Parameters: (char *)Dest ������. (int)Size.
	// Return: (int)������ ������.
	//////////////////////////////////////////////////////////////////////////
	int GetData(char* chpData, int iSize);

	//////////////////////////////////////////////////////////////////////////
	// ����Ÿ ����.
	//
	// Parameters: (char *)Src ������. (int)SrcSize.
	// Return: (int)������ ������.
	//////////////////////////////////////////////////////////////////////////
	int PutData(char* chpData, int iSize);

	//////////////////////////////////////////////////////////////////////////
	// ��Ŷ ��� ������ ����
	//
	// Parameters: ����.
	// Return: (int) ��� ũ��
	//////////////////////////////////////////////////////////////////////////
	inline void ReserveHeadSize(int iSize) { _iUseSize += iSize; _iRear += iSize; }

	//////////////////////////////////////////////////////////////////////////
	// ����� ��� ������ ������ ����
	//
	// Parameters: (char*)��� ������, (int) ��� ũ��
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	inline void InputHeadData(char* pHeadData, int iSize) { memcpy(&_buffer[0], pHeadData, iSize); }

	//////////////////////////////////////////////////////////////////////////
	// �α� �����.
	//
	// Parameters: ����
	// Return: (char*) ���� ��� (int)���� Line, (BYTE) �ڷ��� ���� Ÿ��
	//////////////////////////////////////////////////////////////////////////
	void	MakeLogFile(const WCHAR* path, int iLine, BYTE bType);

	//////////////////////////////////////////////////////////////////////////
	// MemoryPool���� ����ȭ���� ������ �Ҵ�
	//
	// Parameters: ����
	// Return: (Packet*) ����ȭ ���� ������
	//////////////////////////////////////////////////////////////////////////
	static Packet* Alloc();

	//////////////////////////////////////////////////////////////////////////
	// ����ȭ���� MemoryPool�� ��ȯ
	//
	// Parameters: (Packet*) ����ȭ ���� ��ƾ��
	// Return: ����
	//////////////////////////////////////////////////////////////////////////
	static void Free(Packet* pPacket);

	//////////////////////////////////////////////////////////////////////////
	// ����ī��Ʈ 1 ����
	//
	// Parameters: ����.
	// Return: ����
	//////////////////////////////////////////////////////////////////////////
	inline void AddRef() { InterlockedIncrement(_refCount); }

	//////////////////////////////////////////////////////////////////////////
	// ����ī��Ʈ 1 ����
	//
	// Parameters: ����
	// Return: ����ī��Ʈ 1 ���� ��
	//////////////////////////////////////////////////////////////////////////
	inline long SubRef() { return InterlockedDecrement(_refCount); }
	
	Packet& operator=(const Packet& ref) = delete;

	Packet& operator<<(unsigned char data);
	Packet& operator<<(char data);
	Packet& operator<<(unsigned short data);
	Packet& operator<<(short data);
	Packet& operator<<(unsigned int data);
	Packet& operator<<(int data);
	Packet& operator<<(unsigned long data);
	Packet& operator<<(long data);
	Packet& operator<<(unsigned __int64 data);
	Packet& operator<<(__int64 data);
	Packet& operator<<(float data);
	Packet& operator<<(double data);


	Packet& operator>>(unsigned char& data)		throw(PacketOutputException);
	Packet& operator>>(char& data)				throw(PacketOutputException);
	Packet& operator>>(unsigned short& data)	throw(PacketOutputException);
	Packet& operator>>(short& data)				throw(PacketOutputException);
	Packet& operator>>(unsigned int& data)		throw(PacketOutputException);
	Packet& operator>>(int& data)				throw(PacketOutputException);
	Packet& operator>>(unsigned long& data)		throw(PacketOutputException);
	Packet& operator>>(long& data)				throw(PacketOutputException);
	Packet& operator>>(unsigned __int64& data)	throw(PacketOutputException);
	Packet& operator>>(__int64& data)			throw(PacketOutputException);
	Packet& operator>>(float& data)				throw(PacketOutputException);
	Packet& operator>>(double& data)			throw(PacketOutputException);

private:
	//////////////////////////////////////////////////////////////////////////
	// ���� ũ�� ����.
	//
	// Parameters: ����
	// Return: ����
	//////////////////////////////////////////////////////////////////////////
	void Resize(void);

	// TEMP PUBLIC
	// ORIGIN PRIVATE
public:
	char* _buffer = nullptr;
	
	int _iUseSize;
	int _iFront;
	int _iRear;
	int _iBufferSize;
	long* _refCount;

	alignas(64) bool _bOnEncodeFlag;

public:
	static MemoryPoolTLS<Packet> _packetPool;
	static uint8 _packetKey;
	static uint8 _packetCode;
};

inline void Packet::Clear(void)
{
	_iFront = 0;
	_iRear = 0;
	_iUseSize = 0;
}