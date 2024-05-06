#pragma warning(disable: 6258)
#pragma comment(lib,"ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <process.h>
#include <stdio.h>
#include "CLanServer.h"
#include "CrashDump.h"

/////////////////////////////////////////////////////////////////////////

CLanServer::CLanServer()
{
	InitSettings();
}

CLanServer::~CLanServer()
{
	Stop();
}

bool CLanServer::Start(const wchar_t* ip, unsigned short port, int workerThreadCnt, int runningThreadCnt, bool nagleOff, int maxUser)
{
	if (workerThreadCnt == 0 || runningThreadCnt == 0 || maxUser == 0 ||
		port < 0 || port > 65535 || maxUser < 0 || maxUser > 65535)
		return false;

	InitSettings();
	_workerThreadCnt = workerThreadCnt;
	_nagleOff = nagleOff;
	_maxUser = maxUser;
	_monitorSwitch = true;

	// Prepare listen socket 
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (_listen_sock == INVALID_SOCKET)
		return false;

	SOCKADDR_IN sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	if (ip == NULL)
		sockaddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	else
		InetPtonW(AF_INET, ip, &sockaddr.sin_addr);

	int retval;

	retval = bind(_listen_sock, (SOCKADDR*)&sockaddr, sizeof(sockaddr));
	if (retval == SOCKET_ERROR)
	{
		closesocket(_listen_sock);
		return false;
	}

	retval = listen(_listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		closesocket(_listen_sock);
		return false;
	}

	// Create IOCP
	_hCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, runningThreadCnt);
	if (_hCP == NULL)
	{
		closesocket(_listen_sock);
		return false;
	}

	// Change server state
	_serverOn = true;

	// Create Session DataStructure
	_sessionArr = new SESSION[maxUser];
	int idx;
	for (idx = 0; idx < maxUser; idx++)
	{
		_sessionArr[idx].ioFlag.ioCount = 0;
		_freeSessionStack.Push(idx);
	}

	OnStart();

	// Create thread
	_hThreadArr = new HANDLE[workerThreadCnt];
	for (int i = 0; i < workerThreadCnt; i++)
		_hThreadArr[i] = (HANDLE)_beginthreadex(NULL, 0, RunIOCPWorkerThread, (LPVOID)this, NULL, NULL);
	_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, RunAcceptThread, (LPVOID)this, NULL, NULL);
	_hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, RunMonitorThread, (LPVOID)this, NULL, NULL);

	// Check Thread state
	bool isSafe = true;
	do
	{
		for (int i = 0; i < workerThreadCnt; i++)
		{
			if (_hThreadArr[i] == NULL)
				isSafe = false;
		}

		if (_hAcceptThread == NULL || _hMonitorThread == NULL)
			isSafe = false;

		if (isSafe)
			break;

		// Exception handling
		for (int i = 0; i < workerThreadCnt; i++)
		{
			if (_hThreadArr[i] != NULL)
				CloseHandle(_hThreadArr[i]);
		}

		if (_hAcceptThread != NULL)
			CloseHandle(_hAcceptThread);

		if (_hMonitorThread != NULL)
			CloseHandle(_hMonitorThread);

		_serverOn = false;
		closesocket(_listen_sock);
		CloseHandle(_hCP);
		delete[] _hThreadArr;
		delete[] _sessionArr;
		return false;
	} while (1);

	return true;
}

void CLanServer::Stop()
{
	if (!_serverOn)
		return;

	OnStop();

	/*

	Ŭ���̾�Ʈ ���Ͽ� ���� ���� ���� �κ� �߰� ����

	*/

	// Accept �ߴ�
	closesocket(_listen_sock);
	if (WaitForSingleObject(_hAcceptThread, 10000) != WAIT_OBJECT_0)
		TerminateThread(_hAcceptThread, 1);

	// IOCP WorkerThread �ߴ�
	int idx;
	for (idx = 0; idx < _workerThreadCnt; idx++)
		PostQueuedCompletionStatus(_hCP, NULL, NULL, NULL);
	if (WaitForMultipleObjects(_workerThreadCnt, _hThreadArr, TRUE, 20000) == WAIT_TIMEOUT)
	{
		for (idx = 0; idx < _workerThreadCnt; idx++)
		{
			DWORD exitCode;
			GetExitCodeThread(_hThreadArr[idx], &exitCode);
			if (exitCode == STILL_ACTIVE)
				TerminateThread(_hThreadArr[idx], 1);
		}
	}

	// Monitor Thread �ߴ�
	_monitorSwitch = false;
	if (WaitForSingleObject(_hMonitorThread, 10000) != WAIT_OBJECT_0)
		TerminateThread(_hMonitorThread, 1);

	// �����Ҵ� �޸� ����
	delete[] _hThreadArr;
	delete[] _sessionArr;

	// �ڵ� ��ȯ
	for (idx = 0; idx < _workerThreadCnt; idx++)
		CloseHandle(_hThreadArr[idx]);
	CloseHandle(_hAcceptThread);
	CloseHandle(_hMonitorThread);
	CloseHandle(_hCP);

	// ����� ��� ��� ���� �ʱ�ȭ
	InitSettings();
}

unsigned WINAPI CLanServer::RunMonitorThread(LPVOID lpParam)
{
	CLanServer* server = (CLanServer*)lpParam;
	server->MonitorThread();
	return 0;
}

void CLanServer::MonitorThread()
{
	while (_monitorSwitch)
	{
		_sendTPS = InterlockedExchange((LONG*)&_sendCnt, 0);
		_recvTPS = InterlockedExchange((LONG*)&_recvCnt, 0);
		_acceptTPS = InterlockedExchange((LONG*)&_acceptCnt, 0);
		Sleep(1000);
	}
}

unsigned WINAPI CLanServer::RunAcceptThread(LPVOID lpParam)
{
	CLanServer* server = (CLanServer*)lpParam;
	server->AcceptThread();
	return 0;
}

void CLanServer::AcceptThread()
{
	SOCKET listen_sock = _listen_sock;
	HANDLE hCP = _hCP;
	SESSION* sessionArr = _sessionArr;
	int bkMaxUser = _maxUser;
	bool nagleOff = _nagleOff;
	while (1)
	{
		SOCKADDR_IN clientAddr;
		int addrLen = sizeof(clientAddr);
		SOCKET client_sock = accept(listen_sock, (SOCKADDR*)&clientAddr, &addrLen);
		if (client_sock == SOCKET_ERROR)
		{
			DWORD err = WSAGetLastError();
			if (err == WSAEINTR)
				return;
			else
				return;
		}

		InterlockedIncrement((LONG*)&_acceptCnt);

		// Set socket option
		int bufsize = 0; // 100% overlapped processing (send only)
		setsockopt(client_sock, SOL_SOCKET, SO_SNDBUF, (const char*)&bufsize, sizeof(bufsize));

		LINGER linger = { 1, 0 }; // Prevent server connecting from remaining TIME_WAIT state
		setsockopt(client_sock, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));

		if (nagleOff) // Nagle algorithm
		{
			int ngOpt = TRUE;
			setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&ngOpt, sizeof(ngOpt));
		}

		// Check now user count
		if (_nowSession == bkMaxUser)
		{
			closesocket(client_sock);
			continue;
		}

		// Contents part connect request logic
		WCHAR ipBuf[16];
		InetNtopW(AF_INET, &clientAddr.sin_addr, ipBuf, 16);
		if (OnConnectionRequest(ipBuf, ntohs(clientAddr.sin_port)) == false)
		{
			closesocket(client_sock);
			continue;
		}

		InterlockedIncrement((PLONG)&_nowSession);

		// Set session count
		++_sessionCnt;
		if (_sessionCnt > 0xFFFFFFFFFFFF)
			_sessionCnt = 1;

		// Alloc session
		UINT idx;
		while (!_freeSessionStack.Pop(&idx));
		InterlockedIncrement((PULONG)&sessionArr[idx].ioFlag.ioCount);
		sessionArr[idx].socket = client_sock;
		sessionArr[idx].relSocket = client_sock;
		sessionArr[idx].sessionID = (_sessionCnt << 16) | (UINT64)idx;
		sessionArr[idx].ioFlag.releaseFlag = 0;
		sessionArr[idx].sendFlag = 0;
		sessionArr[idx].sendCnt = 0;
		//sessionArr[idx].sendQ.ClearBuffer();
		sessionArr[idx].recvQ.ClearBuffer();

		if (CreateIoCompletionPort((HANDLE)client_sock, hCP, (ULONG_PTR)&sessionArr[idx], NULL) == NULL)
			return;

		OnClientJoin(sessionArr[idx].sessionID);

		// Recv Part
		ZeroMemory(&_sessionArr[idx].recvOverlapped, sizeof(WSAOVERLAPPED));
		WSABUF rcvbuf;
		rcvbuf.buf = _sessionArr[idx].recvQ.GetRearBufferPtr();
		rcvbuf.len = _sessionArr[idx].recvQ.DirectEnqueueSize();

		DWORD flags = 0;
		int rcvret = WSARecv(client_sock, &rcvbuf, 1, NULL, &flags, &_sessionArr[idx].recvOverlapped, NULL);
		if (rcvret == SOCKET_ERROR)
		{
			DWORD err = WSAGetLastError();
			switch (err)
			{
			case WSA_IO_PENDING:
				if (_sessionArr[idx].socket == INVALID_SOCKET)
				{
					/*
					WSASend�� ���ڷ� INVALID_SOCKET�� �Ǳ� �� ���� ���� ��ũ���Ͱ� ���ڷ�
					����Ǿ� ���޵Ǿ� WSASend ȣ���� �Ǿ����� �� �� �ٸ� �������� Disconnect ȣ�� �ο���
					INVALID_SOCKET���� �����ϰ� CancelIOEx�� ȣ���� �� �ٽ� WSASend ���ο��� Overlapped IO��
					������ ��� �̹� IO�� ���ؼ��� CancelIOEx�� �� �� �� �ɾ���� �ϱ� ������ �߰��� �κ�
					*/
					CancelIoEx((HANDLE)_sessionArr[idx].relSocket, NULL);
				}
				break;
			case WSAENOTSOCK:
			case WSAECONNABORTED:
			case WSAECONNRESET:
				if (InterlockedDecrement((PULONG)&_sessionArr[idx].ioFlag.ioCount) == 0)
					ReleaseSession(&_sessionArr[idx]);
				break;
			default:
				printf("[Error] WSARecv error! [error code: %d]\n", err);
				if (InterlockedDecrement((PULONG)&_sessionArr[idx].ioFlag.ioCount) == 0)
					ReleaseSession(&_sessionArr[idx]);
				break;
			}
		}
		InterlockedIncrement((LONG*)&_recvCnt);
	}
}

unsigned WINAPI CLanServer::RunIOCPWorkerThread(LPVOID lpParam)
{
	CLanServer* server = (CLanServer*)lpParam;
	server->IOCPWorkerThread();
	return 0;
}

void CLanServer::IOCPWorkerThread()
{
	HANDLE hCP = _hCP;
	DWORD dwTransferred;
	SESSION* pSession;
	WSAOVERLAPPED* lpOverlapped;
	while (1)
	{
		lpOverlapped = NULL;
		pSession = NULL;
		dwTransferred = 0;
		GetQueuedCompletionStatus(hCP, &dwTransferred, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&lpOverlapped, INFINITE);

		// End Thread
		if (dwTransferred == 0 && pSession == nullptr && lpOverlapped == nullptr)
			break;

		if (dwTransferred != 0 && pSession->socket != INVALID_SOCKET)
		{
			if (lpOverlapped == NULL)
			{
				switch (dwTransferred)
				{
				case en_IOCP_JOB_SENDPOST:
					SendPost(pSession);
					break;
				case en_IOCP_JOB_RELEASE:
					ReleaseSession(pSession);
					continue;
				default:
					break;
				}
			}
			else if (&pSession->sendOverlapped == lpOverlapped) // Send Logic
			{
				// Send Queue ���� �۾�
				UINT idx;
				UINT bkSendCnt = pSession->sendCnt;
				pSession->sendCnt = 0;
				for (idx = 0; idx < bkSendCnt; idx++)
					CLanPacket::Free(pSession->sendPacketArr[idx]);

				InterlockedExchange((LONG*)&pSession->sendFlag, 0);
				SendPost(pSession);
			}
			else // Recv Logic
			{
				int retSize = pSession->recvQ.MoveRear(dwTransferred);
				if (retSize != dwTransferred)
					CrashDump::Crash();

				CLanPacket::NetHeader header;
				CLanPacket* pPacket;
				while (1)
				{
					if (pSession->recvQ.GetUseSize() < sizeof(CLanPacket::NetHeader))
						break;

					pSession->recvQ.Peek((char*)&header, sizeof(CLanPacket::NetHeader));
					
					if (pSession->recvQ.GetUseSize() < sizeof(CLanPacket::NetHeader) + header.len)
						break;

					pPacket = CLanPacket::Alloc();
					int deqSize = pSession->recvQ.Dequeue(pPacket->GetBufferPtr(), sizeof(CLanPacket::NetHeader) + header.len);
					if (deqSize != sizeof(CLanPacket::NetHeader) + header.len)	// �ӽ�, ���� ����
						CrashDump::Crash();

					pPacket->MoveWritePos(header.len);
					InterlockedIncrement((LONG*)&_recvCnt);

					// ������ ����
					OnRecv(pSession->sessionID, pPacket);
					CLanPacket::Free(pPacket);
				}
				RecvPost(pSession);
			}
		}

		if (InterlockedDecrement((PULONG)&pSession->ioFlag.ioCount) == 0)
			ReleaseSession(pSession);
	}
}

void CLanServer::SendPost(SESSION* pSession)
{
	int cnt = 0;
	int useSize;

	while (1)
	{
		if (pSession->sendQ.GetUseSize() == 0)
			return;

		if (InterlockedExchange((PULONG)&pSession->sendFlag, 1) == 1)
			return;

		useSize = pSession->sendQ.GetUseSize();
		if (useSize == 0)
			pSession->sendFlag = 0;
		else
			break;

		++cnt;

		if (cnt == 2)
			return;
	}

	WSABUF sendbuf[MAXBUF];
	DWORD flags = 0;
	ZeroMemory(&pSession->sendOverlapped, sizeof(WSAOVERLAPPED));

	if (useSize > MAXBUF)
		useSize = MAXBUF;

	for (int i = 0; i < useSize; i++)
	{
		pSession->sendQ.Dequeue(&pSession->sendPacketArr[i]);
		sendbuf[i].buf = pSession->sendPacketArr[i]->GetBufferPtr();
		sendbuf[i].len = pSession->sendPacketArr[i]->GetTotalSize();
	}
	pSession->sendCnt = useSize;

	InterlockedIncrement((PULONG)&pSession->ioFlag.ioCount);
	if (WSASend(pSession->socket, sendbuf, useSize, NULL, flags, &pSession->sendOverlapped, NULL) == SOCKET_ERROR)
	{
		DWORD err = WSAGetLastError();
		switch (err)
		{
		case WSA_IO_PENDING:
			if (pSession->socket == INVALID_SOCKET)
			{
				/*
				WSASend�� ���ڷ� INVALID_SOCKET�� �Ǳ� �� ���� ���� ��ũ���Ͱ� ���ڷ�
				����Ǿ� ���޵Ǿ� WSASend ȣ���� �Ǿ����� �� �� �ٸ� �������� Disconnect ȣ�� �ο���
				INVALID_SOCKET���� �����ϰ� CancelIOEx�� ȣ���� �� �ٽ� WSASend ���ο��� Overlapped IO��
				������ ��� �̹� IO�� ���ؼ��� CancelIOEx�� �� �� �� �ɾ���� �ϱ� ������ �߰��� �κ�
				*/
				CancelIoEx((HANDLE)pSession->relSocket, NULL);
			}
			break;
		case WSAENOTSOCK:
		case WSAECONNABORTED:
		case WSAECONNRESET:
			InterlockedDecrement((PULONG)&pSession->ioFlag.ioCount);
			break;
		default:
			// printf("[Error] WSASend error! [error code: %d]\n", err);
			InterlockedDecrement((PULONG)&pSession->ioFlag.ioCount);
			break;
		}
	}
}

void CLanServer::RecvPost(SESSION* pSession)
{
	WSABUF recvbuf[2];
	DWORD flags = 0;
	ZeroMemory(&pSession->recvOverlapped, sizeof(WSAOVERLAPPED));
	int freeSize = pSession->recvQ.GetFreeSize();
	recvbuf[0].buf = pSession->recvQ.GetRearBufferPtr();
	recvbuf[0].len = (ULONG)pSession->recvQ.DirectEnqueueSize();

	InterlockedIncrement((PULONG)&pSession->ioFlag.ioCount);
	if (freeSize <= recvbuf[0].len)
	{
		if (WSARecv(pSession->socket, recvbuf, 1, NULL, &flags, &pSession->recvOverlapped, NULL) == SOCKET_ERROR)
		{
			DWORD err = WSAGetLastError();
			switch (err)
			{
			case WSA_IO_PENDING:
				if (pSession->socket == INVALID_SOCKET)
				{
					/*
					WSASend�� ���ڷ� INVALID_SOCKET�� �Ǳ� �� ���� ���� ��ũ���Ͱ� ���ڷ�
					����Ǿ� ���޵Ǿ� WSASend ȣ���� �Ǿ����� �� �� �ٸ� �������� Disconnect ȣ�� �ο���
					INVALID_SOCKET���� �����ϰ� CancelIOEx�� ȣ���� �� �ٽ� WSASend ���ο��� Overlapped IO��
					������ ��� �̹� IO�� ���ؼ��� CancelIOEx�� �� �� �� �ɾ���� �ϱ� ������ �߰��� �κ�
					*/
					CancelIoEx((HANDLE)pSession->relSocket, NULL);
				}
				break;
			case WSAENOTSOCK:
			case WSAECONNABORTED:
			case WSAECONNRESET:
				InterlockedDecrement((PULONG)&pSession->ioFlag.ioCount);
				break;
			default:
				// printf("[Error] WSARecv error! [error code: %d]\n", err); // ���� �����ڵ� �Լ� ȣ��
				InterlockedDecrement((PULONG)&pSession->ioFlag.ioCount);
				break;
			}
		}
	}
	else
	{
		recvbuf[1].buf = pSession->recvQ.GetBufferPtr();
		recvbuf[1].len = (ULONG)(pSession->recvQ.GetFrontBufferPtr() - pSession->recvQ.GetBufferPtr() - 1);
		if (WSARecv(pSession->socket, recvbuf, 2, NULL, &flags, &pSession->recvOverlapped, NULL) == SOCKET_ERROR)
		{
			DWORD err = WSAGetLastError();
			switch (err)
			{
			case WSA_IO_PENDING:
				if (pSession->socket == INVALID_SOCKET)
				{
					/*
					WSASend�� ���ڷ� INVALID_SOCKET�� �Ǳ� �� ���� ���� ��ũ���Ͱ� ���ڷ�
					����Ǿ� ���޵Ǿ� WSASend ȣ���� �Ǿ����� �� �� �ٸ� �������� Disconnect ȣ�� �ο���
					INVALID_SOCKET���� �����ϰ� CancelIOEx�� ȣ���� �� �ٽ� WSASend ���ο��� Overlapped IO��
					������ ��� �̹� IO�� ���ؼ��� CancelIOEx�� �� �� �� �ɾ���� �ϱ� ������ �߰��� �κ�
					*/
					CancelIoEx((HANDLE)pSession->relSocket, NULL);
				}
				break;
			case WSAENOTSOCK:
			case WSAECONNABORTED:
			case WSAECONNRESET:
				InterlockedDecrement((PULONG)&pSession->ioFlag.ioCount);
				break;
			default:
				// printf("[Error] WSARecv error! [error code: %d]\n", err); // ���� �����ڵ� �Լ� ȣ��
				InterlockedDecrement((PULONG)&pSession->ioFlag.ioCount);
				break;
			}
		}
	}
}

bool CLanServer::SendPacket(unsigned __int64 sessionID, CLanPacket* pPacket)
{
	SESSION* pSession = &_sessionArr[(USHORT)sessionID];

	if (!EnterSession(pSession, sessionID))
		return false;

	InterlockedIncrement((LONG*)&_sendCnt);
	pPacket->SetLanHeader();
	/*
		Q. Reference count �δ� ���� ?
		A. Broadcast ���� ����
	*/
	pPacket->AddRefCount();

	pSession->sendQ.Enqueue(pPacket);

	if (!PostQueuedCompletionStatus(_hCP, en_IOCP_JOB_SENDPOST, (ULONG_PTR)pSession, NULL))
		LeaveSession(pSession);

	return true;
}

void CLanServer::RequestRelease(SESSION* pSession)
{
	PostQueuedCompletionStatus(_hCP, en_IOCP_JOB_RELEASE, (ULONG_PTR)pSession, NULL);
}

void CLanServer::ReleaseSession(SESSION* pSession)
{
	if (InterlockedCompareExchange64((PLONG64)&pSession->ioFlag, 0x100000000, 0) != 0)
		return;

	int retval;
	CLanPacket* pPacket;
	while (1)
	{
		if (pSession->sendQ.Dequeue(&pPacket) == false)
			break;

		CLanPacket::Free(pPacket);
	}

	UINT bkCnt = pSession->sendCnt;
	for (int i = 0; i < bkCnt; i++)
		CLanPacket::Free(pSession->sendPacketArr[i]);

	closesocket(pSession->relSocket);
	OnClientLeave(pSession->sessionID);
	_freeSessionStack.Push((USHORT)pSession->sessionID);
	InterlockedDecrement((LONG*)&_nowSession);
}

bool CLanServer::Disconnect(unsigned __int64 sessionID)
{
	SESSION* pSession = &_sessionArr[(USHORT)sessionID];

	if (!EnterSession(pSession, sessionID))
		return false;

	if (InterlockedExchange((PLONG)&pSession->socket, INVALID_SOCKET) == INVALID_SOCKET) // �߰� I/O ����
	{
		LeaveSession(pSession);
		return false;
	}

	CancelIoEx((HANDLE)pSession->relSocket, NULL);
	LeaveSession(pSession);
	return true;
}

bool CLanServer::EnterSession(SESSION* pSession, unsigned __int64 sessionID)
{
	InterlockedIncrement((PULONG)&pSession->ioFlag.ioCount);
	if (pSession->ioFlag.releaseFlag == 1 || pSession->sessionID != sessionID)
	{
		LeaveSession(pSession);
		return false;
	}
	return true;
}

void CLanServer::LeaveSession(SESSION* pSession)
{
	if (InterlockedDecrement((PULONG)&pSession->ioFlag.ioCount) == 0)
		RequestRelease(pSession);
}

void CLanServer::InitSettings()
{
	_listen_sock = INVALID_SOCKET;
	_maxUser = 0;
	_nowSession = 0;
	_sessionCnt = 0;
	_nagleOff = false;
	_sessionArr = nullptr;
	_hAcceptThread = NULL;
	_hMonitorThread = NULL;
	_hThreadArr = nullptr;
	_hCP = NULL;
	_acceptCnt = 0;
	_sendCnt = 0;
	_recvCnt = 0;
	_acceptTPS = 0;
	_sendTPS = 0;
	_recvTPS = 0;
	_workerThreadCnt = 0;
	_monitorSwitch = false;
	_serverOn = false;
}

int CLanServer::getAcceptTPS()
{
	return _acceptTPS;
}

int CLanServer::getRecvMessageTPS()
{
	return _recvTPS;
}

int CLanServer::getSendMessageTPS()
{
	return _sendTPS;
}

int CLanServer::GetSessionCount()
{
	return _nowSession;
}
