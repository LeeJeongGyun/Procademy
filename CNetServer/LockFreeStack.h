#pragma once

#include<windows.h>
#include "MemoryPool.h"

template<typename DATA>
class LockFreeStack
{
	struct Node;

	struct CountedNode
	{
		LONG64 uniqueCount;
		Node* nodePtr;
	};

	struct Node
	{
		inline void PlacementNewInit() {}
		inline void Init() {}
		DATA pData;
		Node* next;
	};

public:
	LockFreeStack();
	~LockFreeStack();

	void Push(const DATA& pdata);
	bool Pop(DATA& pData);

	int GetUseCount() const { return _useCount; }

private:
	CountedNode* _top;
	MemoryPool<Node> pool;

	// _top�� _useCount�� ������� ������.
	// ���� ĳ�ö��ο� ������ Interlocked���� �Լ� ����� �� ����� ���µ� ����ϴ� ��찡 �߻��� �� �ִ�.
	alignas(64) long _useCount;
};

template<typename DATA>
inline LockFreeStack<DATA>::LockFreeStack()
	: _useCount(0)
{
	_top = (CountedNode*)_aligned_malloc(sizeof(CountedNode), 16);
	_top->nodePtr = nullptr;
}

template<typename DATA>
inline LockFreeStack<DATA>::~LockFreeStack()
{
	Node* top = _top->nodePtr;
	_aligned_free(_top);

	while (top)
	{
		Node* allocNode = top;
		top = top->next;
		pool.Free(allocNode);
	}
}

template<typename DATA>
inline void LockFreeStack<DATA>::Push(const DATA& data)
{
	Node* pTop;
	Node* pNewNode = pool.Alloc();
	pNewNode->pData = data;

	do {
		pTop = _top->nodePtr;
		pNewNode->next = pTop;
	} while (_InterlockedCompareExchange64((LONG64*)&_top->nodePtr, (LONG64)pNewNode, (LONG64)pTop) != (LONG64)pTop);

	InterlockedIncrement(&_useCount);
}

template<typename DATA>
inline bool LockFreeStack<DATA>::Pop(DATA& pData)
{
	alignas(16) CountedNode popTop;

	// Push �ѹ��ϰ� Pop�� 2���� �����尡 ���´ٸ� if(_useCount == 0) �̷��� �ϸ� �����ȴ�.
	if (InterlockedDecrement(&_useCount) < 0)
	{
		InterlockedIncrement(&_useCount);
		return false;
	}

	do
	{
		// ���� �߻� ���ɼ� ����!! byte���� �����϶�
		//popTop = *_top;
		popTop.uniqueCount = _top->uniqueCount;
		popTop.nodePtr = _top->nodePtr;
	} while (false == _InterlockedCompareExchange128((LONG64*)_top, (LONG64)popTop.nodePtr->next, popTop.uniqueCount + 1, (LONG64*)&popTop));

	pData = popTop.nodePtr->pData;
	pool.Free(popTop.nodePtr);
	return true;
}