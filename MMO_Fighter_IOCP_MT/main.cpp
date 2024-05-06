#include "Contents.h"

int main()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	CLanServer* server = new Contents();
	bool svStatus = server->Start(NULL, 20000, si.dwNumberOfProcessors + 4, si.dwNumberOfProcessors - 2, false, 20000);
	// Logic
	while (svStatus)
	{
		// 종료 입력 등..
		Sleep(10000);
	}

	server->Stop();
	return 0;
}