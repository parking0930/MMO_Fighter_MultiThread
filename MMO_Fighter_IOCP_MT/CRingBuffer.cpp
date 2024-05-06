#include "CRingBuffer.h"
#include <string.h>
#include <stdio.h>

CRingBuffer::CRingBuffer() :iFront(0), iRear(0), iTotalSize(DEFAULT_BUFSIZE + 1)
{
	buffer = new char[DEFAULT_BUFSIZE + 1];
}

CRingBuffer::CRingBuffer(int iBufferSize) :iFront(0), iRear(0), iTotalSize(iBufferSize + 1)
{
	buffer = new char[iBufferSize + 1];
}

CRingBuffer::~CRingBuffer()
{
	delete[] buffer;
}

void CRingBuffer::Resize(int iSize)
{
	if (iSize <= iTotalSize)
		return;
	char* newbuffer = new char[iSize + 1];
	memcpy_s(newbuffer, iSize + 1, buffer, iTotalSize);
	delete[] buffer;
	buffer = newbuffer;
	iTotalSize = iSize + 1;
}

int	CRingBuffer::GetBufferSize()
{
	return iTotalSize - 1;
}


/////////////////////////////////////////////////////////////////////////
// ���� ������� �뷮 ���.
//
// Parameters: ����.
// Return: (int)������� �뷮.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::GetUseSize(void)
{
	int iBkupFront = iFront;
	int iBkupRear = iRear;

	if (iBkupFront == iBkupRear)
		return 0;
	if (iBkupFront < iBkupRear)
		return iBkupRear - iBkupFront;
	if (iBkupFront > iBkupRear)
		return iTotalSize - iBkupFront + iRear;
}

/////////////////////////////////////////////////////////////////////////
// ���� ���ۿ� ���� �뷮 ���. 
//
// Parameters: ����.
// Return: (int)�����뷮.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::GetFreeSize(void)
{
	int iBkupFront = iFront;
	int iBkupRear = iRear;

	if (iBkupFront == iBkupRear)
		return iTotalSize - 1;
	if (iBkupFront > iBkupRear)
		return iBkupFront - iBkupRear - 1;
	if (iBkupFront < iBkupRear)
		return iTotalSize - 1 - iBkupRear + iBkupFront;
}

/////////////////////////////////////////////////////////////////////////
// ���� �����ͷ� �ܺο��� �ѹ濡 �а�, �� �� �ִ� ����.
// (������ ���� ����)
//
// ���� ť�� ������ ������ ���ܿ� �ִ� �����ʹ� �� -> ó������ ���ư���
// 2���� �����͸� ��ų� ���� �� ����. �� �κп��� �������� ���� ���̸� �ǹ�
//
// Parameters: ����.
// Return: (int)��밡�� �뷮.
////////////////////////////////////////////////////////////////////////
int	CRingBuffer::DirectEnqueueSize(void)
{
	int iBkupFront = iFront;
	int iBkupRear = (iRear + 1) % iTotalSize;

	if (iBkupFront < iBkupRear)
		return iTotalSize - iBkupRear;
	if (iBkupFront >= iBkupRear)
		return iBkupFront - iBkupRear;
}

int	CRingBuffer::DirectDequeueSize(void)
{
	int iBkupFront = iFront;
	int iBkupRear = iRear;

	if (iBkupFront == iBkupRear)
		return 0;

	iBkupFront = (iBkupFront + 1) % iTotalSize;

	if (iBkupFront > iBkupRear)
		return iTotalSize - iBkupFront;
	if (iBkupFront <= iBkupRear)
		return iBkupRear - iBkupFront + 1;
}


/////////////////////////////////////////////////////////////////////////
// WritePos �� ����Ÿ ����.
//
// Parameters: (char *)����Ÿ ������. (int)ũ��. 
// Return: (int)���� ũ��.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::Enqueue(const char* chpData, int iSize)
{
	int iBkupRear = (iRear + 1) % iTotalSize;
	int iBkupFront = iFront;
	int iEnqSize = 0;

	// 1�� Direct Enqueue Size ����
	int iDEsize;
	if (iBkupFront < iBkupRear)
		iDEsize = iTotalSize - iBkupRear;
	else if (iBkupFront > iBkupRear)
		iDEsize = iBkupFront - iBkupRear;
	else // queue full
		return 0;

	// �� ���� Enqueue�� ���� �� �ִ��� �Ǵ�
	if (iSize > iDEsize)
	{
		iEnqSize = iDEsize;

		// 1�� Enqueue
		memcpy_s(buffer + iBkupRear, iEnqSize, chpData, iEnqSize);
		iBkupRear = iBkupRear + (iEnqSize - 1);

		int tmpRear = (iBkupRear + 1) % iTotalSize;
		// Enqueue�� �� ���������� ���� ���(���� ������ �Ϻθ� ���� ����̴�.)
		if (tmpRear == iBkupFront)
		{
			iRear = iBkupRear;
			return iEnqSize;
		}

		// 2�� Enqueue
		iBkupRear = tmpRear;
		iSize -= iEnqSize;

		if (iBkupFront < iBkupRear)
			iDEsize = iTotalSize - iBkupRear;
		else if (iBkupFront > iBkupRear)
			iDEsize = iBkupFront - iBkupRear;

		// Enqueue�� �� ���������� ���� ���(���� ������ �Ϻθ� ���� ����̴�.)
		if (iSize > iDEsize)
			iSize = iDEsize;

		// 2�� Enqueue
		memcpy_s(buffer + iBkupRear, iSize, chpData + iEnqSize, iSize);
		iRear = iBkupRear + (iSize - 1);
		iEnqSize += iSize;
		return iEnqSize;
	}
	else
	{
		iEnqSize = iSize;

		// Enqueue
		memcpy_s(buffer + iBkupRear, iEnqSize, chpData, iEnqSize);
		iRear = iBkupRear + (iEnqSize - 1);
		return iEnqSize;
	}
}

/////////////////////////////////////////////////////////////////////////
// ReadPos ���� ����Ÿ ������. ReadPos �̵�.
//
// Parameters: (char *)����Ÿ ������. (int)ũ��.
// Return: (int)������ ũ��.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::Dequeue(char* chpDest, int iSize)
{
	int iBkupRear = iRear;
	int iBkupFront = iFront;
	int iDqSize = 0;

	// Queue empty case
	if (iBkupRear == iBkupFront)
		return 0;

	iBkupFront = (iBkupFront + 1) % iTotalSize;

	// 1�� Direct dequeue size ����
	int iDDEsize;
	if (iBkupFront > iBkupRear)
		iDDEsize = iTotalSize - iBkupFront;
	else // if (iBkupFront <= iBkupRear)
		iDDEsize = iBkupRear - iBkupFront + 1;

	// �� ���� Dequeue�� ���� �� �ִ��� �Ǵ�
	if (iSize > iDDEsize)
	{
		iDqSize = iDDEsize;

		// 1�� Dequeue
		memcpy_s(chpDest, iDqSize, buffer + iBkupFront, iDqSize);
		iBkupFront = iBkupFront + iDqSize - 1;

		// �� �̻� Dequeue�� �� ���� ���(��û ũ�⺸�� ����ִ� �� ���� ���)
		if (iBkupRear == iBkupFront)
		{
			iFront = iBkupFront;
			return iDqSize;
		}

		// 2�� Dequeue ����
		iBkupFront = (iBkupFront + 1) % iTotalSize;
		iSize -= iDqSize;

		if (iBkupFront > iBkupRear)
			iDDEsize = iTotalSize - iBkupFront;
		else // if (iBkupFront <= iBkupRear)
			iDDEsize = iBkupRear - iBkupFront + 1;


		// �� �̻� Dequeue�� �� ���� ���(��û ũ�⺸�� ����ִ� �� ���� ���)
		if (iSize > iDDEsize)
			iSize = iDDEsize;

		// 2�� Dequeue
		memcpy_s(chpDest + iDqSize, iSize, buffer + iBkupFront, iSize);
		iFront = iBkupFront + iSize - 1;
		iDqSize += iSize;
		return iDqSize;
	}
	else
	{
		iDqSize = iSize;

		memcpy_s(chpDest, iDqSize, buffer + iBkupFront, iDqSize);
		iBkupFront = iBkupFront + iDqSize - 1;

		iFront = iBkupFront;
		return iDqSize;
	}
}

/////////////////////////////////////////////////////////////////////////
// ReadPos ���� ����Ÿ �о��. ReadPos ����.
//
// Parameters: (char *)����Ÿ ������. (int)ũ��.
// Return: (int)������ ũ��.
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::Peek(char* chpDest, int iSize)
{
	int iBkupRear = iRear;
	int iBkupFront = iFront;
	int iDqSize = 0;

	// Queue empty case
	if (iBkupRear == iBkupFront)
		return 0;

	iBkupFront = (iBkupFront + 1) % iTotalSize;

	// 1�� Direct dequeue size ����
	int iDDEsize;
	if (iBkupFront > iBkupRear)
		iDDEsize = iTotalSize - iBkupFront;
	else // if (iBkupFront <= iBkupRear)
		iDDEsize = iBkupRear - iBkupFront + 1;

	// �� ���� Dequeue�� ���� �� �ִ��� �Ǵ�
	if (iSize > iDDEsize)
	{
		iDqSize = iDDEsize;

		// 1�� Dequeue
		memcpy_s(chpDest, iDqSize, buffer + iBkupFront, iDqSize);
		iBkupFront = iBkupFront + iDqSize - 1;

		// �� �̻� Dequeue�� �� ���� ���(��û ũ�⺸�� ����ִ� �� ���� ���)
		if (iBkupRear == iBkupFront)
		{
			return iDqSize;
		}

		// 2�� Dequeue ����
		iBkupFront = (iBkupFront + 1) % iTotalSize;
		iSize -= iDqSize;

		if (iBkupFront > iBkupRear)
			iDDEsize = iTotalSize - iBkupFront;
		else // if (iBkupFront <= iBkupRear)
			iDDEsize = iBkupRear - iBkupFront + 1;


		// �� �̻� Dequeue�� �� ���� ���(��û ũ�⺸�� ����ִ� �� ���� ���)
		if (iSize > iDDEsize)
			iSize = iDDEsize;

		// 2�� Dequeue
		memcpy_s(chpDest + iDqSize, iSize, buffer + iBkupFront, iSize);
		iDqSize += iSize;
		return iDqSize;
	}
	else
	{
		iDqSize = iSize;

		memcpy_s(chpDest, iDqSize, buffer + iBkupFront, iDqSize);
		iBkupFront = iBkupFront + iDqSize - 1;
		return iDqSize;
	}
}


/////////////////////////////////////////////////////////////////////////
// ���ϴ� ���̸�ŭ �б���ġ ���� ���� / ���� ��ġ �̵�
//
// Parameters: ����.
// Return: (int)�̵�ũ��
/////////////////////////////////////////////////////////////////////////
int	CRingBuffer::MoveRear(int iSize)
{
	int iBkupRear = (iRear + 1) % iTotalSize;
	int iBkupFront = iFront;
	int iEnqSize = 0;

	// 1�� Direct Enqueue Size ����
	int iDEsize;
	if (iBkupFront < iBkupRear)
		iDEsize = iTotalSize - iBkupRear;
	else if (iBkupFront > iBkupRear)
		iDEsize = iBkupFront - iBkupRear;
	else // queue full
		return 0;

	// �� ���� Enqueue�� ���� �� �ִ��� �Ǵ�
	if (iSize > iDEsize)
	{
		iEnqSize = iDEsize;

		// 1�� Enqueue
		iBkupRear = iBkupRear + (iEnqSize - 1);

		int tmpRear = (iBkupRear + 1) % iTotalSize;
		// Enqueue�� �� ���������� ���� ���(���� ������ �Ϻθ� ���� ����̴�.)
		if (tmpRear == iBkupFront)
		{
			iRear = iBkupRear;
			return iEnqSize;
		}

		// 2�� Enqueue
		iBkupRear = tmpRear;
		iSize -= iEnqSize;

		if (iBkupFront < iBkupRear)
			iDEsize = iTotalSize - iBkupRear;
		else if (iBkupFront > iBkupRear)
			iDEsize = iBkupFront - iBkupRear;

		// Enqueue�� �� ���������� ���� ���(���� ������ �Ϻθ� ���� ����̴�.)
		if (iSize > iDEsize)
			iSize = iDEsize;

		// 2�� Enqueue
		iRear = iBkupRear + (iSize - 1);
		iEnqSize += iSize;
		return iEnqSize;
	}
	else
	{
		iEnqSize = iSize;

		// Enqueue
		iRear = iBkupRear + (iEnqSize - 1);
		return iEnqSize;
	}
}

int	CRingBuffer::MoveFront(int iSize)
{
	int iBkupRear = iRear;
	int iBkupFront = iFront;
	int iDqSize = 0;

	// Queue empty case
	if (iBkupRear == iBkupFront)
		return 0;

	iBkupFront = (iBkupFront + 1) % iTotalSize;

	// 1�� Direct dequeue size ����
	int iDDEsize;
	if (iBkupFront > iBkupRear)
		iDDEsize = iTotalSize - iBkupFront;
	else // if (iBkupFront <= iBkupRear)
		iDDEsize = iBkupRear - iBkupFront + 1;

	// �� ���� Dequeue�� ���� �� �ִ��� �Ǵ�
	if (iSize > iDDEsize)
	{
		iDqSize = iDDEsize;

		// 1�� Dequeue
		iBkupFront = iBkupFront + iDqSize - 1;

		// �� �̻� Dequeue�� �� ���� ���(��û ũ�⺸�� ����ִ� �� ���� ���)
		if (iBkupRear == iBkupFront)
		{
			iFront = iBkupFront;
			return iDqSize;
		}

		// 2�� Dequeue ����
		iBkupFront = (iBkupFront + 1) % iTotalSize;
		iSize -= iDqSize;

		if (iBkupFront > iBkupRear)
			iDDEsize = iTotalSize - iBkupFront;
		else // if (iBkupFront <= iBkupRear)
			iDDEsize = iBkupRear - iBkupFront + 1;


		// �� �̻� Dequeue�� �� ���� ���(��û ũ�⺸�� ����ִ� �� ���� ���)
		if (iSize > iDDEsize)
			iSize = iDDEsize;

		// 2�� Dequeue
		iFront = iBkupFront + iSize - 1;
		iDqSize += iSize;
		return iDqSize;
	}
	else
	{
		iDqSize = iSize;

		iBkupFront = iBkupFront + iDqSize - 1;
		iFront = iBkupFront;
		return iDqSize;
	}
}

/////////////////////////////////////////////////////////////////////////
// ������ ��� ����Ÿ ����.
//
// Parameters: ����.
// Return: ����.
/////////////////////////////////////////////////////////////////////////
void CRingBuffer::ClearBuffer(void)
{
	iRear = iFront = 0;
}


/////////////////////////////////////////////////////////////////////////
// ������ Front ������ ����.
//
// Parameters: ����.
// Return: (char *) ���� ������.
/////////////////////////////////////////////////////////////////////////
char* CRingBuffer::GetFrontBufferPtr(void)
{
	return buffer + ((iFront + 1) % iTotalSize);
}


/////////////////////////////////////////////////////////////////////////
// ������ RearPos ������ ����.
//
// Parameters: ����.
// Return: (char *) ���� ������.
/////////////////////////////////////////////////////////////////////////
char* CRingBuffer::GetRearBufferPtr(void)
{
	return buffer + ((iRear + 1) % iTotalSize);
}

char* CRingBuffer::GetBufferPtr(void)
{
	return buffer;
}