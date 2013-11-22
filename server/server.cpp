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
	OVERLAPPED txo[INSTANCES];
	OVERLAPPED rxo[INSTANCES];
	BOOL waitingForConnection[INSTANCES];
	char *rxBuffs[INSTANCES];
	int writeMode[INSTANCES];
	int readPending[INSTANCES];
	int writePending[INSTANCES];

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
		::memset(&txo[i], 0x00, sizeof(OVERLAPPED));
		::memset(&rxo[i], 0x00, sizeof(OVERLAPPED));
		conovr[i].hEvent = hConnectEvents[i]; 

		ConnectNamedPipe(hPipes[i], &conovr[i]);
		waitingForConnection[i] = 1;

		pendingIo[i] = 0;
		if(GetLastError()==ERROR_IO_PENDING)
			pendingIo[i] = 1;

		cout << i << " " << pendingIo[i] << endl;

		rxBuffs[i] = new char[1000];
		writeMode[i] = 0;
		readPending[i] = 0;
		writePending[i] = 0;
	}

	int count = 0;
	while(1)
	{

		DWORD dwWait = WaitForMultipleObjects(  INSTANCES,
			 hConnectEvents,  // event object to wait for 
			 FALSE,
			 INFINITE);

		int ind = dwWait - WAIT_OBJECT_0 ;
		int ind2 = dwWait - WAIT_ABANDONED_0;
		if(dwWait == WAIT_TIMEOUT)
		{
			cout << "WAIT_TIMEOUT" << endl;
		}
		if(ind >= 0 && ind < INSTANCES)
		{
			//cout << "Wait complete " << ind << endl;
			//cout << "io complete " << HasOverlappedIoCompleted(&conovr[ind]) << endl;
			h = hPipes[ind];


			if(readPending[ind])
			{
				DWORD bytesRx = 0;
				BOOL fSuccess = GetOverlappedResult( 
					hPipes[ind], // handle to pipe 
					&rxo[ind], // OVERLAPPED structure 
					&bytesRx,            // bytes transferred 
					FALSE);            // do not wait 

				if(fSuccess==0) 
					wcout << "GetOverlappedResult " << GetLastError() << " " << ErrStr(GetLastError()) << endl;

				//cout << ind << " " << bytesRx << endl;
				//wcout << GetLastError() << " " << ErrStr(GetLastError()) << endl;

				if(fSuccess && bytesRx > 0)
				{
					cout << "rx " << ind << " " << bytesRx << endl;
					readPending[ind] = 0;
					pendingIo[ind] = 0;
					writeMode[ind] = 1;
					count += 1;
				}

			}

			if(writePending[ind])
			{
				DWORD bytesRx = 0;
				BOOL fSuccess = GetOverlappedResult( 
					hPipes[ind], // handle to pipe 
					&txo[ind], // OVERLAPPED structure 
					&bytesRx,            // bytes transferred 
					FALSE);            // do not wait 

				if(fSuccess==0) 
					wcout << "GetOverlappedResult " << GetLastError() << " " << ErrStr(GetLastError()) << endl;

				//cout << ind << " " << bytesRx << endl;
				//wcout << GetLastError() << " " << ErrStr(GetLastError()) << endl;

				if(fSuccess && bytesRx > 0)
				{
					cout << "tx " << ind << " " << bytesRx << endl;
					writePending[ind] = 0;
					pendingIo[ind] = 0;
					writeMode[ind] = 0;
					count += 1;
				}			
			}

			if(waitingForConnection[ind])
			{
				cout << "Connection on pipe " << ind << endl;
				waitingForConnection[ind] = 0;
			}

			if(!readPending[ind] && !writePending[ind])
			{
				if(writeMode[ind])
				{
					cout << "Starting write " << ind << endl;
					::memset(&txo[ind], 0x00, sizeof(OVERLAPPED));
					txo[ind].hEvent = hConnectEvents[ind]; 

					char testBuff[] = "Test Transmission";
					BOOL res = WriteFileEx(hPipes[ind], 
						testBuff, 
						strlen(testBuff), 
						&txo[ind], NULL);
					
					if(res==0) 
						wcout << "tx " << GetLastError() << " " << ErrStr(GetLastError()) << endl;

					pendingIo[ind] = 1;
					writeMode[ind] = 0;
					writePending[ind] = 1;
				}
				else
				{
					cout << "Starting read " << ind << endl;
					::memset(&rxo[ind], 0x00, sizeof(OVERLAPPED));
					rxo[ind].hEvent = hConnectEvents[ind]; 

					BOOL res = ReadFileEx(hPipes[ind],
						rxBuffs[ind],
						1000,
						&rxo[ind],
						NULL);

					if(res==0) 
						wcout << "rx " << GetLastError() << " " << ErrStr(GetLastError()) << endl;

					pendingIo[ind] = 1;
					readPending[ind] = 1;
				}
			}

		}

		if(ind2 >= 0 && ind2 < INSTANCES)
		{
			cout << "Wait abandoned " << ind2 << endl;
		}

		Sleep(20);
	}

	while(1)
		Sleep(1000);

	return 0;
}

