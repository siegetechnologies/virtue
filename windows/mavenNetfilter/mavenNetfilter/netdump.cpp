/*
* netdump.c
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
* This is a simple traffic monitor.  It uses a WinDivert handle in SNIFF mode.
* The SNIFF mode copies packets and does not block the original.
*
*
*/

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\..\include\windivert.h"
#include "..\..\include\maven.h"

#define MAXBUF  0xFFFF
#define LINE_WIDTH 80

#undef WINDIVERT_DEBUG
#ifdef WINDIVERT_DEBUG
#define PRINT_DBG printf
#define WPRINT_DBG wprintf
#else
#define PRINT_DBG(...)
#define WPRINT_DBG(...)
#endif

#undef LOG_DEBUG
#ifdef LOG_DEBUG
#define LOG(...) pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, __VA_ARGS__);
#else
#define LOG(...) 
#endif

#define _ERROR(fmt,...) fprintf(stderr, "%s line %d " fmt, __FILE__, __LINE__, __VA_ARGS__)

/* build time options */
#define LOG_PACKETS
#undef _DUMP_BODY

#define _BYTES_REMAINING (((size_t)outBuf) + sizeof(outBuf) - (size_t)pOutBuf)

extern SOCKET ConnectSocket;

/***************************************************************************
* Name: packetLogOpen
*
* Routine Description: This function opens a log file for packet logging.
*
* Returns: HANDLE to the opened file
*
**************************************************************************/
HANDLE packetLogOpen(void)
{
	HANDLE statusLogFile = INVALID_HANDLE_VALUE;

	PRINT_DBG("packetLogOpen\n");
	/* open the log file */
	statusLogFile = CreateFile(PKTLOG_NAME,
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

/***************************************************************************
* Name: packetSend
*
* Routine Description: This function sends a formatted packet message to
*	mavenClient.
*
* Returns: N/A
*
**************************************************************************/
void packetSend(char *outBuf, int len)
{
	if (ConnectSocket == INVALID_SOCKET)
	{
		PRINT_DBG("INVALID_SOCKET\n");
		return;
	}
	PRINT_DBG("packetSend %s", outBuf);
	if (send(ConnectSocket, outBuf, len + 1, 0) == SOCKET_ERROR) 
	{
		_ERROR("packet log send failed with error: %d\n", WSAGetLastError());
	}
	PRINT_DBG("\n--\n");
}

/***************************************************************************
* Name: packetLogClose
*
* Routine Description: This function closes the log file
*
* Returns: N/A
*
**************************************************************************/
void packetLogClose(HANDLE file)
{
	if (file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
	}
}

/***************************************************************************
* Name: logPacket
*
* Routine Description: This function formats a string from the passed packet
*	and forwards this to a log file or message socket.
*
* Returns:
*
**************************************************************************/
void logPacket(
	HANDLE file,
	BOOL packetBlocked,
	HANDLE handle,
	double time_passed,
	unsigned char *packet,
	UINT packet_len,
	PWINDIVERT_IPHDR ip_header,
	PWINDIVERT_IPV6HDR ipv6_header,
	PWINDIVERT_ICMPHDR icmp_header,
	PWINDIVERT_ICMPV6HDR icmpv6_header,
	PWINDIVERT_TCPHDR tcp_header,
	PWINDIVERT_UDPHDR udp_header,
	WINDIVERT_ADDRESS addr
)
{
	UINT i;
#ifdef _DUMP_BODY
	UINT j;
#endif
	DWORD dwBytesWritten = 0;
	BOOL errorFlag = FALSE;

	char outBuf[PACKETDUMP_MSGSIZE], *pOutBuf;
	pOutBuf = &outBuf[0];

	// Dump packet info: 
	if (packetBlocked)
	{
		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, NETFILTER_ID_STRING
			" BLOCKED PACKET (%d bytes) -------------------------------\n", packet_len);
	}
	else
	{

		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, NETFILTER_ID_STRING
			" REINJECTED PACKET (%d bytes) -------------------------------\n", packet_len);
	}
	pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "Packet [Timestamp=%.8g, Direction=%s IfIdx=%u SubIfIdx=%u "
		"Loopback=%u]\n",
		time_passed, (addr.Direction == WINDIVERT_DIRECTION_OUTBOUND ?
			"outbound" : "inbound"), addr.IfIdx, addr.SubIfIdx,
		addr.Loopback);
	if (ip_header != NULL)
	{
		UINT8 *src_addr = (UINT8 *)&ip_header->SrcAddr;
		UINT8 *dst_addr = (UINT8 *)&ip_header->DstAddr;
		PRINT_DBG("logging IP packet\n" );
		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "IPv4 [Version=%u HdrLength=%u TOS=%u Length=%u Id=0x%.4X "
			"Reserved=%u DF=%u MF=%u \n    FragOff=%u TTL=%u Protocol=%u "
			"Checksum=0x%.4X SrcAddr=%u.%u.%u.%u DstAddr=%u.%u.%u.%u]\n",
			ip_header->Version, ip_header->HdrLength,
			ntohs(ip_header->TOS), ntohs(ip_header->Length),
			ntohs(ip_header->Id), WINDIVERT_IPHDR_GET_RESERVED(ip_header),
			WINDIVERT_IPHDR_GET_DF(ip_header),
			WINDIVERT_IPHDR_GET_MF(ip_header),
			ntohs(WINDIVERT_IPHDR_GET_FRAGOFF(ip_header)), ip_header->TTL,
			ip_header->Protocol, ntohs(ip_header->Checksum),
			src_addr[0], src_addr[1], src_addr[2], src_addr[3],
			dst_addr[0], dst_addr[1], dst_addr[2], dst_addr[3]);
	}
	if (ipv6_header != NULL)
	{
		UINT16 *src_addr = (UINT16 *)&ipv6_header->SrcAddr;
		UINT16 *dst_addr = (UINT16 *)&ipv6_header->DstAddr;
		PRINT_DBG("logging IPV6 packet\n" );
		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "IPv6 [Version=%u TrafficClass=%u FlowLabel=%u Length=%u "
			"NextHdr=%u HopLimit=%u SrcAddr=",
			ipv6_header->Version,
			WINDIVERT_IPV6HDR_GET_TRAFFICCLASS(ipv6_header),
			ntohl(WINDIVERT_IPV6HDR_GET_FLOWLABEL(ipv6_header)),
			ntohs(ipv6_header->Length), ipv6_header->NextHdr,
			ipv6_header->HopLimit);
		for (i = 0; i < 8; i++)
		{
			pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "%x%c", ntohs(src_addr[i]), (i == 7 ? ' ' : ':'));
		}
		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "DstAddr=[");
		for (i = 0; i < 8; i++)
		{
			pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "%x", ntohs(dst_addr[i]));
			if (i != 7)
			{
				pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING,":");
			}
		}
		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "]\n");
	}
	if (icmp_header != NULL)
	{
		PRINT_DBG("logging ICMP packet\n");
		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "ICMP [Type=%u Code=%u Checksum=0x%.4X Body=0x%.8X]\n",
			icmp_header->Type, icmp_header->Code,
			ntohs(icmp_header->Checksum), ntohl(icmp_header->Body));
	}
	if (icmpv6_header != NULL)
	{
		PRINT_DBG("logging ICMPV6 packet\n");
		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "ICMPV6 [Type=%u Code=%u Checksum=0x%.4X Body=0x%.8X]\n",
			icmpv6_header->Type, icmpv6_header->Code,
			ntohs(icmpv6_header->Checksum), ntohl(icmpv6_header->Body));
	}
	if (tcp_header != NULL)
	{
		PRINT_DBG("logging TCP packet\n");
		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "TCP  [SrcPort=%u DstPort=%u SeqNum=%u AckNum=%u "
			"HdrLength=%u \n    Reserved1=%u Reserved2=%u Urg=%u Ack=%u "
			"Psh=%u Rst=%u\n Syn=%u Fin=%u Window=%u Checksum=0x%.4X "
			"UrgPtr=%u]\n",
			ntohs(tcp_header->SrcPort), ntohs(tcp_header->DstPort),
			ntohl(tcp_header->SeqNum), ntohl(tcp_header->AckNum),
			tcp_header->HdrLength, tcp_header->Reserved1,
			tcp_header->Reserved2, tcp_header->Urg, tcp_header->Ack,
			tcp_header->Psh, tcp_header->Rst, tcp_header->Syn,
			tcp_header->Fin, ntohs(tcp_header->Window),
			ntohs(tcp_header->Checksum), ntohs(tcp_header->UrgPtr));
	}
	if (udp_header != NULL)
	{
		PRINT_DBG("logging UDP packet\n");
		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "UDP [SrcPort=%u DstPort=%u Length=%u "
			"Checksum=0x%.4X]\n",
			ntohs(udp_header->SrcPort), ntohs(udp_header->DstPort),
			ntohs(udp_header->Length), ntohs(udp_header->Checksum));
	}
	LOG("\ndump packet in hex\n");
	/* dump packet header in hex */
	for (i = 0; i < packet_len; i++)
	{
		if (i % (LINE_WIDTH/2) == 0)
		{
			pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "\n");
		}
		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "%.2X", (UINT8)packet[i]);
	}

#ifdef _DUMP_BODY
	i = 0;
	/* print packet header with native line breaks */
	if (tcp_header != NULL)
	{
		LOG("\nprint packet header with native line breaks\n");
		pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "\n");
		while (i < packet_len)
		{
			int newlineCount = 0;

			if (isprint(packet[i]) || (packet[i] == '\n') || (packet[i] == '\r'))
			{
				pOutBuf += snprintf(pOutBuf, 1, "%c", packet[i]);
			}
			else
			{
				pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, ".");
			}

			/* the HTML header is terminated with a blank line. i.e. 4 sequential
			* characters of either \n or \n.
			*/
			if ((packet[i] == '\r') || (packet[i] == '\t'))
			{
				if (++newlineCount == 4)
				{
					i++;
					break; /* end of header */
				}
				else
				{
					newlineCount = 0;
				}
			}
			i++;
		}
	}
	/* dump remaining body */
	j = i; /* start offset for dumping */
	for (; i < packet_len; i++)
	{
		if ((i + j) % LINE_WIDTH == 0)
		{
			pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING, "\n");
		}
		if (isprint(packet[i]))
		{
			pOutBuf += snprintf(pOutBuf, 1, "%c", (UINT8)packet[i]);
		}
		else
		{
			pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING,".");
		}
	}
#endif
	pOutBuf += snprintf(pOutBuf, _BYTES_REMAINING,"\n");

#ifdef LOG_PACKETS
	/* report only blocked packets */
	if (packetBlocked)
	{
		packetSend(outBuf, static_cast<int>(strlen(outBuf)));
	}
#endif
	errorFlag = WriteFile(
		file,								// open file handle
		outBuf,								// start of data to write
		static_cast<int>(strlen(outBuf)),	// number of bytes to write
		&dwBytesWritten,					// number of bytes that were written
		NULL);								// no overlapped structure

	if (errorFlag == FALSE)
	{
		_ERROR("Unable to write to packet log file.\n");
	}
	else if (dwBytesWritten == 0)
	{
		// This is an error because a synchronous write that results in
		// success (WriteFile returns TRUE) should write all data as
		// requested. This would not necessarily be the case for
		// asynchronous writes.
		_ERROR("error writing to packet log file.\n");
	}

}


