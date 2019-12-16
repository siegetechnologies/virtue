#include "stdafx.h"

#define LINE_SIZE PAYLOAD_MSG_SZ

#undef WINDIVERT_DEBUG
#ifdef WINDIVERT_DEBUG
#define PRINT_DBG printf
#define WPRINT_DBG wprintf
#else
#define PRINT_DBG(...)
#define WPRINT_DBG(...)
#endif

#define _ERROR(fmt,...) fprintf(stderr, "%s line %d " fmt, __FILE__, __LINE__, __VA_ARGS__)

extern 	SOCKET ConnectSocket;
extern int eventCount;		/* number of blocked packets */
int eventThreshold = 1;		/* max blocked packets per heartbeat */

extern packet_metrics_t metrics;

static const char *serverString[SERVER_NET_NUM_CMDS] = {
	SERVER_NET_CMD_FILTER, SERVER_NET_CMD_IDLE, 
	SERVER_NET_CMD_EXIT, SERVER_NET_CMD_STATUS
};
static char nullArg[] = "\0";
static const char *netStateString[2] = { "Idle", "Filter" };

NETFILTER_STATE netState = NETFILTER_FILTER;

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
			_ERROR(" recv error %d\n", WSAGetLastError());
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

#if LOG_STATUS

HANDLE statusLogOpen(void)
{
	HANDLE statusLogFile = INVALID_HANDLE_VALUE;

	/* open the log file */
	statusLogFile = CreateFile(NETLOG_NAME,
		GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ,
		NULL, // default security
		CREATE_ALWAYS,
		FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_WRITE_THROUGH,
		NULL);

	if (statusLogFile == NULL)
	{
		_ERROR("unable to open log file\n");
	}
	return statusLogFile;
}

void statusLogClose(HANDLE statusLogFile)
{
	if (statusLogFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(statusLogFile);
	}
}

void logStatus(
	double timePassed,
	HANDLE statusLogFile
)
{
	char outBuf[LINE_SIZE];
	HRESULT hResult = S_OK;
	BOOL errorFlag = FALSE;
	DWORD dwBytesToWrite;
	DWORD dwBytesWritten = 0;

	/* log metrics to file */
	sprintf_s(outBuf, LINE_SIZE, NETFILTER_ID_STRING " Status Time=%8.2f, "
		"icmpCountInbound %lld, icmpv6CountInbound %lld, "
		"tcpCountInbound %lld, udpCountInbound %lld\n"
		"     icmpCountOutbound %lld, icmpv6CountOutbound %lld, "
		"tcpCountOutbound %lld, udpCountOutbound %lld eventCount=%d, mode=%s\n",
		timePassed,
		metrics.icmpCountInbound, metrics.icmpv6CountInbound,
		metrics.tcpCountInbound, metrics.udpCountInbound,
		metrics.icmpCountOutbound, metrics.icmpv6CountOutbound,
		metrics.tcpCountOutbound, metrics.udpCountOutbound,
		eventCount, netStateString[netState]);

	/* Send a metrics message */
	PRINT_DBG("statusWorkerThread logs message %s", outBuf);

	dwBytesToWrite = (DWORD)strlen(outBuf);
	errorFlag = WriteFile(
		statusLogFile,      // open file handle
		outBuf,				// start of data to write
		dwBytesToWrite,		// number of bytes to write
		&dwBytesWritten,	// number of bytes that were written
		NULL);				// no overlapped structure

	if (errorFlag == FALSE)
	{
		_ERROR("Unable to write to status log file.\n");
	}
	else if (dwBytesWritten != dwBytesToWrite)
	{
		// This is an error because a synchronous write that results in
		// success (WriteFile returns TRUE) should write all data as
		// requested. This would not necessarily be the case for
		// asynchronous writes.
		_ERROR("error writing to status log file.\n");
	}
#if 0
	if (FlushFileBuffers(statusLogFile))
	{
		_ERROR("error flushing status log file.\n");
	}
#endif
}
#endif /* LOG_STATUS */


void sendStatus(
	double timePassed,
	char *pString
)
{
	char outBuf[LINE_SIZE];
	int iResult;
	HRESULT hResult = S_OK;

	/* log metrics to file */
	sprintf_s(outBuf, LINE_SIZE, NETFILTER_ID_STRING " Status Mode = %s, Time=%8.2f, "
		"icmpCountInbound %lld, icmpv6CountInbound %lld, "
		"tcpCountInbound %lld, udpCountInbound %lld\n"
		"     icmpCountOutbound %lld, icmpv6CountOutbound %lld, "
		"tcpCountOutbound %lld, udpCountOutbound %lld eventCount=%d\n",
		netStateString[netState], timePassed,
		metrics.icmpCountInbound, metrics.icmpv6CountInbound,
		metrics.tcpCountInbound, metrics.udpCountInbound,
		metrics.icmpCountOutbound, metrics.icmpv6CountOutbound,
		metrics.tcpCountOutbound, metrics.udpCountOutbound,
		eventCount);
	/* Send a metrics message */
	PRINT_DBG("statusWorkerThread sends message %s", outBuf);
	iResult = send(ConnectSocket, outBuf, (int)strlen(outBuf) + 1, 0);
	if (iResult == SOCKET_ERROR) {
		hResult = NS_E_INTERNAL_SERVER_ERROR;
		_ERROR("send failed with error: %d\n", WSAGetLastError());
	}
}


MAVEN_SERVER_NET_COMMAND parseServerCommand(
	char *serverRecvbuf,
	char **argString)
{
	int i;
	size_t cmdLen;
	*argString = nullArg;
	PRINT_DBG("mavenNetFilter - parsing %s\n", serverRecvbuf);
	for (i = 0; i < SERVER_NET_NUM_CMDS; i++)
	{
		cmdLen = strlen(serverString[i]);
		PRINT_DBG("    comparing (%s) to %s size=%zd\n", serverString[i],
			serverRecvbuf, cmdLen);
		if (strncmp(serverString[i], serverRecvbuf, cmdLen) == 0) {
			/* argument uses space as field seperator */
			if (serverRecvbuf[cmdLen] == ' ') {
				*argString = serverRecvbuf + cmdLen + 1;
			}
			break; /* found valid command */
		}
	}
	return (MAVEN_SERVER_NET_COMMAND)i;
}

DWORD WINAPI clientWorkerThread(LPVOID lpParam)
{
	char outBuf[LINE_SIZE];
	LARGE_INTEGER base;
	LARGE_INTEGER freq;
	int iResult;
	int wsaError = 0;
	MAVEN_SERVER_NET_COMMAND serverCommand;
	char serverRecvbuf[SERVER_CMD_SIZE];
	char serverResponsebuf[SERVER_RESPONSE_SIZE];
	char *pServerResponsebuf;
	char *argString;
	char OKstring[] = "    ";
	char ErrString[] = "****";

	// Set up timing:
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&base);
	pServerResponsebuf = (char *)((size_t)serverResponsebuf); /* start forming output string */

	PRINT_DBG("mavenFileFilter - receive\n");
	while (1) 
	{
		HRESULT hResult = S_OK;
		double timePassed;
		LARGE_INTEGER timestamp;
		char *pString = OKstring;
		int recvIndex = 0;
		if (!recvMsg(ConnectSocket, serverRecvbuf, SERVER_CMD_SIZE))
		{
			return 0;
		}

		/* we received a message from the server */
		serverCommand = parseServerCommand(serverRecvbuf, &argString);
		PRINT_DBG("netfilter clientWorkerThread received %s, serverCommand=%d\n", serverRecvbuf, serverCommand);
		/* dispatch command */
		switch (serverCommand) {
		case netCmdFilter:			/* set mavenNetFilter to filter mode */
			netState = NETFILTER_FILTER;
			PRINT_DBG("netfilter clientWorkerThread processing netCmdFilter\n");
			break;
		case netCmdIdle:			/* set mavenNetFilter to idle mode */
			netState = NETFILTER_IDLE;
			PRINT_DBG("netfilter clientWorkerThread processing netCmdIdle\n");
			break;
		case netCmdExit:			/* exit mavenNetFilter */
			PRINT_DBG("netfilter clientWorkerThread processing netCmdExit\n");
			break;
		case netCmdStatus:			/* read status from driver and send to server */
			PRINT_DBG("netfilter clientWorkerThread processing netCmdStatus\n");
			QueryPerformanceCounter(&timestamp);
			/* compute elapsed time from start(seconds) */
			timePassed = (double)(timestamp.QuadPart - base.QuadPart) /
				(double)freq.QuadPart;
			PRINT_DBG("netfilter clientWorkerThread sending status\n");
			sendStatus(timePassed, pString);
			break;
		case netCmdInvalid:			/* unable to parse */
			_ERROR("netfilter clientWorkerThread - invalid command\n");
			iResult = send(ConnectSocket, outBuf, (int)strlen(outBuf) + 1, 0);
			if (iResult == SOCKET_ERROR) {
				hResult = NS_E_INTERNAL_SERVER_ERROR;
				_ERROR("send failed with error: %d\n", WSAGetLastError());
			}
			sprintf_s(outBuf, LINE_SIZE, NETFILTER_ID_STRING " **** invalid command %s", serverRecvbuf);
			break;
		}
	}
	return 0;
}

DWORD WINAPI statusWorkerThread(LPVOID lpParam)
{
#if LOG_STATUS
	HANDLE statusLogFile = INVALID_HANDLE_VALUE;
#endif

	HANDLE driverHandle = lpParam; /* handle to netfilter driver port */
	LARGE_INTEGER base;
	LARGE_INTEGER timestamp;
	LARGE_INTEGER freq;
	double timePassed;
	double lastTimePassed = 0;
	int lastEventCount = 0;
	char OKstring[] = "    ";
	char ErrString[] = "****";

	// Set up timing:
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&base);

#if LOG_STATUS
	statusLogFile = statusLogOpen();
#endif

	PRINT_DBG("mavenNetfilter - entered statusWorkerThread ");
	while (1)
	{		
		/* get metrics from driver */
		if (!WinDivertGetMetrics(driverHandle, &metrics, sizeof(packet_metrics_t)))
		{
			_ERROR("warning: failed to read metrics from driver (%d)\n",
				GetLastError());
			/* inform admin */
		}
		else if (ConnectSocket != INVALID_SOCKET)
		{
			double lastElapsed; /* elapsed time since we last checked alarms */
			int thisEventCount; /* alarms this event period */

			QueryPerformanceCounter(&timestamp);
			/* compute elapsed time from start(seconds) */
			timePassed = (double)(timestamp.QuadPart - base.QuadPart) /
			(double)freq.QuadPart;
			char *pString = OKstring;


			lastElapsed = lastTimePassed - lastTimePassed;
			/* if a second has passed */
			if (lastElapsed > .9)
			{
				thisEventCount = lastEventCount - eventCount;
				if (thisEventCount > eventThreshold)
				{
					pString = ErrString;
				}
				lastEventCount = thisEventCount;
			}
#if REPORT_STATUS
			sendStatus(timePassed, pString);
#endif
#if LOG_STATUS
			logStatus(timePassed, statusLogFile);
#endif

		}

		/* send heartbeat here */

		Sleep(HEARTBEAT_DELAY);
	}
#if LOG_STATUS
	statusLogClose(statusLogFile);
#endif
	return 0;
}

