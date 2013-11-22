// client.cpp : Defines the entry point for the console application.
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

VOID CALLBACK FileIOCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,LPOVERLAPPED lpOverlapped)
{
	cout << "FileIOCompletionRoutine" << endl;
}

int _tmain(int argc, _TCHAR* argv[])
{
	LPCTSTR n = L"\\\\.\\pipe\\testpipe";

	HANDLE h = CreateFile(n,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		NULL);

	if(h == 0)
	{
		wcout << ErrStr(GetLastError()) << endl;
	}

	OVERLAPPED rxo;
	memset(&rxo, 0x00, sizeof(OVERLAPPED));
	OVERLAPPED txo;
	memset(&txo, 0x00, sizeof(OVERLAPPED));

	while(1)
	{
		Sleep(1000);
		char *test = "Test";
		
		DWORD bytesWritten = 0;
		BOOL res = WriteFileEx(h, test, strlen(test), &txo, NULL);
		if(res==0) wcout << ErrStr(GetLastError()) << endl;
		cout << "Client tx " << res << "," << bytesWritten << endl;

		res = GetOverlappedResult(h, &txo, &bytesWritten, TRUE);
		if(res==0) wcout << ErrStr(GetLastError()) << endl;
		cout << "Client tx2 " << res << "," << bytesWritten << endl;

		char buff[1000];
		DWORD bytesRead = 0;
		
		if(HasOverlappedIoCompleted(&rxo))
		{
			res = ReadFileEx(h,
			  buff,
			  1000,
			  &rxo,
			  &FileIOCompletionRoutine);
			if(res==0) 
				wcout << "rx " << GetLastError() << " " << ErrStr(GetLastError()) << endl;
		}

		res = GetOverlappedResult(h, &rxo, &bytesRead, FALSE);

		if(res==0) 
			wcout << "rx2 " << GetLastError() << " " << ErrStr(GetLastError()) << endl;

		cout << "Server rx " << res << "," << bytesRead << endl;

		
	}


	return 0;
}

