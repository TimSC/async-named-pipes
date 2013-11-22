// server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <Windows.h>
#include <iostream>
#include <string>
using namespace std;

wstring ErrStr(DWORD err)
{
	// Translate ErrorCode to String.
	LPTSTR Error = 0;
	if(::FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
						NULL,
						err,
						0,
						(LPTSTR)&Error,
						0,
						NULL) == 0)
	{
	   // Failed in translating.
		wstring out(L"Unknown error");
		return out;
	}
	wstring out(Error);
	return out;
}

VOID WINAPI CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead, 
    LPOVERLAPPED lpOverLap) 
{
	cout << "CompletedReadRoutine " << cbBytesRead << endl;
}

int _tmain(int argc, _TCHAR* argv[])
{
	LPCTSTR n = L"\\\\.\\pipe\\testpipe";
	HANDLE h = CreateNamedPipe(n, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, 
		PIPE_TYPE_BYTE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES,
		1024*1024,
		1024*1024,
		5000, NULL);

	if(h == 0)
	{
		wcout << ErrStr(GetLastError()) << endl;
	}

	HANDLE hConnectEvent = CreateEvent( 
      NULL,    // default security attribute
      TRUE,    // manual reset event 
      TRUE,    // initial state = signaled 
      NULL);   // unnamed event object 

	OVERLAPPED conovr;
	memset(&conovr, 0x00, sizeof(OVERLAPPED));
	conovr.hEvent = hConnectEvent; 

	BOOL res = ConnectNamedPipe(h, &conovr);

	DWORD dwWait = WaitForSingleObjectEx( 
         hConnectEvent,  // event object to wait for 
         INFINITE,       // waits indefinitely 
         TRUE);          // alertable wait enabled 

	if(res == 0)
	{
		wcout << ErrStr(GetLastError()) << endl;
	}

	OVERLAPPED rxo;
	memset(&rxo, 0x00, sizeof(OVERLAPPED));
	OVERLAPPED txo;
	memset(&txo, 0x00, sizeof(OVERLAPPED));

	HANDLE ev;

	while(1)
	{
		char buff[1000];
		DWORD bytesRead = 0;

		if(HasOverlappedIoCompleted(&rxo))
		{
			BOOL res = ReadFileEx(h,
			  buff,
			  1000,
			  &rxo,
			  (LPOVERLAPPED_COMPLETION_ROUTINE) CompletedReadRoutine);
			if(res==0 && GetLastError() != ERROR_NO_DATA) 
				wcout << "rx " << GetLastError() << " " << ErrStr(GetLastError()) << endl;

		}

		BOOL res = GetOverlappedResult(h, &rxo, &bytesRead, FALSE);
		
		/*if (res == 0 && GetLastError() == ERROR_BROKEN_PIPE)
		{
			cout << "Reinit" << endl;
			//Reinitialise listening for client
			DisconnectNamedPipe(h);
			BOOL res = ConnectNamedPipe(h, NULL);
			continue;
		}*/

		if(res==0) 
			wcout << "rx2 " << GetLastError() << " " << ErrStr(GetLastError()) << endl;

		cout << "Server rx " << res << "," << bytesRead << endl;

		char test2[] = "test2";
		DWORD bytesWritten = 0;
		BOOL es = WriteFileEx(h, test2, strlen(test2), &txo, NULL);
		if(res==0) wcout << "tx " << ErrStr(GetLastError()) << endl;

		cout << "Server tx " << res << "," << bytesRead << endl;

		res = GetOverlappedResult(h, &txo, &bytesWritten, TRUE);
		if(res==0) wcout << ErrStr(GetLastError()) << endl;
		cout << "Client tx2 " << res << "," << bytesWritten << endl;

		Sleep(20);
	}

	return 0;
}

