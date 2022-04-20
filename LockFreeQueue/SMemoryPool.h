#pragma once

#include<Windows.h>

template<typename DATA>
class SMemoryPool
{
	struct Node;

	// NOTE
	// Alloc �Լ����� CountedNode�� ���������� �����ϰ� �ִ�.
	// DoubleCAS�� ������ ��������Ƿ� 16Byte ���� �ʼ�
	struct alignas(16) CountedNode
	{
		// NOTE 
		// Alloc �Լ����� CountedNode�� ���������� Ŭ������ ����� 
		// ����ü ������ �� ��� ������ �����ؾ��Ѵ�.
		// uniqueCount�� ��������Ʈ�� �����ϸ� ���� �߻� ���ɼ� �����Ѵ�.
		LONG64 uniqueCount;
		Node* nodePtr;
	};

	struct Node
	{
		DATA data;
		Node* next;
	};

public:
	SMemoryPool();
	~SMemoryPool();

	DATA*			Alloc();
	void			Free(DATA* ptr);

	LONG64			GetAllocCount() { return _allocCount; }
	int				GetUseCount() { return _useCount; }
	int				GetNodeCount() { return _nodeCount; }

private:
	CountedNode* _top;

	// NOTE
	// _top, _allocCount, _useCount, _nodeCount ���� Interlocked �Լ� ���
	// ���� ���� ĳ�ö��ο� �����ϰ� �ȴٸ� ���� ������ �������� ĳ�ö��ο� �����ϱ� ����
	// �ϵ���� ������ ����ϴ� ������ �߻��� �� �ִ�.
	// ���� alignas�� ����Ͽ� ĳ�ö��θ�ŭ ���� ��� �����ߴ�.
	alignas(64) LONG64	_allocCount;
	alignas(64) long	_useCount;
	alignas(64) long	_nodeCount;
};

template<typename DATA>
inline SMemoryPool<DATA>::SMemoryPool()
	:_useCount(0), _nodeCount(0), _allocCount(0)
{
	_top = (CountedNode*)_aligned_malloc(sizeof(CountedNode), 16);
	_top->nodePtr = nullptr;
	_top->uniqueCount = 0;
}

template<typename DATA>
inline SMemoryPool<DATA>::~SMemoryPool()
{
	Node* top = _top->nodePtr;
	_aligned_free(_top);

	while (top)
	{
		Node* dNode = top;
		top = dNode->next;
		delete dNode;
	}
}

template<typename DATA>
inline DATA* SMemoryPool<DATA>::Alloc()
{
	CountedNode popTop;
	Node* retNode = nullptr;

	// NOTE
	// ���ʿ� ������ _InterlockedDecrement�� ���� �������ν�
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
		retNode = new Node;

		// NOTE
		// ���ÿ� 2�� �̻��� �����尡 �� �ڵ带 �����ų �� �ֱ� ������
		// ��Ȯ�� AllocCount�� ����ϱ� ���� Interlocked ����ߴ�.
		_InterlockedIncrement64(&_allocCount);
	}

	_InterlockedIncrement(&_useCount);
	return (DATA*)retNode;
}

template<typename DATA>
inline void SMemoryPool<DATA>::Free(DATA* ptr)
{
	Node* newTop = (Node*)ptr;
	Node* nowTop;

	// NOTE
	// _InterlockedCompareExchange128�� _InterlockedCompareExchange64���� �� ������.
	// ���� Free�κп����� 128 ������� �ʰ� 64�� ä����
	// �׷����ؼ� Alloc�� Node�� Pop�ϴ� �κп��� ���� �߻��� �� �ִ��� ������ �� �־�� �ȴ�.

	do
	{
		nowTop = _top->nodePtr;
		newTop->next = nowTop;
	} while (_InterlockedCompareExchange64((LONG64*)&_top->nodePtr, (LONG64)newTop, (LONG64)nowTop) != (LONG64)nowTop);

	_InterlockedIncrement(&_nodeCount);
	_InterlockedDecrement(&_useCount);
}