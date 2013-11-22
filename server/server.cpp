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

#define INSTANCES 4

int _tmain(int argc, _TCHAR* argv[])
{

	HANDLE hConnectEvents[INSTANCES]; 
	HANDLE hPipes[INSTANCES];
	HANDLE h;
	BOOL pendingIo[INSTANCES];
	OVERLAPPED conovr[INSTANCES];
	BOOL waitingForConnection[INSTANCES];
	char *rxBuffs[INSTANCES];
	int writeMode[INSTANCES];

	for (int i = 0; i < INSTANCES; i++) 
	{ 

		LPCTSTR n = L"\\\\.\\pipe\\testpipe";
		hPipes[i] = CreateNamedPipe(n, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, 
			PIPE_TYPE_BYTE | PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,
			1024*1024,
			1024*1024,
			5000, NULL);

		if(hPipes[i] == 0)
		{
			wcout << ErrStr(GetLastError()) << endl;
		}

		hConnectEvents[i] = CreateEvent( 
		  NULL,    // default security attribute
		  TRUE,    // manual reset event 
		  TRUE,    // initial state = signaled 
		  NULL);   // unnamed event object 

		::memset(&conovr[i], 0x00, sizeof(OVERLAPPED));
		conovr[i].hEvent = hConnectEvents[i]; 

		ConnectNamedPipe(hPipes[i], &conovr[i]);
		waitingForConnection[i] = 1;

		pendingIo[i] = 0;
		if(GetLastError()==ERROR_IO_PENDING)
			pendingIo[i] = 1;

		cout << i << " " << pendingIo[i] << endl;

		rxBuffs[i] = new char[1000];
		writeMode[i] = 0;
	}

	while(1)
	{

		DWORD dwWait = WaitForMultipleObjects(  INSTANCES,
			 hConnectEvents,  // event object to wait for 
			 FALSE,
			 INFINITE);          

		int ind = dwWait - WAIT_OBJECT_0 ;
		int ind2 = dwWait - WAIT_ABANDONED_0;
		if(dwWait == WAIT_TIMEOUT)
			int timeOut = 1;
		if(ind >= 0 && ind < INSTANCES)
		{
			h = hPipes[ind];

			if(pendingIo[ind])
			{
				DWORD bytesRx = 0;
				BOOL fSuccess = GetOverlappedResult( 
					hPipes[ind], // handle to pipe 
					&conovr[ind], // OVERLAPPED structure 
					&bytesRx,            // bytes transferred 
					FALSE);            // do not wait 

				if(bytesRx > 0)
					cout << ind << " " << bytesRx << endl;

				pendingIo[ind] = 0;
			}

			if(waitingForConnection[ind])
			{
				cout << "Connection on pipe " << ind << endl;
				waitingForConnection[ind] = 0;
			}

			if(!pendingIo[ind])
			{
				if(writeMode[ind])
				{
					char testBuff[] = "Test Transmission";
					BOOL es = WriteFileEx(hPipes[ind], 
						testBuff, 
						strlen(testBuff), 
						&conovr[ind], NULL);
					//if(res==0 && GetLastError() != ERROR_NO_DATA) 
					//	wcout << "rx " << GetLastError() << " " << ErrStr(GetLastError()) << endl;

					pendingIo[ind] = 1;
					writeMode[ind] = 0;
				}
				else
				{
					BOOL res = ReadFileEx(hPipes[ind],
						rxBuffs[ind],
						1000,
						&conovr[ind],
						(LPOVERLAPPED_COMPLETION_ROUTINE) CompletedReadRoutine);
					//if(res==0 && GetLastError() != ERROR_NO_DATA) 
					//	wcout << "rx " << GetLastError() << " " << ErrStr(GetLastError()) << endl;

					pendingIo[ind] = 1;
					writeMode[ind] = 1;
				}
			}

		}
	}

	return 0;
}

