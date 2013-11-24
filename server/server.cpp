// server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <Windows.h>
#include <iostream>
#include <string>
using namespace std;

#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
 #define BUFSIZE 512
 
DWORD WINAPI InstanceThread(LPVOID); 
VOID GetAnswerToRequest(char *, LPDWORD); 
void ProcessClientMessage(char *pchRequest);
 
int _tmain(VOID) 
{ 
   BOOL   fConnected = FALSE; 
   DWORD  dwThreadId = 0; 
   HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL; 
   LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\testpipe"); 
 
// The main loop creates an instance of the named pipe and 
// then waits for a client to connect to it. When the client 
// connects, a thread is created to handle communications 
// with that client, and this loop is free to wait for the
// next client connect request. It is an infinite loop.
 
   for (;;) 
   { 
      _tprintf( TEXT("\nPipe Server: Main thread awaiting client connection on %s\n"), lpszPipename);
      hPipe = CreateNamedPipe( 
          lpszPipename,             // pipe name 
          PIPE_ACCESS_DUPLEX,       // read/write access 
          PIPE_TYPE_BYTE |       // message type pipe 
          PIPE_READMODE_BYTE |   // message-read mode 
          PIPE_WAIT,                // blocking mode 
          PIPE_UNLIMITED_INSTANCES, // max. instances  
          BUFSIZE,                  // output buffer size 
          BUFSIZE,                  // input buffer size 
          0,                        // client time-out 
          NULL);                    // default security attribute 

      if (hPipe == INVALID_HANDLE_VALUE) 
      {
          _tprintf(TEXT("CreateNamedPipe failed, GLE=%d.\n"), GetLastError()); 
          return -1;
      }
 
      // Wait for the client to connect; if it succeeds, 
      // the function returns a nonzero value. If the function
      // returns zero, GetLastError returns ERROR_PIPE_CONNECTED. 
 
      fConnected = ConnectNamedPipe(hPipe, NULL) ? 
         TRUE : (GetLastError() == ERROR_PIPE_CONNECTED); 
 
      if (fConnected) 
      { 
         printf("Client connected, creating a processing thread.\n"); 
      
         // Create a thread for this client. 
         hThread = CreateThread( 
            NULL,              // no security attribute 
            0,                 // default stack size 
            InstanceThread,    // thread proc
            (LPVOID) hPipe,    // thread parameter 
            0,                 // not suspended 
            &dwThreadId);      // returns thread ID 

         if (hThread == NULL) 
         {
            _tprintf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError()); 
            return -1;
         }
         else CloseHandle(hThread); 
       } 
      else 
        // The client could not connect, so close the pipe. 
         CloseHandle(hPipe); 
   } 

   return 0; 
} 
 
DWORD WINAPI InstanceThread(LPVOID lpvParam)
// This routine is a thread processing function to read from and reply to a client
// via the open pipe connection passed from the main loop. Note this allows
// the main loop to continue executing, potentially creating more threads of
// of this procedure to run concurrently, depending on the number of incoming
// client connections.
{ 
   HANDLE hHeap      = GetProcessHeap();
   char* pRequest = (char*)HeapAlloc(hHeap, 0, BUFSIZE);
   char* pReply   = (char*)HeapAlloc(hHeap, 0, BUFSIZE);

   DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0; 
   BOOL fSuccess = FALSE;
   HANDLE hPipe  = NULL;

   // Do some extra error checking since the app will keep running even if this
   // thread fails.

   if (lpvParam == NULL)
   {
       printf( "\nERROR - Pipe Server Failure:\n");
       printf( "   InstanceThread got an unexpected NULL value in lpvParam.\n");
       printf( "   InstanceThread exitting.\n");
       if (pReply != NULL) HeapFree(hHeap, 0, pReply);
       if (pRequest != NULL) HeapFree(hHeap, 0, pRequest);
       return (DWORD)-1;
   }

   if (pRequest == NULL)
   {
       printf( "\nERROR - Pipe Server Failure:\n");
       printf( "   InstanceThread got an unexpected NULL heap allocation.\n");
       printf( "   InstanceThread exitting.\n");
       if (pReply != NULL) HeapFree(hHeap, 0, pReply);
       return (DWORD)-1;
   }

   if (pReply == NULL)
   {
       printf( "\nERROR - Pipe Server Failure:\n");
       printf( "   InstanceThread got an unexpected NULL heap allocation.\n");
       printf( "   InstanceThread exitting.\n");
       if (pRequest != NULL) HeapFree(hHeap, 0, pRequest);
       return (DWORD)-1;
   }

   // Print verbose messages. In production code, this should be for debugging only.
   printf("InstanceThread created, receiving and processing messages.\n");

// The thread's parameter is a handle to a pipe object instance. 
 
   hPipe = (HANDLE) lpvParam; 

// Loop until done reading
   while (1) 
   { 
   // Read client requests from the pipe. This simplistic code only allows messages
   // up to BUFSIZE characters in length.
      fSuccess = ReadFile( 
         hPipe,        // handle to pipe 
         pRequest,    // buffer to receive data 
         BUFSIZE, // size of buffer 
         &cbBytesRead, // number of bytes read 
         NULL);        // not overlapped I/O 

      if (!fSuccess || cbBytesRead == 0)
      {   
          if (GetLastError() == ERROR_BROKEN_PIPE)
          {
              _tprintf(TEXT("InstanceThread: client disconnected.\n"), GetLastError()); 
          }
          else
          {
              _tprintf(TEXT("InstanceThread ReadFile failed, GLE=%d.\n"), GetLastError()); 
          }
          break;
      }

	  //Process received message
	  ProcessClientMessage(pRequest);

   // Get response string
      GetAnswerToRequest(pReply, &cbReplyBytes); 
 
   // Write the reply to the pipe. 
      fSuccess = WriteFile( 
         hPipe,        // handle to pipe 
         pReply,     // buffer to write from 
         cbReplyBytes, // number of bytes to write 
         &cbWritten,   // number of bytes written 
         NULL);        // not overlapped I/O 

      if (!fSuccess || cbReplyBytes != cbWritten)
      {   
          _tprintf(TEXT("InstanceThread WriteFile failed, GLE=%d.\n"), GetLastError()); 
          break;
      }
  }

// Flush the pipe to allow the client to read the pipe's contents 
// before disconnecting. Then disconnect the pipe, and close the 
// handle to this pipe instance. 
 
   FlushFileBuffers(hPipe); 
   DisconnectNamedPipe(hPipe); 
   CloseHandle(hPipe); 

   HeapFree(hHeap, 0, pRequest);
   HeapFree(hHeap, 0, pReply);

   printf("InstanceThread exitting.\n");
   return 1;
}

VOID GetAnswerToRequest(char *pReply, LPDWORD pchBytes )
// This routine is a simple function to print the client request to the console
// and populate the reply buffer with a default data string. This is where you
// would put the actual client request processing code that runs in the context
// of an instance thread. Keep in mind the main thread will continue to wait for
// and receive other client connections while the instance thread is working.
{
    //_tprintf( TEXT("Client Request String:\"%s\"\n"), pchRequest );

	char str[] = "default answer from server";
	strncpy(pReply, str, BUFSIZE);
    *pchBytes = (strlen(str)+1);
}

void ProcessClientMessage(char *pchRequest)
{
	printf("rx %d\n", strlen(pchRequest));
}
