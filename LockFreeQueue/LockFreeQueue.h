#pragma once

#include<Windows.h>
#include "SMemoryPool.h"
#define dfLOG_ARR_SIZE 16384

template<typename DATA>
class LockFreeQueue
{
	struct Node;

	struct alignas(64) CountedNode
	{
		LONG64 uniqueCount;
		Node* nodePtr;
	};


	struct Node
	{
		DATA data;
		Node* next;
	};

	// LOG
	struct Log
	{
		DWORD id;
		char type;
		Node* head;
		Node* nextHead;
		Node* copyTail;
		Node* nowTail;
		Node* newTail;
		DATA dataPtr;
	};

public:
	LockFreeQueue();
	~LockFreeQueue();

	void Enqueue(const DATA& data);
	bool Dequeue(DATA& data);

	long GetUseCount() { return _useCount; }

	inline void PrintLog(BYTE type, Node* head, Node* nextHead, Node* copytail, Node* nowTail, Node* newTail, DATA ptr = nullptr);

private:
	CountedNode* _head;
	CountedNode* _tail;
	SMemoryPool<Node> _pool;

	alignas(64) long _useCount;

	// LOG
	Log _memoryLog[dfLOG_ARR_SIZE];
	LONG64 _logIndex = -1;
};

template<typename DATA>
inline LockFreeQueue<DATA>::LockFreeQueue()
	:_useCount(0), _logIndex(-1)
{
	// NOTE
	// CAS������ �� �� ENQ�� DEQ�� ���� ������ �������� ĳ�ö��θ�ŭ ����߷�����
	// ENQ CAS������ �� �� DEQ CAS������ ���� ���� �ʵ��� �ϱ� ���� 64BYTE ��迡 �����.
	_head = (CountedNode*)_aligned_malloc(sizeof(CountedNode), 64);
	_tail = (CountedNode*)_aligned_malloc(sizeof(CountedNode), 64);
	_head->nodePtr = _pool.Alloc();
	_tail->nodePtr = _head->nodePtr;
	_head->uniqueCount = 0;
	_tail->uniqueCount = 0;
	_tail->nodePtr->next = nullptr;
}

template<typename DATA>
inline LockFreeQueue<DATA>::~LockFreeQueue()
{
	Node* node = _head->nodePtr;
	_aligned_free(_head);
	_aligned_free(_tail);

	while (node)
	{
		Node* dNode = node;
		node = node->next;
		_pool.Free(dNode);
	}
}

template<typename DATA>
inline void LockFreeQueue<DATA>::Enqueue(const DATA& data)
{
	CountedNode tail;
	Node* next;

	// NOTE
	// ENQ�� ���� ���� Alloc���� ����� next�� NULL�� �о�� �ȴ�.
	// ������ tail �����͸� ������ CAS������ �ϱ� ������ tail �����Ͱ� ����Ű�� ������ ������� ���� �Ұ����ϴ�.
	// ���� �ƹ���峪 next�� NULL�� �� ��� Queue�� �ƴ� ������ ���ο� ��带 ������ ���ɼ��� �����.
	// ���� ENQ������ tail�� next�� NULL�� ����!
	Node* pNewNode = _pool.Alloc();
	pNewNode->next = nullptr;
	pNewNode->data = data;

	//LOG
	Node* head;

	while (true)
	{
		tail.uniqueCount = _tail->uniqueCount;
		tail.nodePtr = _tail->nodePtr;
		next = tail.nodePtr->next;

		// LOG
		head = _head->nodePtr;
		PrintLog(1, head, head->next, tail.nodePtr, _tail->nodePtr, pNewNode, pNewNode->data);

		if (next == nullptr)
		{
			if (_InterlockedCompareExchangePointer((PVOID*)&tail.nodePtr->next, pNewNode, nullptr) == nullptr)
			{
				//LOG
				PrintLog(2, head, head->next, tail.nodePtr, _tail->nodePtr, pNewNode, pNewNode->data);

				// NOTE
				// �ٸ� ������ tail�� �̵���Ű�鼭 Enq, Deq�� �߻��Ǳ� ������ 
				// pNewNode�� �̹� release�� �� �ִ� ���ɼ��� �����Ѵ�.
				// ���� ���⼭�� DoubleCAS�� ������ ��ߵȴ�.
				_InterlockedCompareExchange128((LONG64*)_tail, (LONG64)pNewNode, tail.uniqueCount + 1, (LONG64*)&tail);
				break;
			}
		}
		else
		{
			// NOTE
			// ������ ���������� ������ DoubleCAS ����ؾ� �ȴ�.
			// ���� _tail->next�� NULL�� �ƴϸ� ���� �Űܳ��� Enq�ϰ� ��������.
			_InterlockedCompareExchange128((LONG64*)_tail, (LONG64)next, tail.uniqueCount + 1, (LONG64*)&tail);
		}
	}

	//LOG
	PrintLog(3, head, head->next, tail.nodePtr, _tail->nodePtr, pNewNode, pNewNode->data);

	_InterlockedIncrement(&_useCount);
}

template<typename DATA>
inline bool LockFreeQueue<DATA>::Dequeue(DATA& data)
{
	if (_InterlockedDecrement(&_useCount) < 0)
	{
		_InterlockedIncrement(&_useCount);
		return false;
	}

	CountedNode releaseHead;
	Node* nextHead;
	DATA copyData = 0;

	CountedNode tail;
	Node* next;

	while (true)
	{
		releaseHead.uniqueCount = _head->uniqueCount;
		releaseHead.nodePtr = _head->nodePtr;
		nextHead = releaseHead.nodePtr->next;

		if (nextHead != NULL)
		{
			tail.uniqueCount = _tail->uniqueCount;
			tail.nodePtr = _tail->nodePtr;

			// NOTE 
			// _head�� _tail �Ѿ�� ��� ����
			if (releaseHead.nodePtr == tail.nodePtr)
			{
				// NOTE
				// _head�� _tail �Ѿ�� ������ �߻��� �� �ִ� Deque ���ѷ��� ����
				next = tail.nodePtr->next;

				if (next)
				{
					_InterlockedCompareExchange128((LONG64*)_tail, (LONG64)next, tail.uniqueCount + 1, (LONG64*)&tail);
				}
			}
			else
			{
				copyData = nextHead->data;

				if (_InterlockedCompareExchange128((LONG64*)_head, (LONG64)nextHead, releaseHead.uniqueCount + 1, (LONG64*)&releaseHead))
					break;
			}
		}
	}

	//LOG
	PrintLog(4, releaseHead.nodePtr, nextHead, tail.nodePtr, _tail->nodePtr, tail.nodePtr->next, copyData);

	data = copyData;
	_pool.Free(releaseHead.nodePtr);
	
	return true;
}

template<typename DATA>
inline void LockFreeQueue<DATA>::PrintLog(BYTE type, Node* head, Node* nextHead, Node* copytail, Node* nowTail, Node* newTail, DATA dataPtr)
{
	LONG64 localIndex = _InterlockedIncrement64(&_logIndex);
	localIndex &= (dfLOG_ARR_SIZE - 1);

	_memoryLog[localIndex].id = GetCurrentThreadId();
	_memoryLog[localIndex].type = type;
	_memoryLog[localIndex].head = head;
	_memoryLog[localIndex].nextHead = nextHead;
	_memoryLog[localIndex].copyTail = copytail;
	_memoryLog[localIndex].nowTail = nowTail;
	_memoryLog[localIndex].newTail = newTail;
	_memoryLog[localIndex].dataPtr = dataPtr;
}
