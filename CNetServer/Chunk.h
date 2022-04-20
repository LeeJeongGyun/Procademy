#pragma once

#include<Windows.h>
#include "Macro.h"

template<typename T>
class Chunk
{
public:
	typedef struct ChunkNode
	{
		T data;
		Chunk* pChunk;

#ifndef _PERFORMANCE
		ULONG64 guard;
#endif
	}ChunkNode;

public:
	Chunk();
	~Chunk();

	//////////////////////////////////////////////////////////////////////////
	// ChunkNode ���� DATA ������ ȣ���� ���� new �Ҵ�
	//
	// Parameters: ����.
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void Init();

	//////////////////////////////////////////////////////////////////////////
	// placementNew�� �̿��ϱ� ���ؼ� ChunkNode�迭 malloc�� �̿��� �Ҵ�
	//
	// Parameters: ����.
	// Return: ����.
	//////////////////////////////////////////////////////////////////////////
	void PlacementNewInit();

public:
	ChunkNode* node;
	long _allocCount;
	long _freeCount;

#ifndef _PERFORMANCE
	ULONG64 guard;
#endif
};

template<typename T>
inline Chunk<T>::Chunk()
	:_allocCount(0), _freeCount(0)
{
}

template<typename T>
inline Chunk<T>::~Chunk()
{
}

template<typename T>
inline void Chunk<T>::Init()
{
	node = new ChunkNode[dfCHUNK_SIZE];

#ifndef _PERFORMANCE
	guard = InterlockedIncrement(&g_ChunkGuard);
#endif

	for (int iCnt = 0; iCnt < dfCHUNK_SIZE; iCnt++)
	{
		node[iCnt].pChunk = this;
#ifndef _PERFORMANCE
		node[iCnt].guard = guard;
#endif
	}
}

template<typename T>
inline void Chunk<T>::PlacementNewInit()
{
	node = (ChunkNode*)malloc(sizeof(ChunkNode) * dfCHUNK_SIZE);

#ifndef _PERFORMANCE
	guard = InterlockedIncrement(&g_ChunkGuard);
#endif

	for (int iCnt = 0; iCnt < dfCHUNK_SIZE; iCnt++)
	{
		node[iCnt].pChunk = this;
#ifndef _PERFORMANCE
		node[iCnt].guard = guard;
#endif
	}
}
