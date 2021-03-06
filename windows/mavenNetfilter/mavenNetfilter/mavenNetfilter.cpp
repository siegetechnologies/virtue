/* mavenNetfilter.cpp
* This process interfaces with the netfilter driver, windivertDrv, and handles
*	message traffic and state changes. 
*/

#include "stdafx.h"

/*
* portions of this are based on windivert webfilter.  The copyright notice
* from that is included below.
*/

/*
* webfilter.c
* (C) 2017,2018 all rights reserved,
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
* DESCRIPTION:
* This is a simple web (HTTP) filter using WinDivert.
*
* It works by intercepting outbound HTTP GET/POST requests and matching
* the URL against a urlBlackList.  If the URL is matched, we hijack the TCP
* connection, reseting the connection at the server end, and sending a
* blockpage to the browser.
*/


#define MAXBUF 0xFFFF
#define MAXURL 4096
#define LINE_SIZE 132

#undef MAVEN_DEBUG
#ifdef MAVEN_DEBUG
#define PRINT_DBG printf
#define WPRINT_DBG wprintf
#else
#define PRINT_DBG(...)
#define WPRINT_DBG(...)
#endif
#pragma warning(disable: 4996) // strcpy insecure warning

#define _ERROR(fmt,...) fprintf(stderr, "%s line %d " fmt, __FILE__, __LINE__, __VA_ARGS__)

/*
* URL and urlBlackList representation.
*/
typedef struct
{
	char *domain;
	char *uri;
} URL, *PURL;
typedef struct
{
	UINT size;
	UINT length;
	PURL *urls;
} URLBLACKLIST, *PURLBLACKLIST;

UINT32 ipV4Blacklist[IP_BLACKLIST_SIZE] = { 0 };
UINT64 ipV6Blacklist[IP_BLACKLIST_SIZE] = { 0 };

/*
* Pre-fabricated packets.
*/
typedef struct
{
	WINDIVERT_IPHDR  ip;
	WINDIVERT_TCPHDR tcp;
} PACKET, *PPACKET;

typedef struct
{
	PACKET header;
	UINT8 data[1];	/* variable size array, defined here as one element */
} DATAPACKET, *PDATAPACKET;

/* global data */
SOCKET ConnectSocket = INVALID_SOCKET;

static HANDLE driverHandle = NULL;

/*
* THe block page contents.
*/
const char block_data[] =
"HTTP/1.1 200 OK\r\n"
"Connection: close\r\n"
"Content-Type: text/html\r\n"
"\r\n"
"<!doctype html>\n"
"<html>\n"
"\t<head>\n"
"\t\t<title>BLOCKED!</title>\n"
"\t</head>\n"
"\t<body>\n"
"\t\t<h1>BLOCKED!</h1>\n"
"\t\t<hr>\n"
"\t\t<p>This URL has been blocked!</p>\n"
"\t</body>\n"
"</html>\n";

extern HANDLE packetLogOpen(void);
extern void packetLogClose(HANDLE file);
extern HANDLE statusLogOpen(void);
extern void statusLogClose(HANDLE file);
extern NETFILTER_STATE netState;

packet_metrics_t metrics = { 0 };	/* metrics from driver */
int eventCount = 0;		/* number of blocked packets */


/*
* Prototypes
*/
extern void logPacket(
	HANDLE logFile,
	BOOL packetBlocked,
	HANDLE driverHandle,
	double time_passed,
	unsigned char *packet,
	UINT packet_len,
	PWINDIVERT_IPHDR ip_header,
	PWINDIVERT_IPV6HDR ipv6_header,
	PWINDIVERT_ICMPHDR icmp_header,
	PWINDIVERT_ICMPV6HDR icmpv6_header,
	PWINDIVERT_TCPHDR tcp_header,
	PWINDIVERT_UDPHDR udp_header,
	WINDIVERT_ADDRESS addr);

static void PacketInit(PPACKET packet);
static int __cdecl UrlCompare(const void *a, const void *b);
static int UrlMatch(PURL urla, PURL urlb);
static PURLBLACKLIST urlBlackListInit(void);
static void urlBlackListInsert(PURLBLACKLIST urlBlackList, PURL url);
static void urlBlackListSort(PURLBLACKLIST urlBlackList);
static BOOL urlBlackListMatch(PURLBLACKLIST urlBlackList, PURL url);
static void urlBlackListRead(PURLBLACKLIST urlBlackList, const char *filename);
static BOOL urlBlackListPayloadMatch(PURLBLACKLIST urlBlackList, char *data,
	UINT16 len);
static void ipV4BlackListRead(void);
static BOOL ipV4BlackListAddressMatch(PVOID pPacket, UINT packetLen);

DWORD WINAPI statusWorkerThread(LPVOID lpParam);
DWORD WINAPI clientWorkerThread(LPVOID lpParam);

/*
* Entry.
*/
int __cdecl main(void)
{
	HANDLE  pktFile;
	LARGE_INTEGER base;
	LARGE_INTEGER freq;
	WINDIVERT_ADDRESS addr;
	UINT8 packet[MAXBUF];
	UINT packet_len;
	BOOL packetBlocked;
	PWINDIVERT_IPHDR ip_header;
	PWINDIVERT_TCPHDR tcp_header;
	PWINDIVERT_IPV6HDR ipv6_header;
	PWINDIVERT_ICMPHDR icmp_header;
	PWINDIVERT_ICMPV6HDR icmpv6_header;
	PWINDIVERT_UDPHDR udp_header;

	HRESULT hResult = S_OK;
	WSADATA wsaData;
	struct addrinfo *result = NULL;
	struct addrinfo *ptr = NULL;
	struct addrinfo hints;
	int iResult;
	
	PVOID payload;
	UINT payload_len;
	PACKET reset0;
	PPACKET reset = &reset0;
	PACKET finish0;
	PPACKET finish = &finish0;
	PDATAPACKET blockpage;
	UINT16 blockpage_len;
	PURLBLACKLIST urlBlackList;
	INT16 priority = 404;       // Arbitrary.
	double time_passed;
	HANDLE  hClientWorkerThread;
	HANDLE hStatusWorkerThread;
	DWORD   dwThreadId;
	
#if 0 // for connecting the debugger
	printf("************netfilter waits for user, press a key\n");
	printf("%c", getchar());
#endif
	// Read the blacklists.
	urlBlackList = urlBlackListInit();
	urlBlackListRead(urlBlackList, URL_BLACKLIST_NAME);
	urlBlackListSort(urlBlackList);
	ipV4BlackListRead();

	// Initialize the pre-frabricated packets:
	blockpage_len = sizeof(DATAPACKET) + sizeof(block_data) - 1;
	blockpage = (PDATAPACKET)malloc(blockpage_len);
	if (blockpage == NULL)
	{
		_ERROR("error: memory allocation failed\n");
		exit(EXIT_FAILURE);
	}
	PacketInit(&blockpage->header);
	blockpage->header.ip.Length = htons(blockpage_len);
	blockpage->header.tcp.SrcPort = htons(80);
	blockpage->header.tcp.Psh = 1;
	blockpage->header.tcp.Ack = 1;
	memcpy(blockpage->data, block_data, sizeof(block_data) - 1);
	PacketInit(reset);
	reset->tcp.Rst = 1;
	reset->tcp.Ack = 1;
	PacketInit(finish);
	finish->tcp.Fin = 1;
	finish->tcp.Ack = 1;

	// Open the Divert device:
	driverHandle = WinDivertOpen(
		// "outbound && "           /* Outbound traffic only */
		"icmp || "					/* ICMP */
		"udp || "					/* UDP */
		"!loopback && "             /* No loopback traffic */
		"ip && "                    /* IPv4 */
		"!tcp.DstPort == 65000 && " /* not maven port */
		"!tcp.DstPort == 5170 && "  /* not fluentd port */
		"!tcp.DstPort == 3389 && "  /* not RDP port */
		"!tcp.DstPort == 4022 && "  /* not remote debugger port */
		// "tcp.DstPort == 80 && "  /* HTTP (port 80) only */
		"tcp.PayloadLength > 0",    /* TCP data packets only */
		WINDIVERT_LAYER_NETWORK, priority, 0
	);
	if (driverHandle == INVALID_HANDLE_VALUE)
	{
		_ERROR("error: failed to open the WinDivert device (%d)\n",
			GetLastError());
		exit(EXIT_FAILURE);
	}
	PRINT_DBG("OPENED WinDivert\n");

	// Set up timing:
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&base);

	/* open the packet log file */
	pktFile = packetLogOpen();

	/* open a socket to the client app */

	PRINT_DBG("mavenNetFilter - Initialize Winsock\n");
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
	iResult = getaddrinfo(NULL, NETFILTER_PORT_NUMBER, &hints, &result);
	if (iResult != 0) {
		_ERROR("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}
	PRINT_DBG("mavenNetFilter - Attempt to connect to an address \n");
	/* Attempt to connect to an address until one succeeds */
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		PRINT_DBG(".");
		/* Create a SOCKET for connecting to server */
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			_ERROR("mavenNetFilter - socket failed with error: %ld\n", WSAGetLastError());
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

	hClientWorkerThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		clientWorkerThread,		// thread function name
		NULL,					// argument to thread function 
		0,                      // use default creation flags 
		&dwThreadId);			// returns the thread identifier 

	hStatusWorkerThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		statusWorkerThread,		// thread function name
		driverHandle,           // argument to thread function 
		0,                      // use default creation flags 
		&dwThreadId);			// returns the thread identifier 

	/* Send a connect message */
	PRINT_DBG("mavenNetfilter Send a connect message\n");
	iResult = send(ConnectSocket, NETFILTER_ID_STRING, (int)strlen(NETFILTER_ID_STRING) + 1, 0);
	if (iResult == SOCKET_ERROR) {
		hResult = NS_E_INTERNAL_SERVER_ERROR;
		PRINT_DBG("mavenNetFilter - send failed with error: %d\n", WSAGetLastError());
		goto Main_Exit;
	}

	PRINT_DBG("mavenNetFilter - Bytes Sent: %ld, 0x%x\n", iResult, hResult);

	// Main loop:
	while (TRUE)
	{
		BOOL parseStatus;

		if (!WinDivertRecv(driverHandle, packet, sizeof(packet), &addr, &packet_len))
		{
			_ERROR("warning: failed to read packet (%d)\n",
				GetLastError());
			continue;
		}

		parseStatus = WinDivertHelperParsePacket(packet, packet_len, &ip_header,
			&ipv6_header, &icmp_header, &icmpv6_header, &tcp_header,
			&udp_header, &payload, &payload_len);

		/* get current time
		 * compute elapsed time from start (seconds) */
		time_passed = (double)(addr.Timestamp - base.QuadPart) /
			(double)freq.QuadPart;

		PRINT_DBG("Received packet status = %d\n", parseStatus);

		/* if TCPIPV4 && either blacklist matches */
		if ( (netState == NETFILTER_FILTER) &&
			 (((PWINDIVERT_IPHDR)packet)->Version == 4) &&
			 (((PWINDIVERT_IPHDR)packet)->Protocol == IPPROTO_TCP)
			 && (
			 ipV4BlackListAddressMatch(packet, packet_len) ||
			 urlBlackListPayloadMatch(urlBlackList, (char *)payload, 
				(UINT16)payload_len)))
		{
			packetBlocked = TRUE;
			eventCount++;
			PRINT_DBG("Block packet\n");
		}
		else
		{
			// Packet does not match either BlackList; simply reinject it.
			PRINT_DBG("Reinject packet\n");
			if (!WinDivertSend(driverHandle, packet, packet_len, &addr, NULL))
			{
				_ERROR("warning: failed to reinject packet (%d)\n",
					GetLastError());
			}
			packetBlocked = FALSE;

		}

#if 0
		if ( !((((PWINDIVERT_IPHDR)packet)->Version == 4) && 
			   (((PWINDIVERT_IPHDR)packet)->Protocol == IPPROTO_TCP)
			  ) ||
			!urlBlackListPayloadMatch(urlBlackList, (char *)payload, (UINT16)payload_len))
		{
			// Packet does not match the urlBlackList; simply reinject it.
			PRINT_DBG("Reinject packet\n");
			if (!WinDivertSend(driverHandle, packet, packet_len, &addr, NULL))
			{
				fprintf(stderr, "warning: failed to reinject packet (%d)\n",
					GetLastError());
			}
			packetBlocked = FALSE;

		}
		else
		{
			packetBlocked = TRUE;
			eventCount++;
			PRINT_DBG("Block packet\n");
		}
#endif
		logPacket(
			pktFile,
			packetBlocked,
			driverHandle,
			time_passed,
			packet,
			packet_len,
			ip_header,
			ipv6_header,
			icmp_header,
			icmpv6_header,
			tcp_header,
			udp_header,
			addr);

		if (!packetBlocked)
		{
			continue;
		}

		// The URL matched the urlBlackList; we block it by hijacking the TCP
		// connection.

		// (1) Send a TCP RST to the server; immediately closing the
		//     connection at the server's end.
		reset->ip.SrcAddr = ip_header->SrcAddr;
		reset->ip.DstAddr = ip_header->DstAddr;
		reset->tcp.SrcPort = tcp_header->SrcPort;
		reset->tcp.DstPort = htons(80);
		reset->tcp.SeqNum = tcp_header->SeqNum;
		reset->tcp.AckNum = tcp_header->AckNum;
		WinDivertHelperCalcChecksums((PVOID)reset, sizeof(PACKET), &addr, 0);
		if (!WinDivertSend(driverHandle, (PVOID)reset, sizeof(PACKET), &addr, NULL))
		{
			_ERROR("warning: failed to send reset packet (%d)\n",
				GetLastError());
		}

		// (2) Send the blockpage to the browser:
		blockpage->header.ip.SrcAddr = ip_header->DstAddr;
		blockpage->header.ip.DstAddr = ip_header->SrcAddr;
		blockpage->header.tcp.DstPort = tcp_header->SrcPort;
		blockpage->header.tcp.SeqNum = tcp_header->AckNum;
		blockpage->header.tcp.AckNum =
			htonl(ntohl(tcp_header->SeqNum) + payload_len);
		addr.Direction = !addr.Direction;     // Reverse direction.
		WinDivertHelperCalcChecksums((PVOID)blockpage, blockpage_len, &addr, 0);
		if (!WinDivertSend(driverHandle, (PVOID)blockpage, blockpage_len, &addr,
			NULL))
		{
			_ERROR("warning: failed to send block page packet (%d)\n",
				GetLastError());
		}

		// (3) Send a TCP FIN to the browser; closing the connection at the 
		//     browser's end.
		finish->ip.SrcAddr = ip_header->DstAddr;
		finish->ip.DstAddr = ip_header->SrcAddr;
		finish->tcp.SrcPort = htons(80);
		finish->tcp.DstPort = tcp_header->SrcPort;
		finish->tcp.SeqNum =
			htonl(ntohl(tcp_header->AckNum) + sizeof(block_data) - 1);
		finish->tcp.AckNum =
			htonl(ntohl(tcp_header->SeqNum) + payload_len);
		WinDivertHelperCalcChecksums((PVOID)finish, sizeof(PACKET), &addr, 0);
		if (!WinDivertSend(driverHandle, (PVOID)finish, sizeof(PACKET), &addr, NULL))
		{
			_ERROR("warning: failed to send finish packet (%d)\n",
				GetLastError());
		}
	}

Main_Exit:

	packetLogClose(pktFile);


}

/*
* Initialize a PACKET.
*/
static void PacketInit(PPACKET packet)
{
	memset(packet, 0, sizeof(PACKET));
	packet->ip.Version = 4;
	packet->ip.HdrLength = sizeof(WINDIVERT_IPHDR) / sizeof(UINT32);
	packet->ip.Length = htons(sizeof(PACKET));
	packet->ip.TTL = 64;
	packet->ip.Protocol = IPPROTO_TCP;
	packet->tcp.HdrLength = sizeof(WINDIVERT_TCPHDR) / sizeof(UINT32);
}

/*
* read ipV4BlackList from file.
*/
static void ipV4BlackListRead(void)
{
	int listIndex = 0;
	int i;
	FILE *file;
	char line[LINE_SIZE];

	PRINT_DBG("ipV4BlackListRead opening %s\n", IPV4_BLACKLIST_NAME);
	fopen_s(&file, IPV4_BLACKLIST_NAME, "r");

	if (file == NULL)
	{
		_ERROR("error: could not open" IPV4_BLACKLIST_NAME "\n");
		return;
	}

	// Read addresses from the file and add them to the ipV4BlackList: 
	while (listIndex < IP_BLACKLIST_SIZE)
	{
		/* windows has no getline function */
		i = -1;
		do
		{
			i++;
			line[i] = getc(file);
		} while ((line[i] != '\n') && (line[i] != '\0') && (line[i] != EOF));

		line[i] = '\0';

		PRINT_DBG("ipV4BlackListRead reads %s\n", line);
		/* convert the string to an IP address */
		if (inet_pton(AF_INET, line, &ipV4Blacklist[listIndex]))
		{
			PRINT_DBG("ipV4BlackListRead writes 0x%x\n", ipV4Blacklist[listIndex]);
		}
		else
		{
			break;
		}
		listIndex++;

	}
	fclose(file);
	return;
}

/*
* read ipV6BlackList from file.
*/
static void ipV6BlackListRead(void)
{
	int listIndex = 0;
	int i;
	FILE *file;
	char line[LINE_SIZE];

	fopen_s(&file, IPV6_BLACKLIST_NAME, "r");

	if (file == NULL)
	{
		_ERROR("error: could not open" IPV6_BLACKLIST_NAME "\n");
		return;
	}

	// Read addresses from the file and add them to the ipV6BlackList: 
	while (listIndex < IP_BLACKLIST_SIZE)
	{
		/* windows has no getline function */
		i = 0;
		do
		{
			line[i++] = getc(file);
		} while ((line[i] != '\n') && (line[i] != EOF));

		/* convert the string to an IP address */
		if (inet_pton(AF_INET6, line, &ipV6Blacklist[listIndex]))
		{
			listIndex++;
		}

	}
	fclose(file);
	return;
}

/*
* Initialize an empty urlBlackList.
*/
static PURLBLACKLIST urlBlackListInit(void)
{
	PURLBLACKLIST urlBlackList = (PURLBLACKLIST)malloc(sizeof(URLBLACKLIST));
	UINT size;
	if (urlBlackList == NULL)
	{
		goto memory_error;
	}
	size = 1024;
	urlBlackList->urls = (PURL *)malloc(size * sizeof(PURL));
	if (urlBlackList->urls == NULL)
	{
		goto memory_error;
	}
	urlBlackList->size = size;
	urlBlackList->length = 0;

	return urlBlackList;

memory_error:
	_ERROR("error: failed to allocate memory\n");
	exit(EXIT_FAILURE);
}

/*
* Insert a URL into a urlBlackList.
*/
static void urlBlackListInsert(PURLBLACKLIST urlBlackList, PURL url)
{

	if (urlBlackList->length >= urlBlackList->size)
	{
		urlBlackList->size = (urlBlackList->size * 3) / 2;
		PRINT_DBG("GROW urlBlackList to %u\n", urlBlackList->size);
		urlBlackList->urls = (PURL *)realloc(urlBlackList->urls,
			urlBlackList->size * sizeof(PURL));
		if (urlBlackList->urls == NULL)
		{
			_ERROR("error: failed to reallocate memory\n");
			exit(EXIT_FAILURE);
		}
	}

	urlBlackList->urls[urlBlackList->length++] = url;
}

/*
* Sort the urlBlackList (for searching).
*/
static void urlBlackListSort(PURLBLACKLIST urlBlackList)
{
	qsort(urlBlackList->urls, urlBlackList->length, sizeof(PURL), UrlCompare);
}

/*
* Match a URL against the urlBlackList.
*/
static BOOL urlBlackListMatch(PURLBLACKLIST urlBlackList, PURL url)
{
	int lo = 0, hi = ((int)urlBlackList->length) - 1;

	while (lo <= hi)
	{
		INT mid = (lo + hi) / 2;
		int cmp = UrlMatch(url, urlBlackList->urls[mid]);
		if (cmp > 0)
		{
			hi = mid - 1;
		}
		else if (cmp < 0)
		{
			lo = mid + 1;
		}
		else
		{
			return TRUE;
		}
	}
	return FALSE;
}


/*
* Read URLs from a file.
*/
static void urlBlackListRead(PURLBLACKLIST urlBlackList, const char *filename)
{
	char domain[MAXURL + 1];
	char uri[MAXURL + 1];
	int c;
	UINT16 i, j;
	PURL url;
	FILE *file;
		
	fopen_s(&file, filename, "r");

	if (file == NULL)
	{
		_ERROR("error: could not open urlBlackList file %s\n",
			filename);
		exit(EXIT_FAILURE);
	}

	// Read URLs from the file and add them to the urlBlackList: 
	while (TRUE)
	{
		while (isspace(c = getc(file)))
			;
		if (c == EOF)
		{
			break;
		}
		if (c != '-' && !isalnum(c))
		{
			while (!isspace(c = getc(file)) && c != EOF)
				;
			if (c == EOF)
			{
				break;
			}
			continue;
		}
		i = 0;
		domain[i++] = (char)c;
		while ((isalnum(c = getc(file)) || c == '-' || c == '.') && i < MAXURL)
		{
			domain[i++] = (char)c;
		}
		domain[i] = '\0';
		j = 0;
		if (c == '/')
		{
			while (!isspace(c = getc(file)) && c != EOF && j < MAXURL)
			{
				uri[j++] = (char)c;
			}
			uri[j] = '\0';
		}
		else if (isspace(c))
		{
			uri[j] = '\0';
		}
		else
		{
			while (!isspace(c = getc(file)) && c != EOF)
				;
			continue;
		}

		PRINT_DBG("ADD %s/%s\n", domain, uri);

		url = (PURL)malloc(sizeof(URL));
		if (url == NULL)
		{
			goto memory_error;
		}
		url->domain = (char *)malloc((i + 1) * sizeof(char));
		url->uri = (char *)malloc((j + 1) * sizeof(char));
		if (url->domain == NULL || url->uri == NULL)
		{
			goto memory_error;
		}
#if 0
		strcpy_s(url->uri, MAXURL + 1, uri); // corrupts heap
#else
		strcpy(url->uri, uri);
#endif
		for (j = 0; j < i; j++)
		{
			url->domain[j] = domain[i - j - 1];
		}
		url->domain[j] = '\0';

		urlBlackListInsert(urlBlackList, url);
	}

	fclose(file);
	return;

memory_error:
	_ERROR("error: memory allocation failed\n");
	exit(EXIT_FAILURE);
}

static BOOL ipV4BlackListAddressMatch(PVOID pPacket, UINT packetLen)
{
	int index = 0;
	PWINDIVERT_IPHDR ip_header = (PWINDIVERT_IPHDR)pPacket;
	if ((ip_header->Version != 4) || (packetLen < sizeof(WINDIVERT_IPHDR)))
	{
		return 0;	/* can't process */
	}
	/* search blacklist for a matching address */
	while (ipV4Blacklist[index] != 0)
	{
		PRINT_DBG("ipV4BlackListAddressMatch comparing ip_header->SrcAddr=0x%x, ip_header->DstAddr=0x%x, ipV4Blacklist[%d}=0x%x\n", 
			ip_header->SrcAddr, ip_header->DstAddr, index, ipV4Blacklist[index]);

		if ((ip_header->SrcAddr == ipV4Blacklist[index]) ||
			(ip_header->DstAddr == ipV4Blacklist[index++]))
		{
			PRINT_DBG("ipV4BlackListAddressMatch found\n");
			return 1;
		}
	}
	PRINT_DBG("ipV4BlackListAddressMatch not found\n");
	return 0; /* none found */

}
/*
* Attempt to parse a URL and match it with the urlBlackList.
*
* BUG:
* - This function makes several assumptions about HTTP requests, such as:
*      1) The URL will be contained within one packet;
*      2) The HTTP request begins at a packet boundary;
*      3) The Host header immediately follows the GET/POST line.
*   Some browsers, such as Internet Explorer, violate these assumptions
*   and therefore matching will not work.
*/
static BOOL urlBlackListPayloadMatch(PURLBLACKLIST urlBlackList, char *data, UINT16 len)
{
	static const char get_str[] = "GET /";
	static const char post_str[] = "POST /";
	static const char http_host_str[] = " HTTP/1.1\r\nHost: ";
	static const char host_str[] = "\r\nHost: ";
	static const char host_str_terminator[] = "\r\n";
	char domain[MAXURL];
	char uri[MAXURL];
	URL url = { domain, uri };
	UINT16 i = 0;
	size_t j;
	BOOL result;
	char *pHostStr;
	char *pDomain;
	char *pEndStr;

	if (len <= sizeof(post_str) + sizeof(http_host_str))
	{
		return FALSE;
	}
	if (strncmp(data, get_str, sizeof(get_str) - 1) == 0)
	{
		i += sizeof(get_str) - 1;
	}
	else if (strncmp(data, post_str, sizeof(post_str) - 1) == 0)
	{
		i += sizeof(post_str) - 1;
	}
	else
	{
		return FALSE;
	}

	for (j = 0; i < len && data[i] != ' '; j++, i++)
	{
		uri[j] = data[i];
	}
	uri[j] = '\0';
	if (i + sizeof(http_host_str) - 1 >= len)
	{
		return FALSE;
	}

	/* search for host_str in remaining string */

	/* find domain name key in passed string */
	pHostStr = strstr(data + i, host_str);
	if (pHostStr == NULL)
	{
		return FALSE;
	}
	/* advance past key to domain name */
	pDomain = pHostStr + sizeof(host_str) -1;

	/* copy the domain name from passed string */
	strncpy(domain, pDomain, MAXURL - 1);

	if (domain[0] == 0)
	{
		return FALSE; /* zero length domain */
	}
	/* find next key */
	pEndStr = strstr(domain, host_str_terminator);
	if (pEndStr == NULL)
	{
		return FALSE; /* domain field separator not found */
	}

	*pEndStr = 0; /* termiante domain name */
	j = strlen(domain);

	PRINT_DBG("URL %s/%s: ", domain, uri);

	// Reverse the domain:
	for (i = 0; i < j - 1; i++, j--)
	{
		char t = domain[i];
		domain[i] = domain[j - 1];
		domain[j - 1] = t;
	}


	// Search the urlBlackList:
	result = urlBlackListMatch(urlBlackList, &url);
#if 0
	// Print the verdict:
	console = GetStdHandle(STD_OUTPUT_HANDLE);
	if (result)
	{
		SetConsoleTextAttribute(console, FOREGROUND_RED);
		puts("BLOCKED!");
	}
	else
	{
		SetConsoleTextAttribute(console, FOREGROUND_GREEN);
		puts("allowed");
	}
	SetConsoleTextAttribute(console,
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
	return result;
}

/*
* URL comparison.
*/
static int __cdecl UrlCompare(const void *a, const void *b)
{
	PURL urla = *(PURL *)a;
	PURL urlb = *(PURL *)b;
	int cmp = strcmp(urla->domain, urlb->domain);
	if (cmp != 0)
	{
		return cmp;
	}
	return strcmp(urla->uri, urlb->uri);
}

/*
* URL matching
*/
static int UrlMatch(PURL urla, PURL urlb)
{
	UINT16 i;

	for (i = 0; urla->domain[i] && urlb->domain[i]; i++)
	{
		int cmp = (int)urlb->domain[i] - (int)urla->domain[i];
		if (cmp != 0)
		{
			return cmp;
		}
	}
	if (urla->domain[i] == '\0' && urlb->domain[i] != '\0')
	{
		return 1;
	}

	for (i = 0; urla->uri[i] && urlb->uri[i]; i++)
	{
		int cmp = (int)urlb->uri[i] - (int)urla->uri[i];
		if (cmp != 0)
		{
			return cmp;
		}
	}
	if (urla->uri[i] == '\0' && urlb->uri[i] != '\0')
	{
		return 1;
	}
	return 0;
}

