/*++

Module Name: mavenClient.cpp

Abstract:

This application collects and distributes maven filter messages.

server code follows the example at http://www.codersource.net/2010/02/17/socket-server-in-winsock-event-object-model/


Environment:

User mode

Revision History:
Author Richard Carter - Siege Technologies, Manchester NH

V1.1	03/15/2018 - REC, written
V1.2	03/21/2018 - REC, Added cleanup routine
V1.3	03/27/2018 - REC, handle fragmented messages
V1.4	05/11/2018 - REC, add C&C socket

--*/

#include "stdafx.h"

#define CONSOLE_LINE_SIZE 132

#undef JSON_DEBUG
#ifdef JSON_DEBUG
#define JSON_PRINT printf
#else
#define JSON_PRINT(...)
#endif


#undef MAVEN_DEBUG
#ifdef MAVEN_DEBUG
#define PRINT_DBG printf
#define WPRINT_DBG wprintf
#else
#define PRINT_DBG(...)
#define WPRINT_DBG(...)
#endif
static const char mavenClientVersion[] = "Maven Client V1.4";


#define _ERROR(fmt,...) fprintf(stderr, "%s line %d " fmt, __FILE__, __LINE__, __VA_ARGS__)

void parseConsoleMsg(char *rcvMsg);
void parseFileFilterMsg(char *rcvMsg);
void parseNetFilterMsg(char *rcvMsg);

HANDLE hStopEvent;
HANDLE NetworkEvent;
HANDLE hServiceThread;
HANDLE hCnCThread;

/* the app maintains a list of sockets and callbacks for the various processes */
#define NUM_CLIENTS 3 /* number of socket connections */

#define SERVER_INDEX 0		/* server process connection for flutentd */
#define FILEFILTER_INDEX 1	/* socket connection for file filtering */
#define NETFILTER_INDEX 2	/* socket connection for network filtering */

extern void parseCncMessage(char *buf);


static void (*ClientThreadFunc[NUM_CLIENTS])(char *rcvMsg) = {
	parseConsoleMsg,
	parseFileFilterMsg,
	parseNetFilterMsg
};
static SOCKET ClientThreadSocket[NUM_CLIENTS] = { INVALID_SOCKET,};
static SOCKET CnCSocket = INVALID_SOCKET;

static const char *clientIdString[NUM_CLIENTS] = {
	CLIENT_ID_STRING,
	FILEFILTER_ID_STRING,
	NETFILTER_ID_STRING
};

CArray<HANDLE, HANDLE> threadArray;

#define MODULE_NAME "mavenClient"

static BOOL fileFilterEnabled = FALSE;
static BOOL netFilterEnabled = FALSE;

/***************************************************************************
* Name: transducerEnabled
*
* Routine Description: This function checks the operating mode of the indexed
*	transducer.
*
* Returns: True if in Filter mode.
*
**************************************************************************/
BOOL transducerEnabled(int transducer)
{
	if (transducer == TRANSDUCER_FILEFILTER)
	{
		JSON_PRINT("transducerEnabled %d, %d\n", transducer, fileFilterEnabled);
		return fileFilterEnabled;
	}
	if (transducer == TRANSDUCER_NETFILTER)
	{
		JSON_PRINT("transducerEnabled %d, %d\n", transducer, netFilterEnabled);
		return netFilterEnabled;
	}
	return FALSE;	/* bad argument */
}

/***************************************************************************
* Name: transducerSetModeFlag
*
* Routine Description: This function sets the internal mode flag that keeps
*	track of the transducer flag.
*
* Returns: N/A
*
**************************************************************************/
void transducerSetModeFlag(int transducer, BOOL enable)
{
	JSON_PRINT("transducerSetModeFlag %d, %d\n", transducer, enable);
	if (transducer == TRANSDUCER_FILEFILTER)
	{
		fileFilterEnabled = enable;
	}
	if (transducer == TRANSDUCER_NETFILTER)
	{
		netFilterEnabled = enable;
	}

}

/* sets filter mode if enable==TRUE filter, else idle */
/***************************************************************************
* Name: transducerSetMode
*
* Routine Description: This function sets the operating mode of the indexed
*	transducer.
*
* Returns: N/A
*
**************************************************************************/
void transducerSetMode(int transducer, BOOL enable)
{
	const char *cmd = NULL;
	const char *statusCmd = NULL;
	int myId = 0;
	JSON_PRINT("transducerSetMode %d, %d\n", transducer, enable);
	if (transducer == TRANSDUCER_FILEFILTER)
	{
		myId = FILEFILTER_INDEX;
		statusCmd = SERVER_CMD_STATUS;
		if (enable)
			cmd = SERVER_CMD_FILTER;
		else
			cmd = SERVER_CMD_IDLE;
	}
	if (transducer == TRANSDUCER_NETFILTER)
	{
		myId = NETFILTER_INDEX;
		statusCmd = SERVER_NET_CMD_STATUS;
		if (enable)
			cmd = SERVER_NET_CMD_FILTER;
		else
			cmd = SERVER_NET_CMD_IDLE;
	}

	if (myId == 0)
		return; /* bad transducer number */

	/* set filefilter transducer state */
	JSON_PRINT("    send[%d] %s \n", myId, cmd);
	(void)send(ClientThreadSocket[myId], cmd,
		(int)strlen(cmd) + 1, 0);

	Sleep(1); /* let command complete */

	/* read new transducer state */
	(void)send(ClientThreadSocket[myId], statusCmd,
		(int)strlen(statusCmd) + 1, 0);

	Sleep(100); /* let command complete */
}

/* returns true if unreachable */
/***************************************************************************
* Name: transducerError
*
* Routine Description: This function checks the socket descriptor for
*	the indexed transducer.
*
* Returns: returns TRUE if the socket is invalid.
*
**************************************************************************/
BOOL transducerError(int transducer)
{
	if (transducer == TRANSDUCER_FILEFILTER)
		return (ClientThreadSocket[FILEFILTER_INDEX] == INVALID_SOCKET);
	if (transducer == TRANSDUCER_NETFILTER)
		return (ClientThreadSocket[NETFILTER_INDEX] == INVALID_SOCKET);
	return TRUE; /* parse error */
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
* Name: parseConsoleMsg
*
* Routine Description: This function receives messages from the Admin or
*	Maven sockets.  It determines which output port to forward the message
*	to by examining the first word for a match.  It forwards the message
*	to the appropriate port.
*
* Returns: N/A
*
**************************************************************************/
void parseConsoleMsg(char *rcvMsg)
{
	int iResult;
	HRESULT hResult = S_OK;
	SOCKET destinationSocket = INVALID_SOCKET;

	PRINT_DBG("parseConsoleMsg %s\n", rcvMsg);

	if ((strncmp(rcvMsg, SERVER_CMD_FILE_PREFIX, strlen(SERVER_CMD_FILE_PREFIX)) == 0) ||
		(strncmp(rcvMsg, SERVER_CMD_NET_PREFIX, strlen(SERVER_CMD_NET_PREFIX)) == 0))
	{
		/* command to file filter app */ 
		int i;
		/* find the correct socket */
		for (i = 0; i < NUM_CLIENTS; i++)
		{
			/* the first word of each message must match one of those
			 * in the clientIdString array */
			if (strncmp(rcvMsg, clientIdString[i],
				strlen(clientIdString[i])) == 0)
			{
				destinationSocket = ClientThreadSocket[i];
				break;
			}
		}
		if (i == NUM_CLIENTS)
		{
			_ERROR("Unable to find socket for %s\n", clientIdString[i]);
			goto parseConsoleMsgExit;
		}
		PRINT_DBG("sending message to %d, %s\n", i, clientIdString[i]);
		iResult = send(destinationSocket, rcvMsg,
			(int)strlen(rcvMsg) + 1, 0);
		if (iResult == SOCKET_ERROR) 
		{
			hResult = NS_E_INTERNAL_SERVER_ERROR;
			PRINT_DBG("send to %s failed with error: %d\n",
				clientIdString[i], WSAGetLastError());
		}
		goto parseConsoleMsgExit;

	}
	/* the code block above should handle valid messages except the connect message */
	if ((strncmp(rcvMsg, CONSOLE_ID_STRING, strlen(CONSOLE_ID_STRING)) == 0) ||
		(strncmp(rcvMsg, SERVER_ID_STRING, strlen(SERVER_ID_STRING)) == 0))
	{
		/* connect message, ignore */
		goto parseConsoleMsgExit;
	}
	else
	{
		_ERROR("invalid command %s\n", rcvMsg);
	}

parseConsoleMsgExit:
	return;
}
/***************************************************************************
* Name: parseFileFilterMsg
*
* Routine Description: This function handles messages received from the file
*	filter.  It forwards them to both Admin and Maven sockets.
*
* Returns: N/A
*
**************************************************************************/
void parseFileFilterMsg(char *rcvMsg)
{
	if (ClientThreadSocket[SERVER_INDEX] != INVALID_SOCKET) 
	{
		int iResult;
		int WsaError;
		size_t strLen1,strLen2;

		/* if status message 
			status message format:
			FileFilter Status      Mode = Filter, Log Size = 0 Time=  129.90 totalEvents = 21
		*/
		strLen1 = min(strlen(rcvMsg), strlen("FileFilter Status Mode = Filter"));
		strLen2 = min(strlen(rcvMsg), strlen("FileFilter Status Mode = "));
		JSON_PRINT("parseFileFilterMsg - comparing \n%s / \nFileFilter Status       Mode = Filter\n", rcvMsg);
		/* update status, used by C&C socket interface */
		if (!strncmp(rcvMsg, "FileFilter Status Mode = Filter", strLen1))
		{
			JSON_PRINT("parseFileFilterMsg - finds filefilter enabled\n");
			transducerSetModeFlag(TRANSDUCER_FILEFILTER, TRUE);
		}
		else if (!strncmp(rcvMsg, "FileFilter Status Mode = ", strLen2))
		{
			JSON_PRINT("parseFileFilterMsg - finds filefilter disabled\n");
			transducerSetModeFlag(TRANSDUCER_FILEFILTER, FALSE);
		}

		iResult = send(ClientThreadSocket[SERVER_INDEX], rcvMsg, static_cast<int>(strlen(rcvMsg)) + 1, 0);
		if (iResult == SOCKET_ERROR)
		{
			WsaError = WSAGetLastError();
			_ERROR("server socket%d send failure %ld\n", SERVER_INDEX, WsaError);

			/* if port closed */
			if (WsaError == WSAENOTSOCK)
			{
				ClientThreadSocket[SERVER_INDEX] = INVALID_SOCKET;
			}
		}
	}
}

/***************************************************************************
* Name: parseNetFilterMsg
*
* Routine Description: This function handles messages received from the net
*	filter.  It forwards them to both Admin and Maven sockets.
*
* Returns: N/A
*
**************************************************************************/
void parseNetFilterMsg(char *rcvMsg)
{
	if (ClientThreadSocket[SERVER_INDEX] != INVALID_SOCKET)
	{
		int iResult;
		size_t strLen1, strLen2;

		/* if status message
		status message format:
		NetFilter Status Mode = Filter	*/
		JSON_PRINT("parseNetFilterMsg - comparing \n'%s' / \n'NetFilter Status Mode = Filter'\n", rcvMsg);
		strLen1 = min(strlen(rcvMsg), strlen("NetFilter Status Mode = Filter"));
		strLen2 = min(strlen(rcvMsg), strlen("NetFilter Status Mode = "));
		/* update status, used by C&C socket interface */
		if (!strncmp(rcvMsg, "NetFilter Status Mode = Filter", strLen1))
		{
			JSON_PRINT("parseNetFilterMsg - finds netfilter enabled\n");
			transducerSetModeFlag(TRANSDUCER_NETFILTER, TRUE);
		}
		else if (!strncmp(rcvMsg, "NetFilter Status Mode = ", strLen2))
		{
			JSON_PRINT("parseNetFilterMsg - finds netfilter disabled\n");
			transducerSetModeFlag(TRANSDUCER_NETFILTER, FALSE);
		}
		iResult = send(ClientThreadSocket[SERVER_INDEX], rcvMsg, static_cast<int>(strlen(rcvMsg)) + 1, 0);
		if (iResult == SOCKET_ERROR)
		{
			_ERROR("server socket%d send failure %ld\n", SERVER_INDEX, WSAGetLastError());
		}
	}
}

/***************************************************************************
* Name: ServerportThread
*
* Routine Description: This function handles server socket messages.
*
* Returns: 0
*
**************************************************************************/
DWORD WINAPI ServerportThread(LPVOID thread_data)
{
	DWORD Event;
	SOCKET sServerport;
	WSANETWORKEVENTS NetworkEvents;
	char str[PACKETDUMP_MSGSIZE];
	DWORD recvLength;

	PRINT_DBG("ServerportThread\n");
	sServerport = (SOCKET &)thread_data;

	while (TRUE)
	{
		int myId;

		/* wait for a socket event */
		if ((Event = WSAWaitForMultipleEvents(1, &NetworkEvent, FALSE, WSA_INFINITE, FALSE)) == WSA_WAIT_FAILED)
		{
			_ERROR("WSAWaitForMultipleEvents failed with error %d\n", WSAGetLastError());
			return 0;
		}

		PRINT_DBG("ServerportThread WSAEnumNetworkEvents\n");
		if (WSAEnumNetworkEvents(sServerport, NetworkEvent, &NetworkEvents) == SOCKET_ERROR)
		{
			printf("WSAEnumNetworkEvents failed with error %d\n", WSAGetLastError());
			return 0;
		}

		/* message read event */
		if (NetworkEvents.lNetworkEvents & FD_READ)
		{
			if (NetworkEvents.lNetworkEvents & FD_READ && NetworkEvents.iErrorCode[FD_READ_BIT] != 0)
			{
				_ERROR("FD_READ failed with error %d\n", NetworkEvents.iErrorCode[FD_READ_BIT]);
			}
			else
			{
				/* read message */
				recvLength = recvMsg(sServerport, str, PACKETDUMP_MSGSIZE);

				if (recvLength == 0)
				{
					continue;
				}

				/* find associated handler */
				for (myId = 0; myId < NUM_CLIENTS; myId++)
				{
					if (ClientThreadSocket[myId] == sServerport)
					{
						PRINT_DBG("ServerportThread servicing %d\n", myId);
						break;
					}

				}

				/* if this is a new connection.  Connections are received in a random
				 * order.  We must identify the sender and associate the socket to it */
				if (myId == NUM_CLIENTS)
				{
					/* figure out who we are */
					for (myId = 0; myId < NUM_CLIENTS; myId++)
					{
						/* connect messages have an ID string */
						if ((clientIdString[myId] != NULL) &&
							(strncmp(str, clientIdString[myId], strlen(clientIdString[myId])) == 0))
						{
							printf("connected to client %s\n", clientIdString[myId]);
							ClientThreadSocket[myId] = sServerport;
							break; 
						}
					}
					/* unrecognized connect */
					if (myId == NUM_CLIENTS)
					{
						/* make sure string terminates, shouldn't be longer than ID string */
						if (recvLength > 10)
						{
							str[10] = '\0';
						}
						_ERROR("invalid message target %s\n", str);
					}

					/* when connecting to fileFilter, request status */
					if (myId == FILEFILTER_INDEX)
					{
						(void)send(ClientThreadSocket[myId],
							SERVER_CMD_STATUS,
							(int)strlen(SERVER_CMD_STATUS) + 1, 0);
					}

					/* when connecting to netFilter, request status */
					if (myId == NETFILTER_INDEX)
					{
						(void)send(ClientThreadSocket[myId],
							SERVER_NET_CMD_STATUS,
							(int)strlen(SERVER_NET_CMD_STATUS) + 1, 0);
					}
					continue; /* don't process new connection message */
				}
				
				PRINT_DBG("Parsing to client function%d %s, %s\n", myId, clientIdString[myId], str);

				/* if we found the appropriate socket */
				if (ClientThreadSocket[myId] != INVALID_SOCKET)
				{
					/* process the message */
					ClientThreadFunc[myId](str);
				}
				else
				{
					_ERROR("uninitialize message target %s\n", 
						clientIdString[myId]);
				}
			}
		}

		/* socket close event  */
		if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			shutdown(sServerport, FD_READ | FD_WRITE);
			closesocket(sServerport);
			ExitThread(0);
		}
		if (NetworkEvents.lNetworkEvents & !(FD_READ | FD_CLOSE)) 
		{
			_ERROR("unhandled event 0x%x\n", NetworkEvents.lNetworkEvents);
		}
	} /* end while (TRUE) */

	SetEvent(hStopEvent); /* should never get here */
	return 0;
}


/***************************************************************************
* Name: ServiceThread
*
* Routine Description: This function creates a service to handle windows
*	events and then spawns a task to create ports and handle messages.
*
* Returns: 0
*
**************************************************************************/
DWORD WINAPI ServiceThread(LPVOID thread_data)
{
	SOCKET sService, sServerport;
	SOCKADDR_IN client;
	int iAddrSize;
	DWORD dwThreadId;
	HANDLE hServerportThread;

	PRINT_DBG("ServiceThread\n");
	NetworkEvent = WSACreateEvent();

	sService = (SOCKET &)thread_data;
	if (listen(sService, 5))
	{
		printf("listen() failed with error %d\n", WSAGetLastError());
		return 0;
	}

	while (TRUE)
	{
		iAddrSize = sizeof(client);
		PRINT_DBG("ServiceThread - waiting to accept\n");
		sServerport = accept(sService, (struct sockaddr *)&client,
			&iAddrSize);
		if (sServerport == INVALID_SOCKET)
		{
			printf("accept() failed: %d\n", WSAGetLastError());
			break;
		}

		if (WSAEventSelect(sServerport, NetworkEvent, FD_READ | FD_CLOSE) == SOCKET_ERROR)
		{
			printf("Error in Event Select\n");
			break;
		}

		hServerportThread = CreateThread(NULL, 0, &ServerportThread, (LPVOID)sServerport, 0, &dwThreadId);
		if (hServerportThread == NULL)
		{
			printf("CreateThread() failed: %d\n", GetLastError());
			break;
		}
		else
			threadArray.Add(hServerportThread);
	}
	return 0;
}


/***************************************************************************
* Name: CnCThread
*
* Routine Description: This function handles the C&C message port.
*
* Returns: 0
*
**************************************************************************/
DWORD WINAPI CnCThread(LPVOID thread_data)
{
	SOCKET sService;
	int iResult, bytesReceived;
	char buf[PAYLOAD_MSG_SZ];
	sService = (SOCKET &)thread_data;

	iResult = listen(sService, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("CnCThread listen failed with error: %d\n", WSAGetLastError());
		closesocket(sService);
		WSACleanup();
		return 1;
	}

	do {
		// Accept a client socket
		CnCSocket = accept(sService, NULL, NULL);
		if (CnCSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(sService);
			WSACleanup();
			return 1;
		}

		JSON_PRINT("CnCThread parsing messages\n");
		// Receive until the peer shuts down the connection
		do {
			bytesReceived = recvMsg(CnCSocket, buf, PAYLOAD_MSG_SZ);
			if (bytesReceived == 0) 
			{
				return -1;
			}

			JSON_PRINT("CnCThread receives %s\n", buf);
			parseCncMessage(buf);
			JSON_PRINT("parseCncMessage returns %s\n", buf);

			iResult = send(CnCSocket, buf,
				(int)strlen(buf) + 1, 0);
			if (iResult == SOCKET_ERROR)
			{
				PRINT_DBG("CnCSocket send failed with error: %d\n",
					cWSAGetLastError());
				(void)closesocket(CnCSocket);
				break;
			}


		} while (TRUE);
	} while (TRUE);
	return 0;
}

/***************************************************************************
* Name: StartService
*
* Routine Description: This function creates the sockets, then spawns 
*	processes to handle messages.
*
* Returns: 0 if OK otherwise 1
*
**************************************************************************/
int StartService()
{

	SOCKET ServiceSocket;
	SOCKET CnCServiceSocket;
	SOCKADDR_IN InternetAddr;
	WSADATA wsaData;
	DWORD Ret;
	DWORD dwThreadId;
	STARTUPINFO si = { 0, };
	PROCESS_INFORMATION pi = { 0, };

	wchar_t mavenFilefilterName[] = L"mavenFilefilter.exe";
	wchar_t mavenNetfilterName[] = L"mavenNetfilter.exe";
	wchar_t mavenAdminPortName[] = L"mavenAdminPort.exe";

	PRINT_DBG("StartService\n");

	if ((Ret = WSAStartup(0x0202, &wsaData)) != 0)
	{
		printf("WSAStartup() failed with error %d\n", Ret);
		return 1;
	}

	if ((ServiceSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		printf("socket() failed with error %d\n", WSAGetLastError());
		return 1;
	}
	
#if 0
		// test code
		printf("pause for input ");
		printf("%d", getchar());
#endif
	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(MAVEN_PORT_NUMBER_BIN);

	if (bind(ServiceSocket, (PSOCKADDR)&InternetAddr, sizeof(InternetAddr)) == SOCKET_ERROR)
	{
		printf("bind() failed with error %d\n", WSAGetLastError());
		return 1;
	}


	hServiceThread = CreateThread(NULL, 0, &ServiceThread, (LPVOID)ServiceSocket, 0, &dwThreadId);

	if (hServiceThread == NULL)
	{
		printf("CreateThread(ServiceThread) failed: %d\n", GetLastError());
		return 1;
	}
	Sleep(1); /* let service threads initialize */

	if ((CnCServiceSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		printf("socket() failed with error %d\n", WSAGetLastError());
		return 1;
	}

	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(CNC_PORT_NUMBER_BIN);

	if (bind(CnCServiceSocket, (PSOCKADDR)&InternetAddr, sizeof(InternetAddr)) == SOCKET_ERROR)
	{
		printf("bind() failed with error %d\n", WSAGetLastError());
		return 1;
	}

	/* create thread to process cnc messages */

	hCnCThread = CreateThread(NULL, 0, &CnCThread, (LPVOID)CnCServiceSocket, 0, &dwThreadId);

	if (hCnCThread == NULL)
	{
		printf("CreateThread(CnCThread) failed: %d\n", GetLastError());
		return 1;
	}

#if 1

	PRINT_DBG("Starting mavenAdminPortName process\n");

	si.cb = sizeof(STARTUPINFO);
	if (CreateProcess(mavenAdminPortName, mavenAdminPortName, NULL, NULL, FALSE,
		NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi) == 0)
	{
		_ERROR("unable to start mavenAdminPortName. %d\n", GetLastError());
	}
	
#endif
	PRINT_DBG("Starting mavenFilefilter process\n");

	si.cb = sizeof(STARTUPINFO);
	if (CreateProcess(mavenFilefilterName, mavenFilefilterName, NULL, NULL, FALSE,
		NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi) == 0)
	{
		_ERROR("unable to start mavenFilefilter, launch as adminstrator? %d\n", GetLastError());
	}

	PRINT_DBG("Starting mavenNetfilter process\n");
	si.cb = sizeof(STARTUPINFO);
	if (CreateProcess(mavenNetfilterName, mavenNetfilterName, NULL, NULL, FALSE,
		NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi) == 0)
	{
		_ERROR("unable to start mavenNetfilterName  %d\n", GetLastError());
	}

	return 0;
}

/***************************************************************************
* Name: StopService
*
* Routine Description: This function terminates the process services.
*
* Returns: 0
*
**************************************************************************/
int StopService()
{
	for (int clientCount = 0; clientCount <= threadArray.GetUpperBound(); clientCount++)
	{
		printf("Closing %d\n", clientCount);
		CloseHandle(threadArray.GetAt(clientCount));
	}

	CloseHandle(hServiceThread);
	CloseHandle(hCnCThread);


	return 0;
}

/***************************************************************************
* Name: ConsoleCtrlEventHandler
*
* Routine Description: This function is called when the process exits.  It
*	performs cleanup needed to gracefully exit.
*
* Returns: FALSE
*
**************************************************************************/
BOOL WINAPI ConsoleCtrlEventHandler(DWORD dwCtrlType)
{
	int i;

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
		for (i = 0; i < NUM_CLIENTS; i++)
		{
			if (ClientThreadSocket[i] != INVALID_SOCKET)
			{
				closesocket(ClientThreadSocket[i]);
				ClientThreadSocket[i] = INVALID_SOCKET;

			}
		}
		return FALSE;

	}

	// If it gets this far (it shouldn't), do nothing.
	return FALSE;
}

/***************************************************************************
* Name: main
*
* Routine Description: This function is the main entry routine for the
*	mavenClient.
*
* Returns: 0
*
**************************************************************************/
int main(void)
{

	printf("%s\n", mavenClientVersion);

	hStopEvent = CreateEvent(NULL, TRUE, FALSE, L"Network");

	if (!SetConsoleCtrlHandler(&ConsoleCtrlEventHandler, TRUE))
	{
		_ERROR("unable to register termination handler %d\n", GetLastError());
		Sleep(3000);	/* so user can read message */
		return 1;
	}
	StartService();

	WaitForSingleObject(hStopEvent, INFINITE);
	StopService();


	return 0;
}
