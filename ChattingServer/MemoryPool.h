#pragma once

#include<Windows.h>
#include "Macro.h"

template<typename DATA>
class MemoryPool
{
	struct Node;

	// NOTE
	// Alloc �Լ����� CountedNode�� ���������� �����ϰ� �ִ�.
	// DoubleCAS�� �ּҰ� ��������Ƿ� 16Byte ���� �ʼ�
	struct alignas(16) CountedNode
	{
		// NOTE 
		// Alloc �Լ����� CountedNode�� ���������� Ŭ������ ����� 
		// ����ü ������ �� ��� ������ �����ؾ��Ѵ�.
		// uniqueCount�� nodePtr ���� ���� ���Ե� ���
		// Free������ �ӵ��� ������ ���� DoubleCAS�� ������� �����Ƿ�
		// Alloc�ʿ��� Free�� ���� ���� �Ͼ�� ������ �߻��Ѵ�.
		LONG64 uniqueCount;
		Node* nodePtr;
	};

#ifdef _PERFORMANCE
	struct Node
	{
		inline void PlacementNewInit() {}
		inline void Init() {}
		DATA data;
		Node* next;
	};
#else 
	struct Node
	{
		inline void PlacementNewInit() {}
		inline void Init() {}
		DATA data;
		ULONG64 guard;
		Node* next;
	};
#endif

public:
	//////////////////////////////////////////////////////////////////////////
	// ������, �Ҹ���
	//
	// Parameters:	(bool) Alloc �� ������ / Free �� �Ҹ��� ȣ�� ����
	//////////////////////////////////////////////////////////////////////////
	MemoryPool(bool bPlacementNew = false, bool bTLSPlacementNew = false);
	~MemoryPool();

	//////////////////////////////////////////////////////////////////////////
	// �� �ϳ��� �Ҵ�޴´�.  
	//
	// Parameters: ����.
	// Return: (DATA *) ����Ÿ �� ������.
	//////////////////////////////////////////////////////////////////////////
	DATA* Alloc();

	//////////////////////////////////////////////////////////////////////////
	// ������̴� ���� �����Ѵ�.
	//
	// Parameters: (DATA *) �� ������.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	bool			Free(DATA* ptr);

	//////////////////////////////////////////////////////////////////////////
	// ���� Ȯ�� �� �� ������ ��´�. (�޸�Ǯ ������ ��ü ����)
	//
	// Parameters: ����.
	// Return: (LONG64) �޸� Ǯ ���� ��ü ����
	//////////////////////////////////////////////////////////////////////////
	LONG64			GetAllocCount() { return _allocCount; }

	//////////////////////////////////////////////////////////////////////////
	// ���� ������� �� ������ ��´�.
	//
	// Parameters: ����.
	// Return: (int) ������� �� ����.
	//////////////////////////////////////////////////////////////////////////
	LONG64			GetUseCount() { return _allocCount - _nodeCount; }

private:
	// ���ο��� �����ϴ� ������ top�� ����Ű�� ������
	CountedNode* _top;

	// NOTE
	// _top, _allocCount, _nodeCount ���� Interlocked �Լ� ���
	// ���� ���� ĳ�ö��ο� �����ϰ� �ȴٸ� ���� ������ �������� �޸𸮿� �����ϱ� ����
	// �ϵ���� ������ ����ϴ� ������ �߻��� �� �ִ�.
	// ���� alignas�� ����Ͽ� ĳ�ö��θ�ŭ ���� ��� �����ߴ�.

	// �� �Ҵ����� ���� ����
	alignas(64) DWORD	_allocCount;

	// ���� ���ο��� ��� �ִ� ���� ����
	alignas(64) long	_nodeCount;

	// Placement New ��� �÷���
	alignas(64) bool	_bPlacementNew;

	// TLS���� MemoryPool�� ������� �� TLSMemoryPool�� Placement New ��� �÷���
	bool				_bTLSPlacementNew;

#ifndef _PERFORMANCE
	// �� ������Ʈ Ǯ���� Alloc ���� �޸𸮰� �´��� Ȯ��
	ULONG64				_guard;
#endif
};

template<typename DATA>
inline MemoryPool<DATA>::MemoryPool(bool bPlacementNew, bool bTLSPlacementNew)
	:_allocCount(0), _nodeCount(0), _bPlacementNew(bPlacementNew), _bTLSPlacementNew(bTLSPlacementNew)
{
	_top = (CountedNode*)_aligned_malloc(sizeof(CountedNode), 16);
	_top->nodePtr = nullptr;
	_top->uniqueCount = 0;

#ifndef _PERFORMANCE
	_guard = g_Guard;
	g_Guard++;
#endif
}

template<typename DATA>
inline MemoryPool<DATA>::~MemoryPool()
{
	Node* top = _top->nodePtr;
	_aligned_free(_top);

	while (top)
	{
		Node* dNode = top;
		top = dNode->next;
		if (_bPlacementNew)
		{
			free(dNode);
		}
		else
		{
			delete dNode;
		}
	}
}

template<typename DATA>
inline DATA* MemoryPool<DATA>::Alloc()
{
	CountedNode popTop;
	Node* retNode = nullptr;

	// NOTE
	// ���ʿ� ������ Interlocked �Լ��� ������� �������ν�
	// ���� �߻� ���ɼ� �ٿ��ش�.
	if (_nodeCount > 0)
	{
		if (_InterlockedDecrement(&_nodeCount) >= 0)
		{
			do
			{
				// NOTE
				// popTop = *_top;
				// Compile�� byte���� ����� ���´ٸ� ������ ���� ���ɼ��� �����Ѵ�.
				// ���� ����� ���Կ������� �ٲ�
				popTop.uniqueCount = _top->uniqueCount;
				popTop.nodePtr = _top->nodePtr;
			} while (false == _InterlockedCompareExchange128((LONG64*)_top, (LONG64)popTop.nodePtr->next, popTop.uniqueCount + 1, (LONG64*)&popTop));

			retNode = popTop.nodePtr;
		}
		else
		{
			_InterlockedIncrement(&_nodeCount);
		}
	}

	if (retNode == nullptr)
	{
		if (_bPlacementNew)
		{
			retNode = (Node*)malloc(sizeof(Node));
			new (&retNode->data) DATA;

			if (_bTLSPlacementNew)
			{
				retNode->data.PlacementNewInit();
			}
			else
			{
				retNode->data.Init();
			}
		}
		else
		{
			retNode = new Node;

			// TLSMemoryPool ��� �� ���� �޸�Ǯ�� ������ PlacementNew�� ����ϱ� ������
			// ���⿡ TLS ���� �б⹮�� �� �ʿ䰡 ����.
		}

		_InterlockedIncrement(&_allocCount);
	}
	else
	{
		if (_bPlacementNew)
		{
			new (&retNode->data) DATA;
		}
	}

#ifndef _PERFORMANCE
	retNode->guard = _guard;
#endif

	return (DATA*)retNode;
}

template<typename DATA>
inline bool MemoryPool<DATA>::Free(DATA* ptr)
{
	Node* newTop = (Node*)ptr;
	Node* nowTop;

	if (_bPlacementNew)
	{
		ptr->~DATA();
	}

#ifndef _PERFORMANCE
	if (newTop->guard != _guard)
	{
		// CRASH
		return false;
	}
#endif

	// NOTE
	// _InterlockedCompareExchange128�� _InterlockedCompareExchange64���� �� ������.
	// ���� Free�κп����� 128 ������� �ʰ� 64�� ä����
	// �׷����ؼ� Alloc�� Node Pop�ϴ� �κп��� �߻��� �� �ִ� ������ �����ؾ��Ѵ�.
	do
	{
		nowTop = _top->nodePtr;
		newTop->next = nowTop;
	} while (_InterlockedCompareExchange64((LONG64*)&_top->nodePtr, (LONG64)newTop, (LONG64)nowTop) != (LONG64)nowTop);

	_InterlockedIncrement(&_nodeCount);
	return true;
}