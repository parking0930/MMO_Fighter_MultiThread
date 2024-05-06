#pragma once
#include <unordered_map>
#include <list>
#include "CLanServer.h"
#include "ContentsDefine.h"

using namespace std;

class Contents :public CLanServer
{
public:
	struct SECTOR_POS
	{
		int _x;
		int _y;
	};

	struct SECTOR_AROUND
	{
		int cnt;
		SECTOR_POS around[14];
	};

	struct PLAYER
	{
		struct ACTION
		{
			bool			_isMove;
			unsigned char	_moveDirection;
		};
		unsigned __int64	_sessionID;
		unsigned int		_playerID;
		char				_hp;
		unsigned short		_x;
		unsigned short		_y;
		unsigned char		_direction;
		ACTION				_action;
		SECTOR_POS			_sector;
		bool				_isDisconnected;
		ULONGLONG			_lastRecvTime;
		SRWLOCK				_playerLock;
		//CRITICAL_SECTION	_playerLock;
		ULONGLONG			_owingThread;
	};
public:
	Contents();
	~Contents();
	virtual void OnStart();
	bool OnConnectionRequest(wchar_t* ip, unsigned short port);
	void OnClientJoin(unsigned __int64 sessionID);
	void OnClientLeave(unsigned __int64 sessionID);
	void OnRecv(unsigned __int64 sessionID, CLanPacket* pPacket);
	void OnError(int errorcode, wchar_t* str);
	void SendAround(SECTOR_AROUND* pSectorAround, CLanPacket* pPacket, UINT64 exceptSessionID);
public:
	void GetSectorAround(int sectorX, int sectorY, SECTOR_AROUND* pSectorAround);
	void AcquireLockAround(SECTOR_AROUND* pSectorAround);
	void ReleaseLockAround(SECTOR_AROUND* pSectorAround);
	void CreatePlayer(unsigned __int64 sessionID);
	void DeletePlayer(unsigned __int64 sessionID);
	void SetPlayerDirection(PLAYER* pPlayer, BYTE moveDirection);
	bool Sector_CheckAndUpdate(PLAYER* pPlayer);
	void GetUpdateSectorAround(SECTOR_AROUND* pOldAround, SECTOR_AROUND* pCurAround, SECTOR_AROUND* pRemoveAround, SECTOR_AROUND* pAddAround);
	void IntegrateSectorAround(SECTOR_AROUND* pResult, SECTOR_AROUND* pAround1, SECTOR_AROUND* pAround2);
	void CollisionDetection(PLAYER* pPlayer, BYTE type);
	PLAYER* FindAndLockPlayer(unsigned __int64 sessionID);
	void	ReleasePlayer(PLAYER* pPlayer);
	bool	PlayerMoveBoundaryCheck(unsigned short x, unsigned short y);
public:
	//Send
	void CreateMyCharacterSC(unsigned __int64 sessionID, UINT id, BYTE direction, u_short x, u_short y, BYTE hp);
	void CreateOtherCharacterSC(unsigned __int64 sessionID, UINT id, BYTE direction, u_short x, u_short y, BYTE hp);
	void MoveStartSC(unsigned __int64 sessionID, UINT id, BYTE direction, u_short x, u_short y);
public:
	// Recv handler
	void HandleMoveStartCS(unsigned __int64 sessionID, CLanPacket* pPacket);
	void HandleMoveStopCS(unsigned __int64 sessionID, CLanPacket* pPacket);
	void HandleEchoCS(unsigned __int64 sessionID, CLanPacket* pPacket);
	void HandleAttack1CS(unsigned __int64 sessionID, CLanPacket* pPacket);
	void HandleAttack2CS(unsigned __int64 sessionID, CLanPacket* pPacket);
	void HandleAttack3CS(unsigned __int64 sessionID, CLanPacket* pPacket);
private:
	void Update();
	void RecvHandler(unsigned __int64 sessionID, CLanPacket* pPacket);
	static unsigned WINAPI LaunchUpdate(LPVOID lpParam);
private:
	static MemoryPoolTLS<PLAYER> playerPool;
	list<PLAYER*>	_sector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
	SRWLOCK			_sectorLock[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
	unordered_map<UINT64, PLAYER*> _playerMap;
	SRWLOCK			_playerMapLock;
	unsigned int	_playerCnt;
	HANDLE _hUpdateThread;
};