#pragma once

#include "pch.h"
#include "Macro.h"

#ifndef _PERFORMANCE
	extern ulong64 g_ChunkGuard;
	extern ulong64 g_Guard;
#endif

//enum eTYPE
//{
//	RECV = 0,
//	SEND = 1
//};
//
//// SessionId ���� 2����Ʈ(���� �迭�� �ε���), ���� 6����Ʈ(������ Unique Id)
//// ������ ��������ν� ������ ���ǹ迭�� �˻�
//typedef union
//{
//	struct
//	{
//		uint64 id : 48;
//		uint64 index : 16;
//	}DUMMYSTRUCTNAME;
//
//	uint64 sessionId;
//}SessionId;