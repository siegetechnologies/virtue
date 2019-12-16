//NetworkListenter.h


#include <afxtempl.h>
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>


DWORD WINAPI ClientThread(LPVOID thread_data);
DWORD WINAPI ServerThread(LPVOID thread_data);
int StartServer();
int StopServer();
