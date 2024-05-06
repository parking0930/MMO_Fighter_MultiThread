#pragma once
#include "MemoryPoolTLS.h"
#define	DEFAULT_SIZE		1024

class CLanPacket
{
public:
	void	Release(void);
	void	Clear(void);
	int		GetBufferSize(void);
	int		GetPayloadSize(void);
	int		GetTotalSize(void);
	char*	GetBufferPtr(void);
	char*	GetPayloadPtr(void);
	int		MoveWritePos(int size);
	int		MoveReadPos(int size);
	int		GetData(char* pDest, int size);
	int		SetData(char* pSrc, int srcSize);
	void	GetLanHeader(char* pDest);
	void	SetLanHeader();
	void	AddRefCount();

	CLanPacket& operator<< (char val);
	CLanPacket& operator<< (unsigned char val);

	CLanPacket& operator<< (short val);
	CLanPacket& operator<< (unsigned short val);

	CLanPacket& operator<< (int val);
	CLanPacket& operator<< (unsigned int val);

	CLanPacket& operator<< (long val);
	CLanPacket& operator<< (unsigned long val);

	CLanPacket& operator<< (__int64 val);
	CLanPacket& operator<< (unsigned __int64 val);

	CLanPacket& operator<< (float val);
	CLanPacket& operator<< (double val);
	////////////////////////////////////////////////
	CLanPacket& operator>> (char& val);
	CLanPacket& operator>> (unsigned char& val);

	CLanPacket& operator>> (short& val);
	CLanPacket& operator>> (unsigned short& val);

	CLanPacket& operator>> (int& val);
	CLanPacket& operator>> (unsigned int& val);

	CLanPacket& operator>> (long val);
	CLanPacket& operator>> (unsigned long& val);

	CLanPacket& operator>> (__int64& val);
	CLanPacket& operator>> (unsigned __int64& val);

	CLanPacket& operator>> (float& val);
	CLanPacket& operator>> (double& val);
private:
	CLanPacket();
	CLanPacket(int size);
	CLanPacket(const CLanPacket& packet) = delete;
	CLanPacket& operator = (const CLanPacket& packet) = delete;
	virtual	~CLanPacket();
public:
	static CLanPacket* Alloc();
	static void			Free(CLanPacket* pPacket);
private:
	struct NetHeader
	{
		USHORT	len;
	};
private:
	friend class CLanServer;
	friend class MemoryPoolTLS<CLanPacket>;
	static MemoryPoolTLS<CLanPacket> _packetpool;
protected:
	char* _pBuffer;
	char* _pWritePos;
	char* _pReadPos;
	int			_refCount;
	const int	_cBufferSize;
	bool		_isHeaderSet;
};