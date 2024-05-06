#include <Windows.h>
#include "CLanPacket.h"

MemoryPoolTLS<CLanPacket> CLanPacket::_packetpool(50000, POOL_MAX_ALLOC, false);

CLanPacket* CLanPacket::Alloc()
{
	CLanPacket* pPacket = _packetpool.Alloc();
	pPacket->AddRefCount();
	return pPacket;
}

void CLanPacket::Free(CLanPacket* pPacket)
{
	if (InterlockedDecrement((LONG*)&pPacket->_refCount) == 0)
	{
		pPacket->Clear();
		_packetpool.Free(pPacket);
	}
}

CLanPacket::CLanPacket() :_refCount(0), _cBufferSize(DEFAULT_SIZE)
{
	_pBuffer = new char[DEFAULT_SIZE];
	_pReadPos = _pWritePos = _pBuffer + sizeof(NetHeader);
	_isHeaderSet = false;
}

CLanPacket::CLanPacket(int size) :_refCount(0), _cBufferSize(size)
{
	_pBuffer = new char[size];
	_pReadPos = _pWritePos = _pBuffer + sizeof(NetHeader);
	_isHeaderSet = false;
}

CLanPacket::~CLanPacket()
{
	delete[] _pBuffer;
}

//////////////////////////////////////////////////////////////////////////
// ��Ŷ  �ı�.
//
// Parameters: ����.
// Return: ����.
//////////////////////////////////////////////////////////////////////////
void CLanPacket::Release(void)
{
	this->~CLanPacket();
}

//////////////////////////////////////////////////////////////////////////
// ��Ŷ û��.
//
// Parameters: ����.
// Return: ����.
//////////////////////////////////////////////////////////////////////////
void CLanPacket::Clear(void)
{
	_pWritePos = _pReadPos = _pBuffer + sizeof(NetHeader);
	_isHeaderSet = false;
}

//////////////////////////////////////////////////////////////////////////
// ���� ������ ���.
//
// Parameters: ����.
// Return: (int)��Ŷ ���� ������ ���.
//////////////////////////////////////////////////////////////////////////
int	CLanPacket::GetBufferSize(void)
{
	return _cBufferSize;
}

//////////////////////////////////////////////////////////////////////////
// ���� ������� ������ ���.
//
// Parameters: ����.
// Return: (int)������� ����Ÿ ������.
//////////////////////////////////////////////////////////////////////////
int CLanPacket::GetPayloadSize(void)
{
	return _pWritePos - _pReadPos;
}

int	CLanPacket::GetTotalSize(void)
{
	return _pWritePos - _pBuffer;
}

//////////////////////////////////////////////////////////////////////////
// ���� ������ ���.
//
// Parameters: ����.
// Return: (char *)���� ������.
//////////////////////////////////////////////////////////////////////////
char* CLanPacket::GetBufferPtr(void)
{
	return _pBuffer;
}

char* CLanPacket::GetPayloadPtr(void)
{
	return _pBuffer + sizeof(NetHeader);
}

//////////////////////////////////////////////////////////////////////////
// ���� Pos �̵�. (�����̵��� �ȵ�)
// GetBufferPtr �Լ��� �̿��Ͽ� �ܺο��� ������ ���� ������ ������ ��� ���. 
//
// Parameters: (int) �̵� ������.
// Return: (int) �̵��� ������.
//////////////////////////////////////////////////////////////////////////
int CLanPacket::MoveWritePos(int size)
{
	int remainSize = _cBufferSize - GetTotalSize();

	if (size > remainSize)
		size = remainSize;

	_pWritePos += size;
	return size;
}

int CLanPacket::MoveReadPos(int size)
{
	int remainSize = _pWritePos - _pReadPos;

	if (size > remainSize)
		size = remainSize;

	_pReadPos += size;
	return size;
}

/* ============================================================================= */
// ������ �����ε�
/* ============================================================================= */

//////////////////////////////////////////////////////////////////////////
// �ֱ�.	�� ���� Ÿ�Ը��� ��� ����.
//////////////////////////////////////////////////////////////////////////
CLanPacket& CLanPacket::operator<< (char val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}
CLanPacket& CLanPacket::operator<< (unsigned char val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}

CLanPacket& CLanPacket::operator<< (short val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}
CLanPacket& CLanPacket::operator<< (unsigned short val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}

CLanPacket& CLanPacket::operator<< (int val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}
CLanPacket& CLanPacket::operator<< (unsigned int val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}

CLanPacket& CLanPacket::operator<< (long val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}
CLanPacket& CLanPacket::operator<< (unsigned long val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}

CLanPacket& CLanPacket::operator<< (__int64 val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}
CLanPacket& CLanPacket::operator<< (unsigned __int64 val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}

CLanPacket& CLanPacket::operator<< (float val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}
CLanPacket& CLanPacket::operator<< (double val)
{
	SetData((char*)&val, sizeof(val));
	return *this;
}


//////////////////////////////////////////////////////////////////////////
// ����.	�� ���� Ÿ�Ը��� ��� ����.
//////////////////////////////////////////////////////////////////////////
CLanPacket& CLanPacket::operator>> (char& chValue)
{
	GetData((char*)&chValue, sizeof(chValue));
	return *this;
}
CLanPacket& CLanPacket::operator>> (unsigned char& byValue)
{
	GetData((char*)&byValue, sizeof(byValue));
	return *this;
}

CLanPacket& CLanPacket::operator>> (short& shValue)
{
	GetData((char*)&shValue, sizeof(shValue));
	return *this;
}
CLanPacket& CLanPacket::operator>> (unsigned short& wValue)
{
	GetData((char*)&wValue, sizeof(wValue));
	return *this;
}

CLanPacket& CLanPacket::operator>> (int& iValue)
{
	GetData((char*)&iValue, sizeof(iValue));
	return *this;
}
CLanPacket& CLanPacket::operator>> (unsigned int& uiValue)
{
	GetData((char*)&uiValue, sizeof(uiValue));
	return *this;
}

CLanPacket& CLanPacket::operator>> (long lValue)
{
	GetData((char*)&lValue, sizeof(lValue));
	return *this;
}
CLanPacket& CLanPacket::operator>> (unsigned long& ulValue)
{
	GetData((char*)&ulValue, sizeof(ulValue));
	return *this;
}

CLanPacket& CLanPacket::operator>> (__int64& iValue)
{
	GetData((char*)&iValue, sizeof(iValue));
	return *this;
}
CLanPacket& CLanPacket::operator>> (unsigned __int64& uiValue)
{
	GetData((char*)&uiValue, sizeof(uiValue));
	return *this;
}

CLanPacket& CLanPacket::operator>> (float& fValue)
{
	GetData((char*)&fValue, sizeof(fValue));
	return *this;
}
CLanPacket& CLanPacket::operator>> (double& dValue)
{
	GetData((char*)&dValue, sizeof(dValue));
	return *this;
}



//////////////////////////////////////////////////////////////////////////
// ����Ÿ ���.
//
// Parameters: (char *)Dest ������. (int)Size.
// Return: (int)������ ������.
//////////////////////////////////////////////////////////////////////////
int CLanPacket::GetData(char* pDest, int size)
{
	int remainSize = _pWritePos - _pReadPos;
	if (size > remainSize)
		size = remainSize;

	memcpy_s(pDest, size, _pReadPos, size);
	_pReadPos += size;
	return size;
}

//////////////////////////////////////////////////////////////////////////
// ����Ÿ ����.
//
// Parameters: (char *)Src ������. (int)SrcSize.
// Return: (int)������ ������.
//////////////////////////////////////////////////////////////////////////
int CLanPacket::SetData(char* pSrc, int srcSize)
{
	int remainSize = _cBufferSize - (_pWritePos - _pBuffer);

	if (srcSize > remainSize)
		srcSize = remainSize;

	memcpy_s(_pWritePos, srcSize, pSrc, srcSize);
	_pWritePos += srcSize;
	return srcSize;
}

void CLanPacket::GetLanHeader(char* pDest)
{
	memcpy_s(pDest, sizeof(NetHeader), _pBuffer, sizeof(NetHeader));
}

void CLanPacket::SetLanHeader()
{
	if (_isHeaderSet)
		return;
	NetHeader header = { GetPayloadSize() };
	memcpy_s(_pBuffer, sizeof(NetHeader), (char*)&header, sizeof(NetHeader));
	_isHeaderSet = true;
}

void CLanPacket::AddRefCount()
{
	InterlockedIncrement((LONG*)&_refCount);
}