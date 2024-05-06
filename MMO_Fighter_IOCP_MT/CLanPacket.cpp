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
// 패킷  파괴.
//
// Parameters: 없음.
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
void CLanPacket::Release(void)
{
	this->~CLanPacket();
}

//////////////////////////////////////////////////////////////////////////
// 패킷 청소.
//
// Parameters: 없음.
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
void CLanPacket::Clear(void)
{
	_pWritePos = _pReadPos = _pBuffer + sizeof(NetHeader);
	_isHeaderSet = false;
}

//////////////////////////////////////////////////////////////////////////
// 버퍼 사이즈 얻기.
//
// Parameters: 없음.
// Return: (int)패킷 버퍼 사이즈 얻기.
//////////////////////////////////////////////////////////////////////////
int	CLanPacket::GetBufferSize(void)
{
	return _cBufferSize;
}

//////////////////////////////////////////////////////////////////////////
// 현재 사용중인 사이즈 얻기.
//
// Parameters: 없음.
// Return: (int)사용중인 데이타 사이즈.
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
// 버퍼 포인터 얻기.
//
// Parameters: 없음.
// Return: (char *)버퍼 포인터.
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
// 버퍼 Pos 이동. (음수이동은 안됨)
// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
//
// Parameters: (int) 이동 사이즈.
// Return: (int) 이동된 사이즈.
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
// 연산자 오버로딩
/* ============================================================================= */

//////////////////////////////////////////////////////////////////////////
// 넣기.	각 변수 타입마다 모두 만듬.
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
// 빼기.	각 변수 타입마다 모두 만듬.
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
// 데이타 얻기.
//
// Parameters: (char *)Dest 포인터. (int)Size.
// Return: (int)복사한 사이즈.
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
// 데이타 삽입.
//
// Parameters: (char *)Src 포인터. (int)SrcSize.
// Return: (int)복사한 사이즈.
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