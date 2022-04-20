#pragma once

#include<Windows.h>
#include "MemoryPool.h"
#include "Macro.h"

template<typename DATA>
class LockFreeQueue
{
	struct Node;

	struct alignas(16) CountedNode
	{
		long64 uniqueCount;
		Node* nodePtr;
	};

	struct Node
	{
		inline void PlacementNewInit() {}
		inline void Init() {}
		DATA data;
		Node* next;
	};

public:
	LockFreeQueue();
	~LockFreeQueue();

	void Enqueue(const DATA& data);
	bool Dequeue(DATA& data);
	void ClearBuffer();

	long GetUseCount() { return _useCount; }
private:
	CountedNode* _head;
	CountedNode* _tail;
	MemoryPool<Node> _pool;

	alignas(64) long _useCount;
};

template<typename DATA>
inline LockFreeQueue<DATA>::LockFreeQueue()
	:_useCount(0)
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

	while (true)
	{
		tail.uniqueCount = _tail->uniqueCount;
		tail.nodePtr = _tail->nodePtr;
		next = tail.nodePtr->next;

		if (next == nullptr)
		{
			if (_InterlockedCompareExchangePointer((PVOID*)&tail.nodePtr->next, pNewNode, nullptr) == nullptr)
			{
				// NOTE
				// �ٸ� ������ tail�� �̵���Ű�鼭 Enq, Deq�� �߻��Ǳ� ������ 
				// pNewNode�� �̹� release�� �� �ִ� ���ɼ��� �����Ѵ�.
				// ���� ���⼭�� DoubleCAS�� ������ ��ߵȴ�.
				_InterlockedCompareExchange128((long64*)_tail, (long64)pNewNode, tail.uniqueCount + 1, (long64*)&tail);
				break;
			}
		}
		else
		{
			// NOTE
			// ������ ���������� ������ DoubleCAS ����ؾ� �ȴ�.
			// ���� _tail->next�� NULL�� �ƴϸ� ���� �Űܳ��� Enq�ϰ� ��������.
			_InterlockedCompareExchange128((long64*)_tail, (long64)next, tail.uniqueCount + 1, (long64*)&tail);
		}
	}

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
					_InterlockedCompareExchange128((long64*)_tail, (long64)next, tail.uniqueCount + 1, (long64*)&tail);
				}
			}
			else
			{
				copyData = nextHead->data;

#ifndef dfSMART_PACKET_PTR
				if (_InterlockedCompareExchange128((long64*)_head, (long64)nextHead, releaseHead.uniqueCount + 1, (long64*)&releaseHead))
					break;
#else
				if (_InterlockedCompareExchange128((long64*)_head, (long64)nextHead, releaseHead.uniqueCount + 1, (long64*)&releaseHead))
					break;
				else
				{
					// TODO
					// ���� ã�� 
					//DATA(data._ptr).~DATA();
					copyData .~DATA();
				}
#endif
				
			}
		}
	}

	data = copyData;
	_pool.Free(releaseHead.nodePtr);

#ifdef dfSMART_PACKET_PTR
	copyData .~DATA();
#endif

	return true;
}

template<typename DATA>
inline void LockFreeQueue<DATA>::ClearBuffer()
{
	DATA data;
	while (Dequeue(data));
}
