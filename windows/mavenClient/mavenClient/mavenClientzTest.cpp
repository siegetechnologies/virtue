// mavenClient.cpp : Defines the entry point for the console application.
//
// server code follows the example at http://www.codersource.net/2010/02/17/socket-server-in-winsock-event-object-model/

#include "stdafx.h"
//NetworkListner.cpp

#define CONSOLE_LINE_SIZE 132

#define WINDIVERT_DEBUG
#ifdef WINDIVERT_DEBUG
#define PRINT_DBG printf
#define WPRINT_DBG wprintf
#else
#define PRINT_DBG(...)
#define WPRINT_DBG(...)
#endif

#define DATA_BUFSIZE 8192

HANDLE hStopEvent;
HANDLE NetworkEvent;
HANDLE hServerThread;
int clientno;
CArray<HANDLE, HANDLE> threadArray;


DWORD WINAPI ClientThread(LPVOID thread_data)
{
	DWORD Event;
	SOCKET sClient;
	WSANETWORKEVENTS NetworkEvents;
	char str[200];
	DWORD recvLength;


	sClient = (SOCKET &)thread_data;

	while (true)
	{
		if ((Event = WSAWaitForMultipleEvents(1, &NetworkEvent, FALSE, WSA_INFINITE, FALSE)) == WSA_WAIT_FAILED)
		{
			printf("WSAWaitForMultipleEvents failed with error %d\n", WSAGetLastError());
			return 0;
		}

		if (WSAEnumNetworkEvents(sClient, NetworkEvent, &NetworkEvents) == SOCKET_ERROR)
		{
			printf("WSAEnumNetworkEvents failed with error %d\n", WSAGetLastError());
			return 0;
		}

		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			if (NetworkEvents.lNetworkEvents & FD_READ && NetworkEvents.iErrorCode[FD_READ_BIT] != 0)
			{
				printf("FD_READ failed with error %d\n", NetworkEvents.iErrorCode[FD_READ_BIT]);
			}
			else
			{
				recvLength = recv(sClient, str, 200, 0);
				str[recvLength] = '\0';
				printf("%s\n", str);
				send(sClient, str, 200, 0);
				if (strcmp(str, "stop") == 0)
				{
					SetEvent(hStopEvent);
					ExitThread(0);
				}

			}
		}
		if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			shutdown(sClient, FD_READ | FD_WRITE);
			closesocket(sClient);
			ExitThread(0);
		}

	}


	//	SetEvent(hStopEvent);
	return 0;
}


DWORD WINAPI ServerThread(LPVOID thread_data)
{
	SOCKET sServer, sClient;
	SOCKADDR_IN client;
	int iAddrSize;
	DWORD dwThreadId;
	HANDLE hClientThread;

	NetworkEvent = WSACreateEvent();

	sServer = (SOCKET &)thread_data;
	if (listen(sServer, 5))
	{
		printf("listen() failed with error %d\n", WSAGetLastError());
		return 0;
	}

	while (1)
	{
		iAddrSize = sizeof(client);
		sClient = accept(sServer, (struct sockaddr *)&client,
			&iAddrSize);
		if (sClient == INVALID_SOCKET)
		{
			printf("accept() failed: %d\n", WSAGetLastError());
			break;
		}
		printf("Accepted client:\n");

		if (WSAEventSelect(sClient, NetworkEvent, FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
		{
			printf("Error in Event Select\n");
			break;
		}

		hClientThread = CreateThread(NULL, 0, &ClientThread, (LPVOID)sClient, 0, &dwThreadId);
		if (hClientThread == NULL)
		{
			printf("CreateThread() failed: %d\n", GetLastError());
			break;
		}
		else
			threadArray.Add(hClientThread);
	}
	return 0;
}

int StartServer()
{

	SOCKET Listen;
	SOCKADDR_IN InternetAddr;
	WSADATA wsaData;
	DWORD Ret;
	DWORD dwThreadId;


	if ((Ret = WSAStartup(0x0202, &wsaData)) != 0)
	{
		printf("WSAStartup() failed with error %d\n", Ret);
		return 1;
	}

	if ((Listen = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		printf("socket() failed with error %d\n", WSAGetLastError());
		return 1;
	}


	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(MAVEN_PORT_NUMBER_BIN);

	if (bind(Listen, (PSOCKADDR)&InternetAddr, sizeof(InternetAddr)) == SOCKET_ERROR)
	{
		printf("bind() failed with error %d\n", WSAGetLastError());
		return 1;
	}


	hServerThread = CreateThread(NULL, 0, &ServerThread, (LPVOID)Listen, 0, &dwThreadId);

	if (hServerThread == NULL)
	{
		printf("CreateThread() failed: %d\n", GetLastError());
		return 1;
	}

	return 0;
}



int StopServer()
{
	for (int clientCount = 0; clientCount <= threadArray.GetUpperBound(); clientCount++)
	{
		printf("Closing %d\n", clientCount);
		CloseHandle(threadArray.GetAt(clientCount));
	}

	CloseHandle(hServerThread);

	return 0;
}

int main(void)
{


	PRINT_DBG("mavenClient\n");
	hStopEvent = CreateEvent(NULL, TRUE, FALSE, L"Network");

	StartServer();

	WaitForSingleObject(hStopEvent, INFINITE);
	StopServer();


	return 0;
}
