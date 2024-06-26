#pragma once
#include <Windows.h>
#pragma comment(lib, "Dbghelp.lib")
#include <dbghelp.h>
#include <stdio.h>
#include <crtdbg.h>
#include <psapi.h>

class CrashDump
{
public:
	CrashDump();
	static void Crash();
	static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer);
	static void SetHandlerDump();
	static void myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved);
	static int _custom_Report_hook(int ireposttype, char* message, int* returnvalue);
	static void myPurecallHandler();
	static long _DumpCount;
};

extern CrashDump crash;