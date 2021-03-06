// cncInterfaceTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

/* 
test input
01234567890123456789012345678901234567890
{"get enable": {"name":  "netFilter"}
{"get enable": {"name":  "fileFilter"}
{"enable": {"name":  "netFilter"}
{"enable": {"name":  "fileFilter"}
{"disable": {"name":  "netFilter"}
{"disable": {"name":  "fileFilter"}
{"get configuration": {"name":  "netFilter"}
{"get configuration": {"name":  "fileFilter"}
{"list enabled"}
{"enable": {"name":  "netFilter"}
{"list enabled"}
{"enable": {"name":  "fileFilter"}
{"list enabled"}
{"disable": {"name":  "netFilter"}
{"list enabled"}
{"disable": {"name":  "fileFilter"}
{"list enabled"}

test results
test JSON file commands - enter command
{"get enable": {"name":  "netFilter"}
computed return {"get enable": {"name": "netfilter", "status" : "false"} }
{"get enable": {"name":  "fileFilter"}
computed return {"get enable": {"name": "fileFilter", "status" : "false"} }
{"enable": {"name":  "netFilter"}
sending NetFilterFilter
computed return {"enable": {"name": "netfilter", "status" : "success"} }
{"enable": {"name":  "fileFilter"}
sending FileFilterFilter
computed return {"enable": {"name": "fileFilter", "status" : "success"} }
{"disable": {"name":  "netFilter"}
sending NetFilterIdle
computed return {"disable": {"name": "netfilter", "status" : "success"} }
{"disable": {"name":  "fileFilter"}
sending FileFilterIdle
computed return {"disable": {"name": "fileFilter", "status" : "success"} }
{"get configuration": {"name":  "netFilter"}
computed return {"get configuration": {"name": "netfilter", "config" : null} }
{"get configuration": {"name":  "fileFilter"}
computed return {"get configuration": {"name": "fileFilter", "config" : null} }
{"list enabled"}
computed return {"list enabled": {"name": "[]"} }
{"enable": {"name":  "netFilter"}
sending NetFilterFilter
computed return {"enable": {"name": "netfilter", "status" : "success"} }
{"list enabled"}
computed return {"list enabled": {"name": "[netfilter]"} }
{"enable": {"name":  "fileFilter"}
sending FileFilterFilter
computed return {"enable": {"name": "fileFilter", "status" : "success"} }
{"list enabled"}
computed return {"list enabled": {"name": "[fileFilter, netfilter]"} }
{"disable": {"name":  "netFilter"}
sending NetFilterIdle
computed return {"disable": {"name": "netfilter", "status" : "success"} }
{"list enabled"}
computed return {"list enabled": {"name": "[fileFilter]"} }
{"disable": {"name":  "fileFilter"}
sending FileFilterIdle
computed return {"disable": {"name": "fileFilter", "status" : "success"} }
{"list enabled"}
computed return {"list enabled": {"name": "[]"} }
*/
/* test values */
#define FILEFILTER_INDEX 0
#define NETFILTER_INDEX 1
static int ClientThreadState[2] = { 0,0 };

BOOL transducerEnabled(int transducer)
{
	if (transducer == TRANSDUCER_FILEFILTER)
		return (ClientThreadState[FILEFILTER_INDEX] == TRUE);
	if (transducer == TRANSDUCER_NETFILTER)
		return (ClientThreadState[NETFILTER_INDEX] == TRUE);
	return FALSE;	/* bad argument */
}

/* sets filter mode if enable, else idle */
void transducerSetMode(BOOL enable, int transducer)
{
	const char *cmd = NULL;
	int myId;
	if (transducer == TRANSDUCER_FILEFILTER)
	{
		myId = FILEFILTER_INDEX;
		if (enable)
			cmd = SERVER_CMD_FILTER;
		else
			cmd = SERVER_CMD_IDLE;
	}
	if (transducer == TRANSDUCER_NETFILTER)
	{
		myId = NETFILTER_INDEX;
		if (enable)
			cmd = SERVER_NET_CMD_FILTER;
		else
			cmd = SERVER_NET_CMD_IDLE;
	}

	/* set transducer state */
	if (myId == FILEFILTER_INDEX)
	{
		printf(" sending %s\n", cmd);
		ClientThreadState[myId] = enable;
	}
	/* set transducer state */
	if (myId == NETFILTER_INDEX)
	{
		printf(" sending %s\n", cmd);
		ClientThreadState[myId] = enable;
	}
}

/* returns true if unreachable */
BOOL transducerError(int transducer)
{
	return FALSE; /* parse error */
}



#include "..\..\mavenClient\mavenClient\jsonUtils.cpp"
#define CONSOLE_LINE_SIZE 132
int main()
{
	char commandLine[CONSOLE_LINE_SIZE];

	printf("test JSON file commands - enter command\n");
	/* get console commands, this is a string terminated with \n\0
	* It can be either a two character shortcut or a full command.
	* In either case, it may optionally have a argument */
	while (fgets(commandLine, CONSOLE_LINE_SIZE, stdin) != NULL)
	{
		parseCncMessage(commandLine);
		printf("computed return %s\n", commandLine);
	}

    return 0;
}

