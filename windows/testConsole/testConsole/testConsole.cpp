// testConsole.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#define CONSOLE_LINE_SIZE 132
#define SERVER_NAME_SIZE 132

#undef MAVEN_DEBUG
#ifdef MAVEN_DEBUG
#define PRINT_DBG printf
#define WPRINT_DBG wprintf
#else
#define PRINT_DBG(...)
#define WPRINT_DBG(...)
#endif

static char fileShortcuts[SERVER_NUM_CMDS] = {
	'f','l','i','u','d','r','s','v','g','x' };

static char netShortcuts[SERVER_NET_NUM_CMDS] = {
	'f','i','x','s'};

static const char *fileServerSring[SERVER_NUM_CMDS] = {
	SERVER_CMD_FILTER, SERVER_CMD_LEARN, SERVER_CMD_IDLE,
	SERVER_CMD_READWHITELIST, SERVER_CMD_WRITEWHITELIST,
	SERVER_CMD_RESET, SERVER_CMD_STATUS, SERVER_CMD_READVERSION,
	SERVER_CMD_READLOG, SERVER_CMD_EXIT
};

static const char *netServerSring[SERVER_NET_NUM_CMDS] = {
	SERVER_NET_CMD_FILTER, SERVER_NET_CMD_IDLE,
	SERVER_NET_CMD_EXIT, SERVER_NET_CMD_STATUS
};

#define MODULE_NAME "testConsole"
/***************************************************************************
* Name:recvMsg
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
			fprintf(stderr, MODULE_NAME " recv error %d\n", WSAGetLastError());
			return 0;
		}
		index += bytesReceived;
		if ((bufLen - index) <= 0)
		{
			return 0;
		}
	} while (buf[index - 1] != '\0');	/* defragment until string terminator found */
	return index;
}

/***************************************************************************
* Name: printHelp
*
* Routine Description: This function prints a help screen.
*
* Returns:  N/A
*
**************************************************************************/
void printHelp(void)
{
	printf("Valid Commands are:\n");
	printf("   fltCmdFilter	(ff)		- Set netfilter filter mode\n");
	printf("   fltCmdLearn	(fl)		- Set netfilter learn mode\n");
	printf("   fltCmdIdle (fi)			- Set netfilter idle mode\n");
	printf("   fltCmdReadWhitelist (fu)	- upload netfilter config file from driver\n");
	printf("   fltCmdWriteWhitelist (fd)- download netfilter config file to driver\n");
	printf("   fltCmdReset (fr)			- reset netfilter configuration\n");
	printf("   fltCmdStatus	(fs)		- Show netfilter driver state\n");
	printf("   fltCmdReadVersion (fv)	- get netfilter version\n");
	printf("   fltCmdReadLog (fg)		- get netfilter log entry \n");
	printf("   fltCmdExit (fx)			- exit netfilter\n");
	printf("   netCmdFilter (nf)		- netFilter to filter mode\n");
	printf("   netCmdIdle (ni)			- set netFilter to idle mode\n");
	printf("   netCmdExit (nx),			- exit netFilter\n");
	printf("   netCmdStatus (ns),		- read netFilter status from driver\n");
}
DWORD WINAPI ConsoleThread(LPVOID id)
{
	SOCKET serverSocket = *((SOCKET *)id);
	char str[PACKETDUMP_MSGSIZE];
	DWORD recvLength;

	if (serverSocket == INVALID_SOCKET)
	{
		fprintf(stderr, "invalid socket id passed to thread\n");
		return 0;
	}

	while (1)
	{
		recvLength = recvMsg(serverSocket, str, PACKETDUMP_MSGSIZE);
		if (recvLength != 0)
		{
			printf(">(%d) %s", recvLength, str);
		}
	}
	return 0;
}
int main(int argc, char **argv)
{
	WSADATA wsaData;
	int wsaError = 0;
	char *clientName = NULL;
	char commandLine[CONSOLE_LINE_SIZE], *pCommandLine;
	int iResult, i;
	SOCKET serverSocket = INVALID_SOCKET;
	struct sockaddr_in server;
	struct addrinfo *result = NULL;
	HANDLE hConsoleThread;
	DWORD ConsoleThreadId;

	PRINT_DBG("Console Worker entry\n");

	if (argc > 2)
	{
		fprintf(stderr, "Incorrect number of arguments e.g.\n");
		fprintf(stderr, "  testConsole \"192.168.56.102\"\n");
		Sleep(3000);
		return -1;
	}
	if (argc == 2)
	{
		clientName = argv[1];
	}

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}


	PRINT_DBG("Console Worker - server name %s\n", clientName);

	inet_pton(AF_INET, clientName, &server.sin_addr.s_addr);
	//server.sin_addr.s_addr = inet_addr(clientName);
	server.sin_family = AF_INET;
	server.sin_port = htons(MAVEN_PORT_NUMBER_BIN);
	
	PRINT_DBG("Console Worker - Attempt to connect to an address until one succeeds\n");

	// Create a SOCKET for connecting to client
	serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET) {
		fprintf(stderr, "worker socket failed with error: %ld\n", WSAGetLastError());
		goto testConsoleExit;
	}
	PRINT_DBG("Console Worker - Connect to client\n");
	// Connect to client.
	iResult = connect(serverSocket, (SOCKADDR *)&server, sizeof(server));
	if (iResult == SOCKET_ERROR) {
		closesocket(serverSocket);
		serverSocket = INVALID_SOCKET;
	}

	if (serverSocket == INVALID_SOCKET) {
		fprintf(stderr, "Console Worker unable to connect to client %s\n", clientName);
		goto testConsoleExit;
	}

	Sleep(1);	/* let client accept */
	PRINT_DBG("Console Worker - CreateThread\n");
	/* This thread is used for testing.  In a deployed system, the admin server would connect */
	hConsoleThread = CreateThread(
		NULL, 
		0,
		&ConsoleThread,
		&serverSocket,
		0,
		&ConsoleThreadId);

	sprintf_s(commandLine, sizeof(commandLine), SERVER_ID_STRING);

	PRINT_DBG("Console Worker - sending command %s\n", commandLine);
	// forward command to client who sends it to minifilter
	iResult = send(serverSocket, commandLine, (int)strlen(commandLine) + 1, 0);
	if (iResult == SOCKET_ERROR) {
		printf("Console Worker - send failed with error: %d\n", WSAGetLastError());
	}


	/* print help */
	printHelp();

	PRINT_DBG("Console Worker - get console commands\n");
	/* get console commands, this is a string terminated with \n\0
	* It can be either a two character shortcut or a full command.
	* In either case, it may optionally have a argument */
	while (fgets(commandLine, CONSOLE_LINE_SIZE, stdin) != NULL)
	{
		size_t fullCmdLen;
		char fullCommandLine[CONSOLE_LINE_SIZE];
		size_t cmdLen = strlen(commandLine);
		if ((cmdLen == 1) && (commandLine[0] == '\n'))
			continue; /* empty line */
		pCommandLine = &commandLine[0];

		/* check for empty line */
		if (commandLine[0] == '\n')
		{
			continue;
		}

		/* check for help request */
		if ((cmdLen == 2) && (commandLine[0] == 'h'))
		{
			printHelp();
		}

		/* check for shortcut
		 * if the command line is at least two characters and the first
		 * one indicates fileFilter and the command is two characters */
		if ((cmdLen > 2) && (commandLine[0] == 'f') &&
			((commandLine[2] == '\n') || (commandLine[2] == ' ')))
		{
			/* lookup full command string using shortcut */
			for (i = 0; i < SERVER_NUM_CMDS; i++) {
				if (commandLine[1] == fileShortcuts[i])
					break;
			}
			if (i == SERVER_NUM_CMDS)
			{
				printf("Invalid shortcut\n");
				continue;
			}
			fullCmdLen = strlen(fileServerSring[i]);
			/* form the full command string */
			memcpy(fullCommandLine, fileServerSring[i], fullCmdLen + 1);
			/* if any arguments to send with */
			if (cmdLen > 3)
			{
				memcpy(&fullCommandLine[fullCmdLen], &commandLine[2], cmdLen - 1);
			}
			/* we'll be sending the full command line */
			pCommandLine = fullCommandLine;
		}

		/*if the command line is at least two characters and the first
		 * one indicates netFilter and the command is two characters */
		if ((cmdLen > 2) && (commandLine[0] == 'n') &&
			((commandLine[2] == '\n') || (commandLine[2] == ' ')))
		{
			/* lookup full command string using shortcut */
			for (i = 0; i < SERVER_NUM_CMDS; i++) {
				if (commandLine[1] == netShortcuts[i])
					break;
			}
			if (i == SERVER_NUM_CMDS)
			{
				printf("Invalid shortcut\n");
				continue;
			}
			fullCmdLen = strlen(fileServerSring[i]);
			/* form the full command string */
			memcpy(fullCommandLine, netServerSring[i], fullCmdLen + 1);
			/* if any arguments to send with */
			if (cmdLen > 3)
			{
				memcpy(&fullCommandLine[fullCmdLen], &commandLine[2], cmdLen - 1);
			}
			/* we'll be sending the full command line */
			pCommandLine = fullCommandLine;
		}

		/* we don't want to send a newline terminator with command */
		if (pCommandLine[strlen(pCommandLine) - 1] == '\n')
		{
			/* delete newline from string */
			pCommandLine[strlen(pCommandLine) - 1] = '\0';
		}

		PRINT_DBG("Console Worker - sending command %s\n", pCommandLine);
		// forward command to client who sends it to minifilter
		iResult = send(serverSocket, pCommandLine, (int)strlen(pCommandLine) + 1, 0);
		if (iResult == SOCKET_ERROR) {
			printf("Console Worker - send failed with error: %d\n", WSAGetLastError());
			break;
		}

	}
testConsoleExit:
	PRINT_DBG("Console Worker - exit\n");
	Sleep(10000);
	if (serverSocket != INVALID_SOCKET)
		closesocket(serverSocket);
	WSACleanup();
	return 0;
}
