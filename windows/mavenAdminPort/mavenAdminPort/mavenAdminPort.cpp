/*++

Module Name: mavenAdminPort.cpp

Abstract:

This application directs maven filter messages to the fluentd server.

Environment:

User mode

Revision History:
Author Richard Carter - Siege Technologies, Manchester NH

V1.1	03/15/2018 - REC, written
V1.2	03/27/2018 - REC, handle fragmented messages
--*/

#include "stdafx.h"

#undef WINDIVERT_DEBUG
#ifdef WINDIVERT_DEBUG
#define PRINT_DBG printf
#define WPRINT_DBG wprintf
#else
#define PRINT_DBG(...)
#define WPRINT_DBG(...)
#endif

#define VERSION_STRING " Maven Admin Port V1.2"
#define _ERROR(fmt,...) fprintf(stderr, "%s line %d " fmt, __FILE__, __LINE__, __VA_ARGS__)

static SOCKET mavenSocket = INVALID_SOCKET;
static SOCKET adminSocket = INVALID_SOCKET;
static HANDLE hStopEvent;

/***************************************************************************
* Name: ConsoleCtrlEventHandler
*
* Routine Description: This function is called when the process exits.  It
*	performs cleanup needed to gracefully exit.
*	NOTE: this is cobbled from this example 
*		http://www.cplusplus.com/forum/general/57290/
*
* Returns: FALSE
*
**************************************************************************/
BOOL WINAPI ConsoleCtrlEventHandler(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		// Do nothing.
		// To prevent other potential handlers from
		// doing anything, return TRUE instead.
		break;

	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_CLOSE_EVENT:
		// Do your final processing here!
		//MessageBox(NULL, L"Exit application", L"Exit", MB_OK | MB_ICONINFORMATION);
		if (mavenSocket != INVALID_SOCKET)
		{
			closesocket(mavenSocket);
			mavenSocket = INVALID_SOCKET;

		}
		if (adminSocket != INVALID_SOCKET)
		{
			closesocket(adminSocket);
			adminSocket = INVALID_SOCKET;

		}
		return FALSE;

	}

	// If it gets this far (it shouldn't), do nothing.
	return FALSE;
}

/***************************************************************************
* Name: recvMsg
*
* Routine Description: This function receives a message from a socket and
*	collects message fragments until the entire message is received.
*
* Returns: number of bytes received or zero if error
*
**************************************************************************/
int recvMsg(
	SOCKET s,	/* socket */
	char *buf,	/* pointer to receive buffer */
	int bufLen	/* size of receive buffer */
)
{
	int bytesReceived = 0;
	int index = 0;

	do {
		/* normally, the entire message is received at once, but
		* large messages can be fragmented */
		bytesReceived = recv(s, &buf[index], bufLen - index, 0);
		if (bytesReceived < 0)
		{
			_ERROR("recv error %d\n", WSAGetLastError());
			return 0;
		}
		index += bytesReceived;
		if ((bufLen - index) <= 0)
		{
			_ERROR("recv error - buffer overflow\n");
			return 0;
		}
	} while (buf[index - 1] != '\0');	/* defragment until string terminator found */
	return index;
}

/***************************************************************************
* Name: main
*
* Routine Description: This function is the main routine for the mavenAdmin
*	process.  It creates a server and client socket.  The client connects
*	to the mavenClient process.  The server socket connects to the Admin
*	process.  Once the sockets connect, message traffic received on the
*	client socket is forwarded to the server socket.
*
* Returns: -1
*
**************************************************************************/
int main()
{
	char serverRecvbuf[PACKETDUMP_MSGSIZE];
	WSADATA wsaData;
	char *clientName = NULL;
	int iResult;
	struct addrinfo *result = NULL;
	HRESULT hResult = S_OK;
	struct addrinfo hints;
	struct addrinfo *ptr = NULL;

	hStopEvent = CreateEvent(NULL, TRUE, FALSE, L"Network");

	if (!SetConsoleCtrlHandler(&ConsoleCtrlEventHandler, TRUE))
	{
		_ERROR("***unable to register termination handler %d\n", GetLastError());
		Sleep(3000);	/* so user can read message */
		return 1;
	}

	PRINT_DBG("clientSocketThread - Initialize Winsock\n");
	/*  Initialize Winsock */
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		_ERROR("clientSocketThread WSAStartup failed with error: %d\n", iResult);
		return -1;
	}

	/* create server socket */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	/* Resolve the maven server address and port */
	iResult = getaddrinfo(NULL, MAVEN_PORT_NUMBER, &hints, &result);
	if (iResult != 0) {
		_ERROR("clientSocketThread - getaddrinfo failed with error: %d\n", iResult);
		goto ClientSocket_Exit;
	}

	PRINT_DBG("clientSocketThread Create a SOCKET for connecting to mavenClient\n");
	/* Attempt to connect to an address until one succeeds */
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		PRINT_DBG(".");
		/* Create a SOCKET for connecting to server */
		mavenSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (mavenSocket == INVALID_SOCKET) {
			_ERROR("****clientSocketThread unable to create\n");
			WSACleanup();
			hResult = NS_E_SERVER_UNAVAILABLE;
			goto ClientSocket_Exit;
		}

		/* Connect to server. */
		iResult = connect(mavenSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(mavenSocket);
			mavenSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (iResult == SOCKET_ERROR) {
		_ERROR("****clientSocketThread unable to connect socket\n");
		goto ClientSocket_Exit;
	}
	PRINT_DBG("clientSocketThread Create a SOCKET for connecting to server thread\n");

	/* create client socket */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	/* Resolve the admin server address and port */
	iResult = getaddrinfo(NULL, MAVEN_ADMIN_PORT_NUMBER, &hints, &result);
	if (iResult != 0) {
		_ERROR("clientSocketThread - getaddrinfo failed with error: %d\n", iResult);
		goto ClientSocket_Exit;
	}

	PRINT_DBG("clientSocketThread connect 0x%llx, 0x%llx\n", adminSocket, mavenSocket);
	/* Attempt to connect to an address until one succeeds */
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		PRINT_DBG(".");
		/* Create a SOCKET for connecting to server */
		adminSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (adminSocket == INVALID_SOCKET) {
			_ERROR("clientSocketThread - socket call failed\n");
			WSACleanup();
			hResult = NS_E_SERVER_UNAVAILABLE;
			goto ClientSocket_Exit;
		}

		/* Connect to server. */
		iResult = connect(adminSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(adminSocket);
			adminSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	PRINT_DBG("clientSocketThread Send a connect message to 0x%llx\n", mavenSocket);
	iResult = send(mavenSocket, CLIENT_ID_STRING VERSION_STRING "\n", (int)strlen(CLIENT_ID_STRING VERSION_STRING) + 2, 0);
	if (iResult == SOCKET_ERROR) {
		hResult = NS_E_INTERNAL_SERVER_ERROR;
		_ERROR("clientSocketThread - send failed with error: %d\n", WSAGetLastError());
		goto ClientSocket_Exit;
	}

	freeaddrinfo(result);

	while (1)
	{
		PRINT_DBG("clientSocketThread - receive\n");
		iResult = recvMsg(mavenSocket, serverRecvbuf, PACKETDUMP_MSGSIZE);
		/* if we have a socket error */
		if (iResult <= 0)
		{
			break;
		}

		PRINT_DBG("clientSocketThread forward message to admin\n");
		iResult = send(adminSocket, serverRecvbuf, (int)strlen(serverRecvbuf) + 1, 0);
		if (iResult == SOCKET_ERROR) {
			hResult = NS_E_INTERNAL_SERVER_ERROR;
			_ERROR("clientSocketThread - send failed with error: %d\n", WSAGetLastError());
			goto ClientSocket_Exit;
		}

	}

ClientSocket_Exit:
	WSACleanup();
	if (mavenSocket != INVALID_SOCKET)
	{
		closesocket(mavenSocket);
	}
	if (adminSocket != INVALID_SOCKET)
	{
		closesocket(adminSocket);
	}
	return -1;
}
