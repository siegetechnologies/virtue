/*++

Module Name: mavenFilefilter.cpp

Abstract:

This application runs in user space to provide monitoring and control of the 
mavenMiniFilter driver.



Environment:

User mode

Revision History:
Author Richard Carter - Siege Technologies, Manchester NH

V1.1	11/05/2017 - REC, written
V1.2	12/12/2017 - REC, Update for customer demo
V1.3	12/16/2017 - REC, Add username to log
V1.4	02/22/2018 - REC, Changed name to mavenFilefilter, use port for user interface
V1.5	03/16/2018 - REC, Bugfixes
V1.6	03/16/2018 - REC, Forward event log to fluentd
V1.7	03/27/2018 - REC, handle fragmented messages

--*/

#include "stdafx.h"

#undef MAVEN_DEBUG
#ifdef MAVEN_DEBUG
#define PRINT_DBG printf
#define WPRINT_DBG wprintf
#else
#define PRINT_DBG(...)
#define WPRINT_DBG(...)
#endif

#define _ERROR(fmt,...) fprintf(stderr, "%s line %d " fmt, __FILE__, __LINE__, __VA_ARGS__)
#define _BYTES_REMAINING (((size_t)serverResponsebuf) + sizeof(serverResponsebuf) - (size_t)pServerResponsebuf)
#define LINE_SIZE 132

static const char mavenFilefilterVersion[] = "Maven File Filter V1.5\0";
static HANDLE mavenMinifilterPort = INVALID_HANDLE_VALUE;
static const char *serverString[SERVER_NUM_CMDS] = {
	SERVER_CMD_FILTER, SERVER_CMD_LEARN, SERVER_CMD_IDLE,
	SERVER_CMD_READWHITELIST, SERVER_CMD_WRITEWHITELIST, 
	SERVER_CMD_RESET, SERVER_CMD_STATUS, SERVER_CMD_READVERSION, 
	SERVER_CMD_READLOG, SERVER_CMD_EXIT
};
static char nullArg[] = "\0";
static SOCKET ConnectSocket = INVALID_SOCKET;

int wideToChar(CHAR* dest, const WCHAR* source);
int SIDtoUsername(PWCHAR outbuf, PWCHAR inbuf, DWORD *pBufLen);
int totalEventCount = 0;

#define VERSION_STRING " Maven File Filter V1.7"

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
	int wsaError;

	do {
		/* normally, the entire message is received at once, but
		* large messages can be fragmented */
		bytesReceived = recv(s, &buf[index], bufLen - index, 0);
		if (bytesReceived < 0)
		{
			wsaError = WSAGetLastError();
			if (wsaError == WSAETIMEDOUT)
			{
				return -1;	/* socket timed out */
			}
			_ERROR("recv error %d\n", wsaError);
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
* Name: eventLogOpen
*
* Routine Description: This function opens the event log
*
* Returns: handle to file
*
**************************************************************************/
HANDLE eventLogOpen(void)
{
	HANDLE eventLogFile = INVALID_HANDLE_VALUE;

	/* open the log file */
	eventLogFile = CreateFile(FILELOG_NAME,
		GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ,
		NULL, // default security
		CREATE_ALWAYS,
		FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_WRITE_THROUGH,
		NULL);

	if (eventLogFile == NULL)
	{
		_ERROR("unable to open file filter log file\n");
	}
	return eventLogFile;
}

/***************************************************************************
* Name: eventLogClose
*
* Routine Description: This function closes the event file
*
* Returns: N/A
*
**************************************************************************/
void eventLogClose(HANDLE eventLogFile)
{
	if (eventLogFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(eventLogFile);
	}
}

/***************************************************************************
* Name: logEvent
*
* Routine Description: This function logs an entry to the event log
*
* Returns: N/A
*
**************************************************************************/
void logEvent(
	double timePassed,
	char *outBuf,
	HANDLE eventLogFile
)
{
	BOOL errorFlag = FALSE;
	DWORD dwBytesToWrite;
	DWORD dwBytesWritten = 0;

	dwBytesToWrite = (DWORD)strlen(outBuf);
	errorFlag = WriteFile(
		eventLogFile,       // open file handle
		outBuf,				// start of data to write
		dwBytesToWrite,		// number of bytes to write
		&dwBytesWritten,	// number of bytes that were written
		NULL);				// no overlapped structure

	if (errorFlag == FALSE)
	{
		_ERROR("logEvent Unable to write to event log file.\n");
	}
	else if (dwBytesWritten != dwBytesToWrite)
	{
		// This is an error because a synchronous write that results in
		// success (WriteFile returns TRUE) should write all data as
		// requested. This would not necessarily be the case for
		// asynchronous writes.
		_ERROR("logEvent error writing to event log file.\n");
	}

}

/***************************************************************************
* Name: statusLogOpen
*
* Routine Description: This function opens the status log file
*
* Returns: handle to file
*
**************************************************************************/
HANDLE statusLogOpen(void)
{
	HANDLE statusLogFile = INVALID_HANDLE_VALUE;

	/* open the log file */
	statusLogFile = CreateFile(FILELOG_NAME,
		GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ,
		NULL, // default security
		CREATE_ALWAYS,
		FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_WRITE_THROUGH,
		NULL);

	if (statusLogFile == NULL)
	{
		_ERROR("unable to open file %ls\n", FILELOG_NAME);
	}
	return statusLogFile;
}

/***************************************************************************
* Name: getEventCount
*
* Routine Description: This function searches the filefilter status string
* for a the event count.  It returns the number of events reported.
*
*  Time=   42.34 Mode = Learn, Log Size = 0
*
* Returns:  N/A
*
*/
int getEventCount(char *outBuf)
{
	int keyFound = 0;
	int returnVal = 0;
	while ((*outBuf != '\n') && (*outBuf != '\0'))
	{
		/* look for =sign */
		if (*outBuf == '=')
		{
			keyFound++;
		}
		/* if we found the second =sign */
		if (keyFound == 2)
		{
				/* convert count from ascii to decimal */
			if ((*outBuf >= '0') && (*outBuf <= '9'))
			{
				returnVal *= 10;
				returnVal += *outBuf - '0';
			}
		}
		/* if we found the third =sign */
		if (keyFound == 3)
		{
			break; /* done */
		}

		outBuf++; /* next char */
	}
	return returnVal;
}

/***************************************************************************
* Name: statusLogClose
*
* Routine Description: This function closes the status log file
*
* Returns: N/A
*
**************************************************************************/
void statusLogClose(HANDLE statusLogFile)
{
	if (statusLogFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(statusLogFile);
	}
}

/***************************************************************************
* Name: logStatus
*
* Routine Description: This function logs status to the filefiler log
*	file. It also reads any events reported and logs and sends messages for
*	each.
*
* Returns: N/A
*
**************************************************************************/
void logStatus(
	int eventCount,
	HANDLE statusLogFile,
	char *pServerResponsebuf
)
{
	CHAR eventMessage[PAYLOAD_MSG_SZ], *pEventMessage;
	MAVEN_RESPONSE_MESSAGE responseMessage;
	MAVEN_CMD_MESSAGE commandMessage;
	DWORD bytesReturned = sizeof(MAVEN_RESPONSE_MESSAGE);
	HRESULT hResult = S_OK;
	BOOL errorFlag = FALSE;
	DWORD dwBytesToWrite;
	DWORD dwBytesWritten = 0;
	int i, j;
	WCHAR userName[MAX_USERNAME_LEN];
	DWORD bufLen;

	PRINT_DBG("mavenFileFilter - logs status %s\n", outBuf);

	dwBytesToWrite = (DWORD)strlen(pServerResponsebuf);
	errorFlag = WriteFile(
		statusLogFile,      // open file handle
		pServerResponsebuf,	// start of data to write
		dwBytesToWrite,		// number of bytes to write
		&dwBytesWritten,	// number of bytes that were written
		NULL);				// no overlapped structure

	if (errorFlag == FALSE)
	{
		_ERROR("logStatus Unable to write to %ls.\n", FILELOG_NAME);
	}
	else if (dwBytesWritten != dwBytesToWrite)
	{
		// This is an error because a synchronous write that results in
		// success (WriteFile returns TRUE) should write all data as
		// requested. This would not necessarily be the case for
		// asynchronous writes.
		_ERROR("logStatus error writing to %ls\n", FILELOG_NAME);
	}

	PRINT_DBG("mavenFileFilter - finds %d events \n", eventCount);
	/* for each event reported */
	for (j = 0; j < eventCount; j++)
	{
		commandMessage.msgType = mavenGetLog;

		/* query the driver for the event message */
		hResult = FilterSendMessage(mavenMinifilterPort,
			&commandMessage,
			sizeof(MAVEN_CMD_MESSAGE),
			&responseMessage,
			sizeof(MAVEN_RESPONSE_MESSAGE),
			&bytesReturned);

		if (responseMessage.msgType != mavenSuccess) {
			return;
		}

		/* id the destination socket */
		pEventMessage = eventMessage;
		pEventMessage += sprintf_s(pEventMessage, sizeof(eventMessage), 
			FILEFILTER_ID_STRING " event(%d) - ", eventCount + j);
		
		/* insert user name in response buffer */

		bufLen = MAX_USERNAME_LEN;	/* max length of username buffer */
		i = SIDtoUsername(userName, responseMessage.payload, &bufLen);
		if (i != 0) {
			(void)wideToChar(pEventMessage, userName);
			pEventMessage += bufLen;
		}
		/* print rest of payload */
		(void)wideToChar(pEventMessage, &responseMessage.payload[i]);

		PRINT_DBG("mavenFileFilter - reports event %s \n", eventMessage);
		/* send to fluentd */
		if (send(ConnectSocket, eventMessage, (int)strlen(eventMessage) + 1, 0) == SOCKET_ERROR) 
		{
			hResult = NS_E_INTERNAL_SERVER_ERROR;
			_ERROR("send failed with error: %d\n", WSAGetLastError());
		}
		dwBytesToWrite = (DWORD)strlen(eventMessage);
		errorFlag = WriteFile(
			statusLogFile,      // open file handle
			eventMessage,		// start of data to write
			dwBytesToWrite,		// number of bytes to write
			&dwBytesWritten,	// number of bytes that were written
			NULL);				// no overlapped structure

		if (errorFlag == FALSE)
		{
			_ERROR("logStatus Unable to write to %ls.\n", FILELOG_NAME);
		}
		else if (dwBytesWritten != dwBytesToWrite)
		{
			// This is an error because a synchronous write that results in
			// success (WriteFile returns TRUE) should write all data as
			// requested. This would not necessarily be the case for
			// asynchronous writes.
			_ERROR("logStatus error writing to %ls\n", FILELOG_NAME);
		}
	} /* for each event reported */
}

/***************************************************************************
* Name: charToWide
*
* Routine Description: This function converts a character string to a 
* wide string
*
* Returns:  number of characters in dest buffer (excluding terminator)
*
**************************************************************************/
int charToWide(WCHAR* dest, const CHAR* source)
{
	int i = 0;

	do {
		dest[i] = (WCHAR)source[i];
	} while (source[i++] != '\0');
	dest[i] = 0; /* terminate */
	return i;
}

/***************************************************************************
* Name: csvToText
*
* Routine Description: This function converts a comma delimited string to
*	a tab delimited string.
*
* Returns: N/A
*
**************************************************************************/
void csvToText(CHAR* buf)
{
	int i = 0;

	while (buf[i] != 0) {
		if (buf[i] == ',') {
			buf[i] = '\t';
		}
		i++;
	}

}

/***************************************************************************
* Name: wideToChar
*
* Routine Description: This function converts a wide character string to a
*	character string.
*
* Returns: number of characters in dest buffer excluding null terminator
*
**************************************************************************/
int wideToChar(CHAR* dest, const WCHAR* source)
{
	int i = 0;

	do {
		dest[i] = (CHAR)source[i];
	} while (source[++i] != '\0');
	dest[i] = 0; /* null terminate */
	return i;
}

/***************************************************************************
* Name: textToCsv
*
* Routine Description: This function converts a tab delimeted file to a
*	comma delimeted file.
*
* Returns: N/A
*
**************************************************************************/
void textToCsv(CHAR* buf)
{
	int i = 0;

	while (buf[i] != 0) {
		if (buf[i] == '\t') {
			buf[i] = ',';
		}
		i++;
	}
}

/***************************************************************************
* Name: SIDtoUsername
*
* Routine Description: This function extracts the SID from the beginning
*	of the passed payload and uses it to obtain the user name of the
*	associated process.  It then copies the user name to the output buffer.
*
* Returns:  Number of characters advanced in the passsed string.  Zero if error
*
**************************************************************************/
int SIDtoUsername(
	PWCHAR outbuf,  /* user name */
	PWCHAR inbuf,	/* SID character string */
	DWORD *pBufLen	/* length of name (chars) */
)
{
	BOOL status;
	int i = 0;
	WCHAR stringSecurityDescriptor[PAYLOAD_MSG_SZ];
	WCHAR referencedDomainName[MAX_USERNAME_LEN];
	PSECURITY_DESCRIPTOR pSID;
	DWORD domainnameLen = MAX_USERNAME_LEN;
	SID_NAME_USE eUse = SidTypeUnknown;

	/* copy first part of string, SID, to a new buffer (so we can terminate it) */
	while (TRUE) {
		if (inbuf[i] == '\t') {
			break;
		}
		if (i == (PAYLOAD_MSG_SZ - 1)) {
			return 0;	/* buffer overflow */
		}
		stringSecurityDescriptor[i] = inbuf[i]; /* copy to temporary buffer for later processing */
		i++;
	}
	stringSecurityDescriptor[i] = 0; /* terminate */

	if (ConvertStringSidToSid(stringSecurityDescriptor, &pSID) == 0)
	{
		return 0;	/* unable to convert */
	}

	if (!IsValidSid(pSID)) {
		return 0;	/* unable to convert */
	}


	status = LookupAccountSid(
		NULL,
		pSID,
		outbuf,
		pBufLen,
		referencedDomainName,
		&domainnameLen,
		&eUse);

	if (status == 0)
	{
		PRINT_DBG("LookupAccountSid failed (0x%x\n)", GetLastError());
		return 0;
	}
	/* put a space after the user name */
	outbuf[*pBufLen++] = ' ';

	return i; /* number of characters advanced in input buffer */

}

/***************************************************************************
* Name: parseServerCommand
*
* Routine Description: This function matches the passed string for a match
*	with a predefined set of server commands.
*
* Returns: An enumerated number for the server command and a pointer to
*	the argument list (if present)
*
**************************************************************************/
MAVEN_SERVER_COMMAND parseServerCommand(
	char *serverRecvbuf,
	char **argString)
{
	int i;
	size_t cmdLen;
	*argString = nullArg;
	PRINT_DBG("mavenFileFilter - parsing %s\n", serverRecvbuf);
	for (i = 0; i < SERVER_NUM_CMDS; i++)
	{
		cmdLen = strlen(serverString[i]);
		if (strncmp(serverString[i], serverRecvbuf, cmdLen) == 0) {
			/* argument uses space as field seperator */
			if (serverRecvbuf[cmdLen] == ' ') {
				*argString = serverRecvbuf + cmdLen + 1;
			}
			break; /* found valid command */
		}
	}
	return (MAVEN_SERVER_COMMAND)i;
}

/***************************************************************************
* Name: main
*
* Routine Description: This is the main function for the mavenFilefilter app. 
*	It runs as a shell app.
*
* Returns: 0
*
**************************************************************************/
int main()
{
	HANDLE statusLogFile = INVALID_HANDLE_VALUE;
	HANDLE eventLogFile = INVALID_HANDLE_VALUE;
	HANDLE port = INVALID_HANDLE_VALUE;
	HRESULT hResult = S_OK;
	MAVEN_CMD_MESSAGE commandMessage;
	MAVEN_RESPONSE_MESSAGE responseMessage;
	DWORD bytesReturned = sizeof(MAVEN_RESPONSE_MESSAGE);
	DWORD bufLen;
	CHAR payloadChar[PAYLOAD_MSG_SZ];
	WCHAR userName[MAX_USERNAME_LEN];
	int index, chars, i, eventCount;
	FILE *fptr;

	WSADATA wsaData;
	int socketTimeout = HEARTBEAT_DELAY;
	struct addrinfo *result = NULL;
	struct addrinfo *ptr = NULL;
	struct addrinfo hints;
	int iResult;
	MAVEN_SERVER_COMMAND serverCommand;
	char serverRecvbuf[SERVER_CMD_SIZE];
	char serverResponsebuf[SERVER_RESPONSE_SIZE];
	char *pServerResponsebuf;
	char *argString;

	LARGE_INTEGER base;
	LARGE_INTEGER timestamp;
	LARGE_INTEGER freq;
	double timePassed;

	// Set up timing:
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&base);

#if 0 // to attach debugger early
	printf("mavenFilefilter pauses >");
	printf("%c", getchar()); /* for debug */
#endif
	PRINT_DBG("mavenFileFilter - Open the port\n");

	/* Open the port that is used to talk to filter driver */
	hResult = FilterConnectCommunicationPort(MAVEN_PORT_NAME,
		0,
		NULL,
		0,
		NULL,
		&mavenMinifilterPort);

	if (IS_ERROR(hResult)) {

		_ERROR("Could not connect to filter: 0x%08x\n", hResult);
		goto Main_Exit;
	}

	PRINT_DBG("mavenFileFilter - Initialize Winsock\n");
	/*  Initialize Winsock */
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		_ERROR("WSAStartup failed with error: %d\n", iResult);
		goto Main_Exit;
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	/* Resolve the server address and port */
	iResult = getaddrinfo(NULL, FILEFILTER_PORT_NUMBER, &hints, &result);
	if (iResult != 0) {
		_ERROR("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}
	PRINT_DBG("mavenFileFilter - Attempt to connect to an address \n");
	/* Attempt to connect to an address until one succeeds */
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		PRINT_DBG(".");
		/* Create a SOCKET for connecting to server */
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			WSACleanup();
			hResult = NS_E_SERVER_UNAVAILABLE;
			goto Main_Exit;
		}

		/* Connect to server. */
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		_ERROR("Unable to connect to server!\n");
		PRINT_DBG("cleanup\n");
		WSACleanup();
		hResult = NS_E_SERVER_UNAVAILABLE;
		goto Main_Exit;
	}


	statusLogFile = statusLogOpen();
	eventLogFile = eventLogOpen();

	/* set heartbeat rate */
	PRINT_DBG("set heartbeat\n");
	if (setsockopt(ConnectSocket,
		SOL_SOCKET,
		SO_RCVTIMEO,
		(const char *)&socketTimeout,
		sizeof(socketTimeout)) != 0) {
		hResult = NS_E_INTERNAL_SERVER_ERROR;
		PRINT_DBG("mavenFileFilter - unable to set socket timeout\n");
		goto Main_Exit;
	}

	/* Send a connect message */
	PRINT_DBG("mavenFilefilter Send a connect message\n");
	iResult = send(ConnectSocket, FILEFILTER_ID_STRING VERSION_STRING, 
		(int)strlen(FILEFILTER_ID_STRING) + (int)strlen(VERSION_STRING) + 1, 0);
	if (iResult == SOCKET_ERROR) {
		hResult = NS_E_INTERNAL_SERVER_ERROR;
		PRINT_DBG("mavenFileFilter - send failed with error: %d\n", WSAGetLastError());
		goto Main_Exit;
	}
	Sleep(1000); /* let socket connect */
	PRINT_DBG("mavenFileFilter - Bytes Sent: %ld, 0x%x\n", iResult, hResult);

	while (SUCCEEDED(hResult)) 
	{
		int wsaError = 0;
		pServerResponsebuf = (char *)((size_t)serverResponsebuf); /* start forming output string */

		PRINT_DBG("mavenFileFilter - receive\n");
		iResult = recvMsg(ConnectSocket, serverRecvbuf, SERVER_CMD_SIZE);
		/* compute elapsed time from start(seconds) */
		QueryPerformanceCounter(&timestamp);
		timePassed = (double)(timestamp.QuadPart - base.QuadPart) /
			(double)freq.QuadPart;

		/* if we have a socket error */
		if (iResult <= 0)
		{
			PRINT_DBG("mavenFileFilter - receive socket error %d\n", iResult);
			wsaError = WSAGetLastError();
		}
		if (wsaError == 0)
		{
			/* we received a message from the server */
			serverCommand = parseServerCommand(serverRecvbuf, &argString);
			PRINT_DBG("mavenFileFilter - Bytes received: %d, cmd=%d\n", iResult, serverCommand);
		}
		/* if we have a socket timeout */
		else if ((wsaError == WSAETIMEDOUT) || (wsaError < WSABASEERR))
		{
#if REPORT_STATUS
			/* send heartbeat */
			serverCommand = cmdStatus;
#else
			continue;
#endif
		}
		else
		{
			_ERROR("receive failed with error: %d\n", wsaError);
			//hResult = E_HANDLE;
			//goto Main_Exit;
			continue;
		}

		/* format default error response message */
		pServerResponsebuf += sprintf_s(pServerResponsebuf,
			_BYTES_REMAINING, "%s\t",
			MAVEN_RESPONSE_ERROR);

		PRINT_DBG("mavenFileFilter - dispatch command %d\n", serverCommand);
		/* dispatch command */
		switch (serverCommand) {
		case cmdFilter:
			commandMessage.msgType = mavenModeSetFilter;
			PRINT_DBG("mavenFileFilter - cmdFilter\n");

			hResult = FilterSendMessage(mavenMinifilterPort,
				&commandMessage,
				sizeof(MAVEN_CMD_MESSAGE),
				&responseMessage,
				sizeof(MAVEN_RESPONSE_MESSAGE),
				&bytesReturned);

			if (responseMessage.msgType != mavenSuccess) {
				pServerResponsebuf += sprintf_s(pServerResponsebuf,
					_BYTES_REMAINING,
					"Unable to set Filter mode");
				PRINT_DBG("mavenFileFilter - **** ERROR: Unable to set Filter mode\n");
				goto Send_Cmd_Response;
			}
			pServerResponsebuf = serverResponsebuf; /* start forming output string */
			/* format success response message (overwriting default error) */
			sprintf_s(serverResponsebuf, sizeof(serverResponsebuf),
				"%s\t", MAVEN_RESPONSE_SUCCESS);
			goto Send_Cmd_Response;

		case cmdLearn: /* set Learn mode */
			commandMessage.msgType = mavenModeSetLearn;
			PRINT_DBG("mavenFileFilter - cmdLearn\n");

			hResult = FilterSendMessage(mavenMinifilterPort,
				&commandMessage,
				sizeof(MAVEN_CMD_MESSAGE),
				&responseMessage,
				sizeof(MAVEN_RESPONSE_MESSAGE),
				&bytesReturned);

			if (responseMessage.msgType != mavenSuccess) {
				pServerResponsebuf += sprintf_s(pServerResponsebuf,
					_BYTES_REMAINING,
					"Unable to set Learn mode");
				PRINT_DBG("mavenFileFilter - **** ERROR: Unable to set Learn mode\n");
				goto Send_Cmd_Response;
			}

			/* format success response message (overwriting default error) */
			sprintf_s(serverResponsebuf, sizeof(serverResponsebuf),
				"%s\t", MAVEN_RESPONSE_SUCCESS);
			goto Send_Cmd_Response;

		case cmdIdle: /* set idle mode */
			commandMessage.msgType = mavenModeSetIdle;
			PRINT_DBG("mavenFileFilter - cmdIdle\n");

			hResult = FilterSendMessage(mavenMinifilterPort,
				&commandMessage,
				sizeof(MAVEN_CMD_MESSAGE),
				&responseMessage,
				sizeof(MAVEN_RESPONSE_MESSAGE),
				&bytesReturned);

			if (responseMessage.msgType != mavenSuccess) {
				pServerResponsebuf += sprintf_s(pServerResponsebuf,
					_BYTES_REMAINING,
					"Unable to set Idle mode");
				PRINT_DBG("mavenFileFilter - **** ERROR: Unable to set Idle mode\n");
				goto Send_Cmd_Response;
			}
			/* format success response message (overwriting default error) */
			sprintf_s(serverResponsebuf, sizeof(serverResponsebuf),
				"%s\t", MAVEN_RESPONSE_SUCCESS);
			/* send command response to server */
			goto Send_Cmd_Response;

		case cmdReadWhitelist: /* read whitelist from driver and write to file */
			PRINT_DBG("mavenFileFilter - cmdReadWhitelist %s \n", argString);
			/* this command requires an argument */
			if (strlen(argString) < 2)
			{
				/* send error command response to server */
				goto Send_Cmd_Response;
			}
			/* file name is argument */
			if ((fopen_s(&fptr, argString, "w") != 0) || (fptr == NULL)) {
				pServerResponsebuf += sprintf_s(pServerResponsebuf,
					_BYTES_REMAINING,
					"Unable to open file");
				PRINT_DBG("mavenFileFilter - **** ERROR: Unable to open file\n");
				goto Send_Cmd_Response;
			}

			/* read each record from file and send to driver */
			for (index = 0; index < MAVEN_NUM_PROC; index++) {
				commandMessage.msgType = mavenReadConfig;
				chars = swprintf(commandMessage.payload, PAYLOAD_MSG_SZ, L"%ld", index);

				hResult = FilterSendMessage(mavenMinifilterPort,
					&commandMessage,
					sizeof(MAVEN_CMD_MESSAGE),
					&responseMessage,
					sizeof(MAVEN_RESPONSE_MESSAGE),
					&bytesReturned);

				if (responseMessage.msgType != mavenSuccess) {
					/* end of file */
					break;
				}

				if (bytesReturned == 0) {
					break;
				}
				else {
					pServerResponsebuf = serverResponsebuf; /* start forming output string */
					/* format success response message(overwriting default error) */
					pServerResponsebuf += sprintf_s(serverResponsebuf, sizeof(serverResponsebuf),
							"%s\t", MAVEN_RESPONSE_SUCCESS);
					WPRINT_DBG(L"%s\n", responseMessage.payload);
					/* convert from wide to byte string */
					(void)wideToChar(payloadChar, responseMessage.payload);
					textToCsv(payloadChar);
					fprintf(fptr, "%s\n", payloadChar);
				}
			}
			fclose(fptr);
			goto Send_Cmd_Response;


		case cmdWriteWhitelist: /* read whitelist from file and send to driver */
			PRINT_DBG("mavenFileFilter - cmdWriteWhitelist %s \n", argString);
			/* this command requires an argument */
			if (strlen(argString) < 2)
			{
				/* send error command response to server */
				goto Send_Cmd_Response;
			}
			/* file name is argument */
			if ((fopen_s(&fptr, argString, "r") != 0) || (fptr == NULL)) {
				pServerResponsebuf += sprintf_s(pServerResponsebuf,
					_BYTES_REMAINING,
					"Unable to open file ");
				PRINT_DBG("mavenFileFilter - **** ERROR: Unable to open file\n");
				goto Send_Cmd_Response;
			}
			index = 0;
			while (fgets(payloadChar, PAYLOAD_MSG_SZ, fptr)) {
				PRINT_DBG("sending %s\n", payloadChar);
				csvToText(payloadChar); /* convert comma to tab */
				(void)charToWide(commandMessage.payload, payloadChar);
				commandMessage.msgType = mavenWriteConfig;
				hResult = FilterSendMessage(mavenMinifilterPort,
					&commandMessage,
					sizeof(MAVEN_CMD_MESSAGE),
					&responseMessage,
					sizeof(MAVEN_RESPONSE_MESSAGE),
					&bytesReturned);

				if (responseMessage.msgType != mavenSuccess) {
					pServerResponsebuf += sprintf_s(pServerResponsebuf,
						_BYTES_REMAINING,
						"Unable to write configuration");
					PRINT_DBG("mavenFileFilter - **** ERROR: Unable to write configuration \n");
					fclose(fptr);
					goto Send_Cmd_Response;
				}
			}

			/* end of file, format success response message (overwriting default error) */
			sprintf_s(serverResponsebuf, sizeof(serverResponsebuf),
				"%s\t", MAVEN_RESPONSE_SUCCESS);
			fclose(fptr);
			goto Send_Cmd_Response;

		case cmdReset: /* set reset mode */
			PRINT_DBG("mavenFileFilter - cmdReset\n");
			commandMessage.msgType = mavenModeSetReset;

			hResult = FilterSendMessage(mavenMinifilterPort,
				&commandMessage,
				sizeof(MAVEN_CMD_MESSAGE),
				&responseMessage,
				sizeof(MAVEN_RESPONSE_MESSAGE),
				&bytesReturned);

			if (responseMessage.msgType != mavenSuccess) {
				pServerResponsebuf += sprintf_s(pServerResponsebuf,
					_BYTES_REMAINING,
					"Unable to reset filter");
				PRINT_DBG("mavenFileFilter - **** ERROR: Unable to reset filter\n");
				goto Send_Cmd_Response;
			}
		/* fall through to return status */
		case cmdStatus: /* get Current Operating Mode */
			PRINT_DBG("mavenFileFilter - cmdStatus\n");
			commandMessage.msgType = mavenGetStatus;

			hResult = FilterSendMessage(mavenMinifilterPort,
				&commandMessage,
				sizeof(MAVEN_CMD_MESSAGE),
				&responseMessage,
				sizeof(MAVEN_RESPONSE_MESSAGE),
				&bytesReturned);

			if (responseMessage.msgType != mavenSuccess) {
				pServerResponsebuf += sprintf_s(pServerResponsebuf,
					_BYTES_REMAINING,
					"Unable to get driver status");
				PRINT_DBG("mavenFileFilter - **** ERROR: Unable read current driver state\n");
				goto Send_Cmd_Response;
			}
			pServerResponsebuf = serverResponsebuf; /* start forming output string */
			/* format success response message (overwriting default error) */
			pServerResponsebuf += sprintf_s(serverResponsebuf, SERVER_CMD_SIZE,
				"%s ", MAVEN_RESPONSE_STATUS);
			pServerResponsebuf += wideToChar(pServerResponsebuf, responseMessage.payload);
			pServerResponsebuf -= 1; /* write over \n */
			pServerResponsebuf += sprintf_s(pServerResponsebuf, SERVER_CMD_SIZE - strlen(serverResponsebuf),
				" Time=%8.2f ", timePassed);

			/* check the status message for reported events.  If found, query the filter
			* and report events.
			*/
			eventCount = getEventCount(serverResponsebuf);

			/* log status to file and report each event */
			logStatus(eventCount, statusLogFile, serverResponsebuf);

			pServerResponsebuf -= 1; /* overwrite newline */

			totalEventCount += eventCount;
			sprintf_s(pServerResponsebuf, SERVER_CMD_SIZE - strlen(serverResponsebuf), " totalEvents = %d\n",
				totalEventCount);

			WPRINT_DBG(L"Maven state - %ls\n", responseMessage.payload);
			goto Send_Cmd_Response;

		case cmdReadLog: /* get log entry */
			commandMessage.msgType = mavenGetLog;
			PRINT_DBG("mavenFileFilter - cmdReadLog\n");

			pServerResponsebuf = serverResponsebuf; /* start forming output string */
			pServerResponsebuf += sprintf_s(pServerResponsebuf,
				_BYTES_REMAINING, "%s\t",
				MAVEN_RESPONSE_SUCCESS);

			hResult = FilterSendMessage(mavenMinifilterPort,
				&commandMessage,
				sizeof(MAVEN_CMD_MESSAGE),
				&responseMessage,
				sizeof(MAVEN_RESPONSE_MESSAGE),
				&bytesReturned);

			if (responseMessage.msgType != mavenSuccess) {
				pServerResponsebuf += sprintf_s(pServerResponsebuf,
					_BYTES_REMAINING,
					"Driver log is empty");
				PRINT_DBG("mavenFileFilter - end of driver log\n");
				goto Send_Cmd_Response;
			}
			/* insert user name in response buffer */
			bufLen = MAX_USERNAME_LEN;	/* max length of username buffer */
			i = SIDtoUsername(userName, responseMessage.payload, &bufLen);
			if (i != 0) {
				(void)wideToChar(pServerResponsebuf, userName);
				pServerResponsebuf += bufLen;
			}
			/* print rest of payload */
			(void)wideToChar(pServerResponsebuf, &responseMessage.payload[i]);

			logEvent(timePassed, pServerResponsebuf, eventLogFile);

			/* format command response to server */
			goto Send_Cmd_Response;

		case cmdReadVersion: /* get version */
			commandMessage.msgType = mavenGetVersion;
			PRINT_DBG("mavenFileFilter - cmdReadVersion\n");

			hResult = FilterSendMessage(mavenMinifilterPort,
				&commandMessage,
				sizeof(MAVEN_CMD_MESSAGE),
				&responseMessage,
				sizeof(MAVEN_RESPONSE_MESSAGE),
				&bytesReturned);

			if (responseMessage.msgType != mavenSuccess)
			{
				pServerResponsebuf += sprintf_s(pServerResponsebuf,
					_BYTES_REMAINING,
					"Unable to read driver version");
				PRINT_DBG("mavenFileFilter - **** ERROR: Unable to read driver version\n");
				goto Send_Cmd_Response;
			}
			pServerResponsebuf = serverResponsebuf; /* start forming output string */
			/* format success response message (overwriting default error) */
			pServerResponsebuf += sprintf_s(serverResponsebuf, sizeof(serverResponsebuf),
				"%s  %s  ", MAVEN_RESPONSE_SUCCESS, mavenFilefilterVersion);
			(void)wideToChar(pServerResponsebuf, responseMessage.payload);
			goto Send_Cmd_Response;

		case cmdExit: /* exit */
			PRINT_DBG("mavenFileFilter - cmdExit\n");
			pServerResponsebuf = serverResponsebuf; /* start forming output string */
			pServerResponsebuf += sprintf_s(pServerResponsebuf,
				_BYTES_REMAINING, "%s\t",
				MAVEN_RESPONSE_SUCCESS "mavenFileFilter Exits\n");
			hResult = E_ABORT;
			goto Main_Exit;
			/* fall through */

		case cmdInvalid:
		default:
			PRINT_DBG("mavenFileFilter - invalid command\n");
			/* force a terminator in case the buffer has junk */
			serverRecvbuf[20] = '\0';
			pServerResponsebuf = serverResponsebuf; /* start forming output string */
			pServerResponsebuf += sprintf_s(serverResponsebuf,
				_BYTES_REMAINING, "INVALID COMMAND %s\t", serverRecvbuf);

		Send_Cmd_Response:
			/* Send a response message */
			PRINT_DBG("mavenFileFilter - send server response %s\n", serverResponsebuf);
			iResult = send(ConnectSocket, serverResponsebuf,
				(int)strlen(serverResponsebuf) + 1, 0);
			if (iResult == SOCKET_ERROR)
			{
				_ERROR("send failed with error: %d\n", WSAGetLastError());
				goto Main_Exit;
			}

			PRINT_DBG("mavenFileFilter - (%x) Bytes Sent: %ld\n", hResult, iResult);

			break;
		} /* end switch */
	} /* end while */

Main_Exit:
	PRINT_DBG("mavenFileFilter - Main_Exit\n");
	if (mavenMinifilterPort != INVALID_HANDLE_VALUE) {
		CloseHandle(mavenMinifilterPort);
	}

	if (ConnectSocket != INVALID_SOCKET)
	{
		/*  shutdown the connection */
		iResult = shutdown(ConnectSocket, SD_BOTH);
		if (iResult == SOCKET_ERROR) 
		{
			_ERROR("shutdown failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			hResult = E_UNEXPECTED;
		}
	}

	statusLogClose(statusLogFile);
	eventLogClose(eventLogFile);

	PRINT_DBG("mavenFileFilter - mavenFilefilter exits\n");

	return hResult;
}

