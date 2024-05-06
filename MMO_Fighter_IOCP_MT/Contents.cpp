#pragma comment(lib, "Winmm.lib")
#include <process.h>
#include <algorithm>
#include "Contents.h"
#include "Protocol.h"

MemoryPoolTLS<Contents::PLAYER> Contents::playerPool(10000, POOL_MAX_ALLOC, true);

Contents::Contents() :_hUpdateThread(NULL), _playerCnt(0)
{
	for (int y = 0; y < dfSECTOR_MAX_Y; y++)
	{
		for (int x = 0; x < dfSECTOR_MAX_X; x++)
		{
			InitializeSRWLock(&_sectorLock[y][x]);
		}
	}
	InitializeSRWLock(&_playerMapLock);
	srand(time(NULL));
}

Contents::~Contents()
{
#pragma warning(disable:6258)
	if (WaitForSingleObject(_hUpdateThread, 2000) == WAIT_TIMEOUT)
		TerminateThread(_hUpdateThread, 1);

	// Character 할당 반환 및 Map 삭제

}

unsigned WINAPI Contents::LaunchUpdate(LPVOID lpParam)
{
	Contents* server = (Contents*)lpParam;
	server->Update();
	return 0;
}

void Contents::Update()
{
	PLAYER* pPlayer;
	_playerCnt = 0;
	DWORD stTime = timeGetTime();
	DWORD elapsedTime = 0;
	DWORD fpsCnt = 0;
	DWORD fpsElapsedTime = 0;
	while (1)
	{
		//////////////////////////////////////////////////////
		/*             업데이트  처리 타이밍 계산             */
		DWORD nowTime = timeGetTime();		// 추후 64비트 처리
		elapsedTime += nowTime - stTime;
		fpsElapsedTime += nowTime - stTime;
		stTime = nowTime;
		if (elapsedTime < 20)				// 50fps setting
		{
			Sleep(20 - elapsedTime);
			continue;
		}
		++fpsCnt;
		elapsedTime -= 20;
		if (fpsElapsedTime >= 1000)
		{
			printf("Contents Frame : %d\n", fpsCnt);
			printf("Session Count: %d\nUser Count: %lld\n", GetSessionCount(), _playerMap.size());
			printf("Accept TPS: %d \n", getAcceptTPS());;
			printf("Send TPS: %d\n", getSendMessageTPS());;
			printf("Recv TPS: %d\n", getRecvMessageTPS());;
			printf("-------------------------------------------------------\n");
			fpsCnt = 0;
			fpsElapsedTime -= 1000;
		}
		//////////////////////////////////////////////////////
		ULONGLONG ulCurTick = GetTickCount64();
		AcquireSRWLockExclusive(&_playerMapLock);
		//InterlockedExchange(&_owingThread, (ULONGLONG)GetCurrentThreadId());
		auto it = _playerMap.begin();
		while (it != _playerMap.end())
		{
			pPlayer = it->second;
			AcquireSRWLockExclusive(&pPlayer->_playerLock);
			InterlockedExchange(&pPlayer->_owingThread, (ULONGLONG)GetCurrentThreadId());
			++it;
			// 사망 여부 판단
			if (pPlayer->_hp <= 0)
			{
				Disconnect(pPlayer->_sessionID);
				ReleaseSRWLockExclusive(&pPlayer->_playerLock);
				continue;
			}
			else
			{
				// 일정 시간 수신 없으면 종료 처리
				if (ulCurTick - pPlayer->_lastRecvTime > dfNETWORK_PACKET_RECV_TIMEOUT)
				{
					Disconnect(pPlayer->_sessionID);
					ReleaseSRWLockExclusive(&pPlayer->_playerLock);
					continue;
				}

				if (pPlayer->_action._isMove)
				{
					switch (pPlayer->_action._moveDirection)
					{
					case dfPACKET_MOVE_DIR_LL:
						if (PlayerMoveBoundaryCheck(pPlayer->_x - dfSPEED_PLAYER_X, pPlayer->_y))
						{
							pPlayer->_x -= dfSPEED_PLAYER_X;
						}
						break;
					case dfPACKET_MOVE_DIR_LU:
						if (PlayerMoveBoundaryCheck(pPlayer->_x - dfSPEED_PLAYER_X, pPlayer->_y - dfSPEED_PLAYER_Y))
						{
							pPlayer->_x -= dfSPEED_PLAYER_X;
							pPlayer->_y -= dfSPEED_PLAYER_Y;
						}
						break;
					case dfPACKET_MOVE_DIR_UU:
						if (PlayerMoveBoundaryCheck(pPlayer->_x, pPlayer->_y - dfSPEED_PLAYER_Y))
						{
							pPlayer->_y -= dfSPEED_PLAYER_Y;
						}
						break;
					case dfPACKET_MOVE_DIR_RU:
						if (PlayerMoveBoundaryCheck(pPlayer->_x + dfSPEED_PLAYER_X, pPlayer->_y - dfSPEED_PLAYER_Y))
						{
							pPlayer->_x += dfSPEED_PLAYER_X;
							pPlayer->_y -= dfSPEED_PLAYER_Y;
						}
						break;
					case dfPACKET_MOVE_DIR_RR:
						if (PlayerMoveBoundaryCheck(pPlayer->_x + dfSPEED_PLAYER_X, pPlayer->_y))
						{
							pPlayer->_x += dfSPEED_PLAYER_X;
						}
						break;
					case dfPACKET_MOVE_DIR_RD:
						if (PlayerMoveBoundaryCheck(pPlayer->_x + dfSPEED_PLAYER_X, pPlayer->_y + dfSPEED_PLAYER_Y))
						{
							pPlayer->_x += dfSPEED_PLAYER_X;
							pPlayer->_y += dfSPEED_PLAYER_Y;
						}
						break;
					case dfPACKET_MOVE_DIR_DD:
						if (PlayerMoveBoundaryCheck(pPlayer->_x, pPlayer->_y + dfSPEED_PLAYER_Y))
						{
							pPlayer->_y += dfSPEED_PLAYER_Y;
						}
						break;
					case dfPACKET_MOVE_DIR_LD:
						if (PlayerMoveBoundaryCheck(pPlayer->_x - dfSPEED_PLAYER_X, pPlayer->_y + dfSPEED_PLAYER_Y))
						{
							pPlayer->_x -= dfSPEED_PLAYER_X;
							pPlayer->_y += dfSPEED_PLAYER_Y;
						}
						break;
					}
					// 섹터 이동 시
					Sector_CheckAndUpdate(pPlayer);
				}
			}
			ReleaseSRWLockExclusive(&pPlayer->_playerLock);
		}
		ReleaseSRWLockExclusive(&_playerMapLock);
	}
}

bool Contents::PlayerMoveBoundaryCheck(unsigned short x, unsigned short y)
{
	if (x >= dfRANGE_MOVE_LEFT && x < dfRANGE_MOVE_RIGHT
		&& y >= dfRANGE_MOVE_TOP && y < dfRANGE_MOVE_BOTTOM)
		return true;
	else
		return false;
}


void Contents::OnStart()
{
	_hUpdateThread = (HANDLE)_beginthreadex(NULL, 0, LaunchUpdate, (LPVOID)this, NULL, NULL);
}

bool Contents::OnConnectionRequest(wchar_t* ip, unsigned short port) { return true; }

void Contents::OnClientJoin(unsigned __int64 sessionID)
{
	CreatePlayer(sessionID);
}

void Contents::OnClientLeave(unsigned __int64 sessionID)
{
	DeletePlayer(sessionID);
}

void Contents::OnRecv(unsigned __int64 sessionID, CLanPacket* pPacket)
{
	RecvHandler(sessionID, pPacket);
}

void Contents::OnError(int errorcode, wchar_t* str) {}

void Contents::SendAround(SECTOR_AROUND* pSectorAround, CLanPacket* pPacket, UINT64 exceptSessionID)
{
	int i, x, y;
	PLAYER* pOtherPlayer;
	list<PLAYER*>* pSectorList;
	int cnt = pSectorAround->cnt;
	for (i = 0; i < cnt; i++)
	{
		x = pSectorAround->around[i]._x;
		y = pSectorAround->around[i]._y;
		pSectorList = &_sector[y][x];
		auto it = pSectorList->begin();
		while (it != pSectorList->end())
		{
			pOtherPlayer = (*it);
			++it;
			if (pOtherPlayer->_sessionID != exceptSessionID)
			{
				SendPacket(pOtherPlayer->_sessionID, pPacket);
			}
		}
	}
}

void Contents::RecvHandler(unsigned __int64 sessionID, CLanPacket* pPacket)
{
	u_short type;
	*pPacket >> type;
	switch (type)
	{
	case dfPACKET_CS_MOVE_START:
		HandleMoveStartCS(sessionID, pPacket);
		break;
	case dfPACKET_CS_MOVE_STOP:
		HandleMoveStopCS(sessionID, pPacket);
		break;
	case dfPACKET_CS_ATTACK1:
		HandleAttack1CS(sessionID, pPacket);
		break;
	case dfPACKET_CS_ATTACK2:
		HandleAttack2CS(sessionID, pPacket);
		break;
	case dfPACKET_CS_ATTACK3:
		HandleAttack3CS(sessionID, pPacket);
		break;
	case dfPACKET_CS_ECHO:
		HandleEchoCS(sessionID, pPacket);
		break;
	default:
		Disconnect(sessionID);
		break;
	}
}

////////////////////////////////////////////////////////////////

void Contents::GetSectorAround(int sectorX, int sectorY, SECTOR_AROUND* pSectorAround)
{
	--sectorX;
	--sectorY;
	pSectorAround->cnt = 0;

	int cntX, cntY;
	for (cntY = 0; cntY < 3; cntY++)
	{
		if (sectorY + cntY < 0 || sectorY + cntY >= dfSECTOR_MAX_Y)
		{
			continue;
		}

		for (cntX = 0; cntX < 3; cntX++)
		{
			if (sectorX + cntX < 0 || sectorX + cntX >= dfSECTOR_MAX_X)
			{
				continue;
			}
			pSectorAround->around[pSectorAround->cnt]._x = sectorX + cntX;
			pSectorAround->around[pSectorAround->cnt]._y = sectorY + cntY;
			pSectorAround->cnt++;
		}
	}
}

void Contents::AcquireLockAround(SECTOR_AROUND* pSectorAround)
{
	int i, x, y;
	int cnt = pSectorAround->cnt;
	for (i = 0; i < cnt; i++)
	{
		x = pSectorAround->around[i]._x;
		y = pSectorAround->around[i]._y;
		AcquireSRWLockExclusive(&_sectorLock[y][x]);
	}
}

void Contents::ReleaseLockAround(SECTOR_AROUND* pSectorAround)
{
	int i, x, y;
	int cnt = pSectorAround->cnt;
	for (i = 0; i < cnt; i++)
	{
		x = pSectorAround->around[i]._x;
		y = pSectorAround->around[i]._y;
		ReleaseSRWLockExclusive(&_sectorLock[y][x]);
	}
}

void Contents::SetPlayerDirection(Contents::PLAYER* pPlayer, BYTE moveDirection)
{
	switch (moveDirection)
	{
	case dfPACKET_MOVE_DIR_RR:
	case dfPACKET_MOVE_DIR_RU:
	case dfPACKET_MOVE_DIR_RD:
		pPlayer->_direction = dfPACKET_MOVE_DIR_RR;
		break;
	case dfPACKET_MOVE_DIR_LL:
	case dfPACKET_MOVE_DIR_LU:
	case dfPACKET_MOVE_DIR_LD:
		pPlayer->_direction = dfPACKET_MOVE_DIR_LL;
		break;
	}
}

void Contents::IntegrateSectorAround(SECTOR_AROUND* pResult, SECTOR_AROUND* pAround1, SECTOR_AROUND* pAround2)
{
	// 섹터 통합 및 정렬
	pResult->cnt = 0;
	int i, j;
	vector<pair<int, int>> v;

	for (i = 0; i < pAround1->cnt; i++)
	{
		v.push_back({ pAround1->around[i]._y,pAround1->around[i]._x });
	}
	for (i = 0; i < pAround2->cnt; i++)
	{
		v.push_back({ pAround2->around[i]._y,pAround2->around[i]._x });
	}
	sort(v.begin(), v.end());
	v.erase(unique(v.begin(), v.end()), v.end());
	
	auto it = v.begin();
	while (it != v.end())
	{
		pResult->around[pResult->cnt]._y = it->first;
		pResult->around[pResult->cnt]._x = it->second;
		pResult->cnt++;
		++it;
	}
}

bool Contents::Sector_CheckAndUpdate(Contents::PLAYER* pPlayer)
{
	SECTOR_POS curPos = { pPlayer->_x / SECTOR_RATE_X,pPlayer->_y / SECTOR_RATE_Y };
	if (curPos._x == pPlayer->_sector._x && curPos._y == pPlayer->_sector._y)
		return false;

	int cnt;
	list<PLAYER*>* pSectorList;
	SECTOR_POS oldPos = { pPlayer->_sector._x,pPlayer->_sector._y };
	pPlayer->_sector = curPos;

	SECTOR_AROUND oldAround, curAround;
	SECTOR_AROUND removeAround, addAround;
	GetSectorAround(oldPos._x, oldPos._y, &oldAround);
	GetSectorAround(curPos._x, curPos._y, &curAround);
	// 이전 주변 섹터와 현재 주변 섹터를 비교해 삭제되는 섹터와 추가되는 섹터를 구함
	GetUpdateSectorAround(&oldAround, &curAround, &removeAround, &addAround);

	// Lock 범위를 구함
	SECTOR_AROUND lockAround;
	IntegrateSectorAround(&lockAround, &oldAround, &curAround);
	AcquireLockAround(&lockAround);

	// removeSector에 캐릭터 삭제 패킷 송신
	CLanPacket* pRemovePacket = CLanPacket::Alloc();
	*pRemovePacket << (u_short)dfPACKET_SC_DELETE_CHARACTER;
	*pRemovePacket << pPlayer->_playerID;
	SendAround(&removeAround, pRemovePacket, NULL);
	CLanPacket::Free(pRemovePacket);

	// 현재 플레이어(움직이고 있음)에게 RemoveSector에 있는 캐릭터들 삭제 패킷 송신
	for (cnt = 0; cnt < removeAround.cnt; cnt++)
	{
		pSectorList = &_sector[removeAround.around[cnt]._y][removeAround.around[cnt]._x];

		auto it = pSectorList->begin();
		while (it != pSectorList->end())
		{
			pRemovePacket = CLanPacket::Alloc();
			*pRemovePacket << (u_short)dfPACKET_SC_DELETE_CHARACTER;
			*pRemovePacket << (*it)->_playerID;
			SendPacket(pPlayer->_sessionID, pRemovePacket);
			CLanPacket::Free(pRemovePacket);
			++it;
		}
	}

	// addSector에 캐릭터 생성 패킷 송신
	CLanPacket* pAddPacket = CLanPacket::Alloc();
	*pAddPacket << (u_short)dfPACKET_SC_CREATE_OTHER_CHARACTER;
	*pAddPacket << pPlayer->_playerID;
	*pAddPacket << pPlayer->_direction;
	*pAddPacket << pPlayer->_x;
	*pAddPacket << pPlayer->_y;
	*pAddPacket << pPlayer->_hp;
	SendAround(&addAround, pAddPacket, NULL);
	CLanPacket::Free(pAddPacket);

	// addSector에 생성된 캐릭터 이동 패킷 송신
	CLanPacket* pMovePacket = CLanPacket::Alloc();
	*pMovePacket << (u_short)dfPACKET_SC_MOVE_START;
	*pMovePacket << pPlayer->_playerID;
	*pMovePacket << pPlayer->_action._moveDirection;
	*pMovePacket << pPlayer->_x;
	*pMovePacket << pPlayer->_y;
	SendAround(&addAround, pMovePacket, NULL);
	CLanPacket::Free(pMovePacket);

	// 현재 플레이어(움직이고 있음)에게 addSector에 있는 캐릭터들 생성 패킷 송신
	PLAYER* pExistPlayer;
	for (cnt = 0; cnt < addAround.cnt; cnt++)
	{
		pSectorList = &_sector[addAround.around[cnt]._y][addAround.around[cnt]._x];
		auto it = pSectorList->begin();
		while (it != pSectorList->end())
		{
			pExistPlayer = *it;
			CLanPacket* pCreatePacket = CLanPacket::Alloc();
			*pCreatePacket << (u_short)dfPACKET_SC_CREATE_OTHER_CHARACTER;
			*pCreatePacket << pExistPlayer->_playerID;
			*pCreatePacket << pExistPlayer->_direction;
			*pCreatePacket << pExistPlayer->_x;
			*pCreatePacket << pExistPlayer->_y;
			*pCreatePacket << pExistPlayer->_hp;
			SendPacket(pPlayer->_sessionID, pCreatePacket);
			CLanPacket::Free(pCreatePacket);

			if (pExistPlayer->_action._isMove)
			{
				pMovePacket = CLanPacket::Alloc();
				*pMovePacket << (u_short)dfPACKET_SC_MOVE_START;
				*pMovePacket << pExistPlayer->_playerID;
				*pMovePacket << pExistPlayer->_action._moveDirection;
				*pMovePacket << pExistPlayer->_x;
				*pMovePacket << pExistPlayer->_y;
				SendPacket(pPlayer->_sessionID, pMovePacket);
				CLanPacket::Free(pMovePacket);
			}
			++it;
		}
	}

	// 플레이어 sector 이동
	_sector[oldPos._y][oldPos._x].remove(pPlayer);
	_sector[curPos._y][curPos._x].push_back(pPlayer);
	ReleaseLockAround(&lockAround);
	return true;
}

void Contents::GetUpdateSectorAround(SECTOR_AROUND* pOldAround, SECTOR_AROUND* pCurAround, SECTOR_AROUND* pRemoveAround, SECTOR_AROUND* pAddAround)
{
	bool bFind;
	int iCntOld, iCntCur;
	pRemoveAround->cnt = pAddAround->cnt = 0;
	for (iCntOld = 0; iCntOld < pOldAround->cnt; iCntOld++)
	{
		bFind = false;
		for (iCntCur = 0; iCntCur < pCurAround->cnt; iCntCur++)
		{
			if (pOldAround->around[iCntOld]._x == pCurAround->around[iCntCur]._x &&
				pOldAround->around[iCntOld]._y == pCurAround->around[iCntCur]._y)
			{
				bFind = true;
				break;
			}
		}
		if (!bFind)
		{
			pRemoveAround->around[pRemoveAround->cnt] = pOldAround->around[iCntOld];
			pRemoveAround->cnt++;
		}
	}

	for (iCntCur = 0; iCntCur < pCurAround->cnt; iCntCur++)
	{
		bFind = false;
		for (iCntOld = 0; iCntOld < pOldAround->cnt; iCntOld++)
		{
			if (pOldAround->around[iCntOld]._x == pCurAround->around[iCntCur]._x &&
				pOldAround->around[iCntOld]._y == pCurAround->around[iCntCur]._y)
			{
				bFind = true;
				break;
			}
		}
		if (!bFind)
		{
			pAddAround->around[pAddAround->cnt] = pCurAround->around[iCntCur];
			pAddAround->cnt++;
		}
	}
}

void Contents::CreatePlayer(unsigned __int64 sessionID)
{
	PLAYER* pPlayer = playerPool.Alloc();
	pPlayer->_sessionID = sessionID;
	pPlayer->_playerID = ++_playerCnt;
	pPlayer->_hp = 100;
	//pPlayer->_x = 100;
	//pPlayer->_y = 100;
	pPlayer->_x = rand() % dfRANGE_MOVE_RIGHT;
	pPlayer->_y = rand() % dfRANGE_MOVE_BOTTOM;
	pPlayer->_direction = dfPACKET_MOVE_DIR_LL;
	pPlayer->_action._moveDirection = 0xFF;
	pPlayer->_action._isMove = false;
	pPlayer->_sector = { pPlayer->_x / SECTOR_RATE_X,pPlayer->_y / SECTOR_RATE_Y };
	pPlayer->_isDisconnected = false;
	InitializeSRWLock(&pPlayer->_playerLock);
	//InitializeCriticalSection(&pPlayer->_playerLock);
	pPlayer->_lastRecvTime = GetTickCount64();
	

	AcquireSRWLockExclusive(&_playerMapLock);
	_playerMap.insert(make_pair(sessionID, pPlayer));
	AcquireSRWLockExclusive(&pPlayer->_playerLock);
	InterlockedExchange(&pPlayer->_owingThread, (ULONGLONG)GetCurrentThreadId());
	ReleaseSRWLockExclusive(&_playerMapLock);

	int sectorX = pPlayer->_sector._x;
	int sectorY = pPlayer->_sector._y;

	// 생성 정보 보내주기
	CreateMyCharacterSC(sessionID, pPlayer->_playerID, pPlayer->_direction, pPlayer->_x, pPlayer->_y, pPlayer->_hp);

	int i;
	int x, y;
	PLAYER* pOtherPlayer;
	SECTOR_AROUND around;
	list<PLAYER*>* pSectorList;
	GetSectorAround(sectorX, sectorY, &around);
	AcquireLockAround(&around);
	// 소속 섹터 플레이어 정보를 신규 플레이어에게 보내주기
	for (i = 0; i < around.cnt; i++)
	{
		x = around.around[i]._x;
		y = around.around[i]._y;
		pSectorList = &_sector[y][x];
		auto it = pSectorList->begin();
		while (it != pSectorList->end())
		{
			pOtherPlayer = (*it);
			++it;
			CreateOtherCharacterSC(sessionID,
									pOtherPlayer->_playerID,
									pOtherPlayer->_direction,
									pOtherPlayer->_x,
									pOtherPlayer->_y,
									pOtherPlayer->_hp);
			if (pOtherPlayer->_action._isMove)
			{
				MoveStartSC(sessionID, pOtherPlayer->_playerID,
										pOtherPlayer->_action._moveDirection,
										pOtherPlayer->_x,
										pOtherPlayer->_y);
			}

		}
	}

	// 다른 캐릭터들에게 신규 플레이어 정보 보내주기
	CLanPacket* pPacket = CLanPacket::Alloc();
	*pPacket << (u_short)dfPACKET_SC_CREATE_OTHER_CHARACTER;
	*pPacket << pPlayer->_playerID;
	*pPacket << pPlayer->_direction;
	*pPacket << pPlayer->_x;
	*pPacket << pPlayer->_y;
	*pPacket << pPlayer->_hp;
	SendAround(&around, pPacket, NULL);
	CLanPacket::Free(pPacket);
	
	// 신규 플레이어도 Sector에 소속시킴
	_sector[sectorY][sectorX].push_back(pPlayer);
	ReleaseLockAround(&around);
	ReleasePlayer(pPlayer);
}

void Contents::DeletePlayer(unsigned __int64 sessionID)
{
	PLAYER* pPlayer;
	AcquireSRWLockExclusive(&_playerMapLock);
	auto it = _playerMap.find(sessionID);
	if (it == _playerMap.end())
	{
		ReleaseSRWLockExclusive(&_playerMapLock);
		return;
	}
	pPlayer = it->second;
	_playerMap.erase(it);
	ReleaseSRWLockExclusive(&_playerMapLock);

	AcquireSRWLockExclusive(&pPlayer->_playerLock);
	InterlockedExchange(&pPlayer->_owingThread, (ULONGLONG)GetCurrentThreadId());
	ReleaseSRWLockExclusive(&pPlayer->_playerLock);
	//EnterCriticalSection(&pPlayer->_playerLock);
	//LeaveCriticalSection(&pPlayer->_playerLock);

	SECTOR_AROUND around;
	GetSectorAround(pPlayer->_sector._x, pPlayer->_sector._y, &around);
	AcquireLockAround(&around);
	_sector[pPlayer->_sector._y][pPlayer->_sector._x].remove(pPlayer);

	CLanPacket* pDeletePacket = CLanPacket::Alloc();
	*pDeletePacket << (u_short)dfPACKET_SC_DELETE_CHARACTER;
	*pDeletePacket << pPlayer->_playerID;
	SendAround(&around, pDeletePacket, NULL);
	CLanPacket::Free(pDeletePacket);
	ReleaseLockAround(&around);

	playerPool.Free(pPlayer);
}

void Contents::CollisionDetection(Contents::PLAYER* pPlayer, BYTE type)
{
	struct Rect
	{
		POINT leftup;
		POINT rightdown;
	} rect = { 0,0 };
	int damage;
	switch (type)
	{
	case dfPACKET_CS_ATTACK1:
		if (pPlayer->_direction == dfPACKET_MOVE_DIR_LL)
		{
			rect.leftup.x = pPlayer->_x - dfATTACK1_RANGE_X;
			rect.leftup.y = pPlayer->_y - dfATTACK1_RANGE_Y;
			rect.rightdown.x = pPlayer->_x;
			rect.rightdown.y = pPlayer->_y + dfATTACK1_RANGE_Y;
		}
		else if (pPlayer->_direction == dfPACKET_MOVE_DIR_RR)
		{
			rect.leftup.x = pPlayer->_x;
			rect.leftup.y = pPlayer->_y - dfATTACK1_RANGE_Y;
			rect.rightdown.x = pPlayer->_x + dfATTACK1_RANGE_X;
			rect.rightdown.y = pPlayer->_y + dfATTACK1_RANGE_Y;
		}
		damage = dfATTACK1_DAMAGE;
		break;
	case dfPACKET_CS_ATTACK2:
		if (pPlayer->_direction == dfPACKET_MOVE_DIR_LL)
		{
			rect.leftup.x = pPlayer->_x - dfATTACK2_RANGE_X;
			rect.leftup.y = pPlayer->_y - dfATTACK2_RANGE_Y;
			rect.rightdown.x = pPlayer->_x;
			rect.rightdown.y = pPlayer->_y + dfATTACK2_RANGE_Y;
		}
		else if (pPlayer->_direction == dfPACKET_MOVE_DIR_RR)
		{
			rect.leftup.x = pPlayer->_x;
			rect.leftup.y = pPlayer->_y - dfATTACK2_RANGE_Y;
			rect.rightdown.x = pPlayer->_x + dfATTACK2_RANGE_X;
			rect.rightdown.y = pPlayer->_y + dfATTACK2_RANGE_Y;
		}
		damage = dfATTACK2_DAMAGE;
		break;
	case dfPACKET_CS_ATTACK3:
		if (pPlayer->_direction == dfPACKET_MOVE_DIR_LL)
		{
			rect.leftup.x = pPlayer->_x - dfATTACK3_RANGE_X;
			rect.leftup.y = pPlayer->_y - 30;
			rect.rightdown.x = pPlayer->_x;
			rect.rightdown.y = pPlayer->_y + 30;
		}
		else if (pPlayer->_direction == dfPACKET_MOVE_DIR_RR)
		{
			rect.leftup.x = pPlayer->_x;
			rect.leftup.y = pPlayer->_y - dfATTACK3_RANGE_Y;
			rect.rightdown.x = pPlayer->_x + dfATTACK3_RANGE_X;
			rect.rightdown.y = pPlayer->_y + dfATTACK3_RANGE_Y;
		}
		damage = dfATTACK3_DAMAGE;
		break;
	default:
		return;
	}

	int i;
	SECTOR_AROUND around;
	SECTOR_AROUND aroundDamaged;
	list<PLAYER*>* pSectorList;
	PLAYER* pExistPlayer;
	vector<unsigned __int64> attacked;
	GetSectorAround(pPlayer->_sector._x, pPlayer->_sector._y, &around);
	AcquireLockAround(&around);
	for (i = 0; i < around.cnt; i++)
	{
		pSectorList = &_sector[around.around[i]._y][around.around[i]._x];
		auto it = pSectorList->begin();
		while (it != pSectorList->end())
		{
			pExistPlayer = *it;
			++it;

			if (pExistPlayer == pPlayer)
				continue;
			if (pExistPlayer->_x < rect.leftup.x || pExistPlayer->_x > rect.rightdown.x)
				continue;
			if (pExistPlayer->_y < rect.leftup.y || pExistPlayer->_y > rect.rightdown.y)
				continue;

			attacked.push_back(pExistPlayer->_sessionID);
		}
	}
	ReleaseLockAround(&around);

	ReleasePlayer(pPlayer);

	auto vecIt = attacked.begin();
	while (vecIt != attacked.end())
	{
		AcquireSRWLockExclusive(&_playerMapLock);
		auto mapIt = _playerMap.find(*vecIt);
		++vecIt;
		if (mapIt == _playerMap.end())
		{
			ReleaseSRWLockExclusive(&_playerMapLock);
			continue;
		}
		pExistPlayer = mapIt->second;
		//EnterCriticalSection(&pExistPlayer->_playerLock);
		AcquireSRWLockExclusive(&pExistPlayer->_playerLock);
		InterlockedExchange(&pExistPlayer->_owingThread, (ULONGLONG)GetCurrentThreadId());
		ReleaseSRWLockExclusive(&_playerMapLock);

		pExistPlayer->_hp -= damage;
		GetSectorAround(pExistPlayer->_sector._x, pExistPlayer->_sector._y, &aroundDamaged);
		CLanPacket* pDamagePacket = CLanPacket::Alloc();
		*pDamagePacket << (u_short)dfPACKET_SC_DAMAGE;
		*pDamagePacket << pPlayer->_playerID;
		*pDamagePacket << pExistPlayer->_playerID;
		*pDamagePacket << pExistPlayer->_hp;
		AcquireLockAround(&aroundDamaged);
		SendAround(&aroundDamaged, pDamagePacket, NULL);
		ReleaseLockAround(&aroundDamaged);
		CLanPacket::Free(pDamagePacket);
		ReleaseSRWLockExclusive(&pExistPlayer->_playerLock);
		//LeaveCriticalSection(&pExistPlayer->_playerLock);
	}
}

Contents::PLAYER* Contents::FindAndLockPlayer(unsigned __int64 sessionID)
{
	PLAYER* pPlayer;
	AcquireSRWLockExclusive(&_playerMapLock);
	auto it = _playerMap.find(sessionID);
	if (it == _playerMap.end())
	{
		ReleaseSRWLockExclusive(&_playerMapLock);
		return nullptr;
	}
	pPlayer = it->second;
	AcquireSRWLockExclusive(&pPlayer->_playerLock);
	InterlockedExchange(&pPlayer->_owingThread, (ULONGLONG)GetCurrentThreadId());
	ReleaseSRWLockExclusive(&_playerMapLock);
	return pPlayer;
}

void Contents::ReleasePlayer(Contents::PLAYER* pPlayer)
{
	ReleaseSRWLockExclusive(&pPlayer->_playerLock);
}

//////////////////////////////////////////////////////////////////////////////////////////////
/* Send */

void Contents::CreateMyCharacterSC(unsigned __int64 sessionID, UINT id, BYTE direction, u_short x, u_short y, BYTE hp)
{
	CLanPacket* pPacket = CLanPacket::Alloc();
	*pPacket << (u_short)dfPACKET_SC_CREATE_MY_CHARACTER;
	*pPacket << id;
	*pPacket << direction;
	*pPacket << x;
	*pPacket << y;
	*pPacket << hp;
	SendPacket(sessionID, pPacket);
	CLanPacket::Free(pPacket);
}

void Contents::CreateOtherCharacterSC(unsigned __int64 sessionID, UINT id, BYTE direction, u_short x, u_short y, BYTE hp)
{
	CLanPacket* pPacket = CLanPacket::Alloc();
	*pPacket << (u_short)dfPACKET_SC_CREATE_OTHER_CHARACTER;
	*pPacket << id;
	*pPacket << direction;
	*pPacket << x;
	*pPacket << y;
	*pPacket << hp;
	SendPacket(sessionID, pPacket);
	CLanPacket::Free(pPacket);
}

void Contents::MoveStartSC(unsigned __int64 sessionID, UINT id, BYTE direction, u_short x, u_short y)
{
	CLanPacket* pPacket = CLanPacket::Alloc();
	*pPacket << (u_short)dfPACKET_SC_MOVE_START;
	*pPacket << id;
	*pPacket << direction;
	*pPacket << x;
	*pPacket << y;
	SendPacket(sessionID, pPacket);
	CLanPacket::Free(pPacket);
}

//////////////////////////////////////////////////////////////////////////////////////////////
/* Recv handler */

void Contents::HandleMoveStartCS(unsigned __int64 sessionID, CLanPacket* pPacket)
{
	PLAYER* pPlayer = FindAndLockPlayer(sessionID);
	pPlayer->_lastRecvTime = GetTickCount64();
	BYTE direction;
	u_short x;
	u_short y;
	*pPacket >> direction >> x >> y;

	SECTOR_AROUND around;
	UINT playerID = pPlayer->_playerID;
	GetSectorAround(pPlayer->_sector._x, pPlayer->_sector._y, &around);
	if (abs(x - pPlayer->_x) >= dfERROR_RANGE || abs(y - pPlayer->_y) >= dfERROR_RANGE)
	{
		x = pPlayer->_x;
		y = pPlayer->_y;
		CLanPacket* pSyncPacket = CLanPacket::Alloc();
		*pSyncPacket << (u_short)dfPACKET_SC_SYNC;
		*pSyncPacket << playerID;
		*pSyncPacket << x;
		*pSyncPacket << y;
		AcquireLockAround(&around);
		SendAround(&around, pSyncPacket, NULL);
		ReleaseLockAround(&around);
		CLanPacket::Free(pSyncPacket);
	}
	
	// 방향 및 위치 갱신
	pPlayer->_action._isMove = true;
	pPlayer->_action._moveDirection = direction;
	SetPlayerDirection(pPlayer, direction);
	pPlayer->_x = x;
	pPlayer->_y = y;

	// 섹터 갱신
	if (Sector_CheckAndUpdate(pPlayer))
	{
		GetSectorAround(pPlayer->_sector._x, pPlayer->_sector._y, &around);
	}

	// 주변에 이동 시작 패킷 뿌림
	CLanPacket* pMovePacket = CLanPacket::Alloc();
	*pMovePacket << (u_short)dfPACKET_SC_MOVE_START;
	*pMovePacket << playerID;
	*pMovePacket << direction;
	*pMovePacket << x;
	*pMovePacket << y;
	AcquireLockAround(&around);
	SendAround(&around, pMovePacket, pPlayer->_sessionID);
	ReleaseLockAround(&around);
	CLanPacket::Free(pMovePacket);
	ReleasePlayer(pPlayer);
}

void Contents::HandleMoveStopCS(unsigned __int64 sessionID, CLanPacket* pPacket)
{
	PLAYER* pPlayer = FindAndLockPlayer(sessionID);
	pPlayer->_lastRecvTime = GetTickCount64();
	BYTE direction;
	u_short x;
	u_short y;
	*pPacket >> direction >> x >> y;

	SECTOR_AROUND around;
	UINT playerID = pPlayer->_playerID;
	GetSectorAround(pPlayer->_sector._x, pPlayer->_sector._y, &around);
	if (abs(x - pPlayer->_x) >= dfERROR_RANGE || abs(y - pPlayer->_y) >= dfERROR_RANGE)
	{
		x = pPlayer->_x;
		y = pPlayer->_y;
		CLanPacket* pSyncPacket = CLanPacket::Alloc();
		*pSyncPacket << (u_short)dfPACKET_SC_SYNC;
		*pSyncPacket << playerID;
		*pSyncPacket << x;
		*pSyncPacket << y;
		AcquireLockAround(&around);
		SendAround(&around, pSyncPacket, NULL);
		ReleaseLockAround(&around);
		CLanPacket::Free(pSyncPacket);
	}

	// 방향 및 위치 갱신
	pPlayer->_action._isMove = false;
	pPlayer->_action._moveDirection = 0;
	SetPlayerDirection(pPlayer, direction);
	pPlayer->_x = x;
	pPlayer->_y = y;

	// 섹터 갱신
	if (Sector_CheckAndUpdate(pPlayer))
	{
		GetSectorAround(pPlayer->_sector._x, pPlayer->_sector._y, &around);
	}

	// 주변에 이동 중단 패킷 뿌림
	CLanPacket* pStopPacket = CLanPacket::Alloc();
	*pStopPacket << (u_short)dfPACKET_SC_MOVE_STOP;
	*pStopPacket << playerID;
	*pStopPacket << direction;
	*pStopPacket << x;
	*pStopPacket << y;
	AcquireLockAround(&around);
	SendAround(&around, pStopPacket, NULL);
	ReleaseLockAround(&around);
	CLanPacket::Free(pStopPacket);
	ReleasePlayer(pPlayer);
}

void Contents::HandleAttack1CS(unsigned __int64 sessionID, CLanPacket* pPacket)
{
	PLAYER* pPlayer = FindAndLockPlayer(sessionID);
	pPlayer->_lastRecvTime = GetTickCount64();
	BYTE direction;
	u_short x;
	u_short y;
	*pPacket >> direction >> x >> y;

	pPlayer->_direction = direction;
	SECTOR_AROUND around;
	GetSectorAround(pPlayer->_sector._x, pPlayer->_sector._y, &around);
	if (abs(x - pPlayer->_x) >= dfERROR_RANGE || abs(y - pPlayer->_y) >= dfERROR_RANGE)
	{
		x = pPlayer->_x;
		y = pPlayer->_y;
		CLanPacket* pSyncPacket = CLanPacket::Alloc();
		*pSyncPacket << (u_short)dfPACKET_SC_SYNC;
		*pSyncPacket << pPlayer->_playerID;
		*pSyncPacket << x;
		*pSyncPacket << y;
		AcquireLockAround(&around);
		SendAround(&around, pSyncPacket, NULL);
		ReleaseLockAround(&around);
		CLanPacket::Free(pSyncPacket);
	}
	pPlayer->_x = x;
	pPlayer->_y = y;

	CLanPacket* pAttackPacket = CLanPacket::Alloc();
	*pAttackPacket << (u_short)dfPACKET_SC_ATTACK1;
	*pAttackPacket << pPlayer->_playerID;
	*pAttackPacket << direction;
	*pAttackPacket << x;
	*pAttackPacket << y;
	AcquireLockAround(&around);
	SendAround(&around, pAttackPacket, pPlayer->_sessionID);
	ReleaseLockAround(&around);
	CLanPacket::Free(pAttackPacket);
	CollisionDetection(pPlayer, dfPACKET_CS_ATTACK2);
}

void Contents::HandleAttack2CS(unsigned __int64 sessionID, CLanPacket* pPacket)
{
	PLAYER* pPlayer = FindAndLockPlayer(sessionID);
	pPlayer->_lastRecvTime = GetTickCount64();
	BYTE direction;
	u_short x;
	u_short y;
	*pPacket >> direction >> x >> y;

	pPlayer->_direction = direction;
	SECTOR_AROUND around;
	GetSectorAround(pPlayer->_sector._x, pPlayer->_sector._y, &around);
	if (abs(x - pPlayer->_x) >= dfERROR_RANGE || abs(y - pPlayer->_y) >= dfERROR_RANGE)
	{
		x = pPlayer->_x;
		y = pPlayer->_y;
		CLanPacket* pSyncPacket = CLanPacket::Alloc();
		*pSyncPacket << (u_short)dfPACKET_SC_SYNC;
		*pSyncPacket << pPlayer->_playerID;
		*pSyncPacket << x;
		*pSyncPacket << y;
		AcquireLockAround(&around);
		SendAround(&around, pSyncPacket, NULL);
		ReleaseLockAround(&around);
		CLanPacket::Free(pSyncPacket);
	}
	pPlayer->_x = x;
	pPlayer->_y = y;

	CLanPacket* pAttackPacket = CLanPacket::Alloc();
	*pAttackPacket << (u_short)dfPACKET_SC_ATTACK2;
	*pAttackPacket << pPlayer->_playerID;
	*pAttackPacket << direction;
	*pAttackPacket << x;
	*pAttackPacket << y;
	AcquireLockAround(&around);
	SendAround(&around, pAttackPacket, pPlayer->_sessionID);
	ReleaseLockAround(&around);
	CLanPacket::Free(pAttackPacket);
	CollisionDetection(pPlayer, dfPACKET_CS_ATTACK2);
}

void Contents::HandleAttack3CS(unsigned __int64 sessionID, CLanPacket* pPacket)
{
	PLAYER* pPlayer = FindAndLockPlayer(sessionID);
	pPlayer->_lastRecvTime = GetTickCount64();
	BYTE direction;
	u_short x;
	u_short y;
	*pPacket >> direction >> x >> y;

	pPlayer->_direction = direction;
	SECTOR_AROUND around;
	GetSectorAround(pPlayer->_sector._x, pPlayer->_sector._y, &around);
	if (abs(x - pPlayer->_x) >= dfERROR_RANGE || abs(y - pPlayer->_y) >= dfERROR_RANGE)
	{
		x = pPlayer->_x;
		y = pPlayer->_y;
		CLanPacket* pSyncPacket = CLanPacket::Alloc();
		*pSyncPacket << (u_short)dfPACKET_SC_SYNC;
		*pSyncPacket << pPlayer->_playerID;
		*pSyncPacket << x;
		*pSyncPacket << y;
		AcquireLockAround(&around);
		SendAround(&around, pSyncPacket, NULL);
		ReleaseLockAround(&around);
		CLanPacket::Free(pSyncPacket);
	}
	pPlayer->_x = x;
	pPlayer->_y = y;

	CLanPacket* pAttackPacket = CLanPacket::Alloc();
	*pAttackPacket << (u_short)dfPACKET_SC_ATTACK3;
	*pAttackPacket << pPlayer->_playerID;
	*pAttackPacket << direction;
	*pAttackPacket << x;
	*pAttackPacket << y;
	AcquireLockAround(&around);
	SendAround(&around, pAttackPacket, pPlayer->_sessionID);
	ReleaseLockAround(&around);
	CLanPacket::Free(pAttackPacket);
	CollisionDetection(pPlayer, dfPACKET_CS_ATTACK3);
}

void Contents::HandleEchoCS(unsigned __int64 sessionID, CLanPacket* pPacket)
{
	PLAYER* pPlayer = FindAndLockPlayer(sessionID);
	pPlayer->_lastRecvTime = GetTickCount64();

	UINT time;
	*pPacket >> time;
	CLanPacket* pSendPacket = CLanPacket::Alloc();
	*pSendPacket << (u_short)dfPACKET_SC_ECHO;
	*pSendPacket << time;
	SendPacket(pPlayer->_sessionID, pSendPacket);
	CLanPacket::Free(pSendPacket);
	ReleasePlayer(pPlayer);
}