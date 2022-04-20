#pragma once

#include<Windows.h>
#include<iostream>
#include "MemoryPool.h"
#include "Chunk.h"

template<typename DATA>
class MemoryPoolTLS
{
public:
	//////////////////////////////////////////////////////////////////////////
	// ������, �Ҹ���
	//
	// Parameters:	(bool) Alloc �� ������ / Free �� �Ҹ��� ȣ�� ����
	//////////////////////////////////////////////////////////////////////////
	MemoryPoolTLS(bool bPlacementFlag = false);
	~MemoryPoolTLS();

	//////////////////////////////////////////////////////////////////////////
	// Chunk ���� ������ �Ҵ�
	//
	// Parameters: ����.
	// Return: (DATA *) ������ �� ������.
	//////////////////////////////////////////////////////////////////////////
	DATA*			Alloc();

	//////////////////////////////////////////////////////////////////////////
	// ������̴� Chunk ���� ������ ����
	// Chunk�� �ִ� ��� ������ ���� �����ƴٸ� MemoryPool�� Free
	//
	// Parameters: (DATA *) ������ �� ������.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	bool			Free(DATA* pData);

	//////////////////////////////////////////////////////////////////////////
	// ���� Ȯ�� �� Chunk ������ ��´�. (�޸�Ǯ ������ ��ü ����)
	//
	// Parameters: ����.
	// Return: (LONG64) �޸� Ǯ ���� �Ҵ����� Chunk ����
	//////////////////////////////////////////////////////////////////////////
	long			GetUseCount() { return _useCount; }

private:
	// Chunk�� �����ϴ� �޸� Ǯ
	MemoryPool<Chunk<DATA>>		_memoryPool;

	// Chunk ��� ���� 
	long						_useCount;

	// TLS �ε��� ���� 
	// _tlsIndex�� _placementFlag�� �ѹ� ������ �����ϰ� load�� ������ ĳ�ö��� ��ŭ ����߷�
	// ĳ�� �̽��� Ȯ���� �ٿ��ش�.
	alignas(64) DWORD			_tlsIndex;

	// PlacementNew ��� �÷���
	bool						_bPlacementFlag;
};

template<typename DATA>
inline MemoryPoolTLS<DATA>::MemoryPoolTLS(bool bPlacementFlag)
	: _useCount(0), _memoryPool(true, bPlacementFlag), _bPlacementFlag(bPlacementFlag)
{
	_tlsIndex = TlsAlloc();
	if (_tlsIndex == TLS_OUT_OF_INDEXES)
	{
		// TODO
		// CRASH & LOG
	}
}

template<typename DATA>
inline MemoryPoolTLS<DATA>::~MemoryPoolTLS()
{

}

template<typename DATA>
inline DATA* MemoryPoolTLS<DATA>::Alloc()
{
	Chunk<DATA>* pChunk;

	// ����� �ڵ�
	DWORD tlsIndex = _tlsIndex;
	long allocCount;

	if (TlsGetValue(tlsIndex) == 0)
	{
		pChunk = _memoryPool.Alloc();
		TlsSetValue(tlsIndex, pChunk);

		_InterlockedIncrement(&_useCount);

		// ����� �ڵ�
		allocCount = pChunk->_allocCount++;

		if (allocCount == dfCHUNK_SIZE - 1)
		{
			TlsSetValue(tlsIndex, 0);
		}
	}
	else
	{
		pChunk = (Chunk<DATA>*)TlsGetValue(tlsIndex);

		// ����� �ڵ�
		allocCount = pChunk->_allocCount++;

		if (allocCount == dfCHUNK_SIZE - 1)
		{
			TlsSetValue(tlsIndex, 0);
		}
	}

	if (_bPlacementFlag)
	{
		new (&pChunk->node[allocCount].data) DATA;
	}

	return &pChunk->node[allocCount].data;
}

template<typename DATA>
inline bool MemoryPoolTLS<DATA>::Free(DATA* pData)
{
	Chunk<DATA>* pChunk = ((typename Chunk<DATA>::ChunkNode*)pData)->pChunk;

#ifndef _PERFORMANCE
	if (pChunk->guard != ((typename Chunk<DATA>::ChunkNode*)pData)->guard)
	{
		// CRASH
		// LOG
		wprintf(L"Guard Fail\n");
		return false;
	}
#endif

	if (_bPlacementFlag)
	{
		pData->~DATA();
	}

	if (dfCHUNK_SIZE == InterlockedIncrement(&pChunk->_freeCount))
	{
		_memoryPool.Free(pChunk);
		_InterlockedDecrement(&_useCount);
	}

	return true;
}