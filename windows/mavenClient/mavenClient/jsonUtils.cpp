
#include "stdafx.h"

/* command enumeration from C&C*/
#define CNC_INVALID 0
#define	CNC_ENABLE_FILEFILTER 1
#define	CNC_ENABLE_NETFILTER 2
#define CNC_DISABLE_FILEFILTER 3
#define	CNC_DISABLE_NETFILTER 4
#define	CNC_GET_ENABLE_FILEFILTER 5
#define	CNC_GET_ENABLE_NETFILTER 6
#define	CNC_GET_CONFIG_FILEFILTER 7
#define	CNC_GET_CONFIG_NETFILTER 8
#define CNC_LIST_ENABLED 9

/* strings used to parse C&C commands */
#define CNC_CMD_ENABLE "enable"
#define CNC_CMD_DISABLE "disable"
#define CNC_CMD_GET_ENABLE "get enable"
#define CNC_CMD_GET_CONFIG "get configuration"
#define CNC_CMD_LIST_ENABLED "list enabled"

/* strings ID C&C commands */
#define CNC_KEY_CMD_ENABLE 0
#define CNC_KEY_CMD_DISABLE 1
#define CNC_KEY_CMD_GET_ENABLE 2
#define CNC_KEY_CMD_GET_CONFIG 3
#define CNC_KEY_CMD_LIST_ENABLED 4

#define CNC_NUM_CMDS 5
static const char *CnCCmds[CNC_NUM_CMDS] = {
	CNC_CMD_ENABLE,
	CNC_CMD_DISABLE,
	CNC_CMD_GET_ENABLE,
	CNC_CMD_GET_CONFIG,
	CNC_CMD_LIST_ENABLED,
};

#define CNC_CMD_FILEFILTER "fileFilter"
#define CNC_CMD_NETFILTER "netFilter"
#define NUM_TRANSDUCERS 2


static const char *transducer[NUM_TRANSDUCERS] = {
	CNC_CMD_FILEFILTER,
	CNC_CMD_NETFILTER
};

/* outgoing C&C JSON message fragments */
#define JSON_ENABLE "{\"enable\": "
#define JSON_GET_ENABLE "{\"get enable\": "
#define JSON_GET_CONFIG "{\"get configuration\": "
#define JSON_LIST_ENABLED "{\"list enabled\": "
#define JSON_DISABLE "{\"disable\": "
#define JSON_NAME "{\"name\": "
#define JSON_FILEFILTER "\"fileFilter\", "
#define JSON_NETFILTER "\"netFilter\", "
#define JSON_STATUS "\"status\" : "
#define JSON_SUCCESS "\"success\"} }"
#define JSON_TRUE "\"true\"} }"
#define JSON_FALSE "\"false\"} }"
#define JSON_ERROR "\"ERROR\"} }"
#define JSON_CONFIG "\"config\" : "
#define JSON_NULL "null} }"
#define JSON_NEITHER "\"[]\"} }"
#define JSON_FF "\"[fileFilter]\"} }"
#define JSON_NF "\"[netFilter]\"} }"
#define JSON_BOTH "\"[fileFilter, netFilter]\"} }"



void transducerSetMode(int transducer, BOOL enable); /* sets filter mode if enable, else idle */
BOOL transducerError(int transducer);	/* returns true if unreachable */
BOOL transducerEnabled(int transducer);	/* returns true if filter mode */

/***************************************************************************
* Name: parseGetToken
*
* Routine Description: This function parses a message buffer and returns an
*	index to the next JSON keyword.  Keywords are delimted by quotation marks.
*	Whatever is between the two quotes is assumed to be the keyword.
*	Keywords must be terminated by semicolons ':'.
*
* Returns: Index of next character after the second quote or zero if error.
*
**************************************************************************/
int parseGetToken(
	char *recvbuf,		/* string to parse */
	int	index,			/* index to start looking */
	int *firstQuote,	/* output - index of first quote */
	int *secondQuote	/* output - index of second quote */
)
{
	int strIndex = index;
	while (recvbuf[strIndex] != '\0')
	{
		/* find quotes */
		if (recvbuf[strIndex] == '"')
		{
			if (*firstQuote == 0)
			{
				*firstQuote = strIndex;
			}
			else
			{
				if (*secondQuote == 0)
				{
					*secondQuote = strIndex;
				}
				else
				{
					return 0; /* parse error */
				}
			}
		}
		/* find semicolon or close brace */
		if ((recvbuf[strIndex] == ':') || (recvbuf[strIndex] == '}'))
		{
			if (*secondQuote != 0)
			{
				return strIndex;
			}
			return 0;	/* parse error */
		}

		strIndex++;
	}
	return 0;	/* parse error */

}

/***************************************************************************
* Name: strIndexParseForName
*
* Routine Description: This function parses messages received from the CmC
*	port and returns the transducer index.
*
* Returns: transducer index or -1 if error
*
**************************************************************************/
int strIndexParseForName(char *recvbuf, int index)
{
	int i;
	int strIndex = index;
	int firstQuote = 0;
	int secondQuote = 0;
	size_t sourceLen;

	if (recvbuf[0] != '{')
	{
		return 0; /* not JSON */
	}
	/* start looking for next token delimted by quotes */
	strIndex = parseGetToken(recvbuf, index + 1, &firstQuote, &secondQuote);

	if (strIndex == 0)
	{
		return 0; /* parse error */
	}

	/* compute length of token we found */
	sourceLen = secondQuote - firstQuote - 1;

	if (sourceLen != strlen("name"))
	{
		return 0; /* parse error */
	}

	/* token should be "name" */
	if ((strncmp(&recvbuf[firstQuote + 1], "name", 4) != 0))
	{
	return 0; /* parse error */
	}

	/* start looking for next token delimted by quotes */
	firstQuote = secondQuote = 0;
	strIndex = parseGetToken(recvbuf, strIndex + 1, &firstQuote, &secondQuote);

	/* compute length of command we found */
	sourceLen = secondQuote - firstQuote - 1;

	if (sourceLen <= 0)
	{
		return 0; /* parse error */
	}

	/* we should now have a transducer name to parse, find it */
	for (i = 0; i < NUM_TRANSDUCERS; i++)
	{
		size_t cmdLen;
		cmdLen = strlen(transducer[i]);
		if (sourceLen != cmdLen)
		{
			continue;
		}
		if (strncmp(&recvbuf[firstQuote + 1], transducer[i], sourceLen) == 0)
		{
			break;	/* found command */
		}
	}

	if (i == NUM_TRANSDUCERS)
	{
		return -1; /* parse error, command not fund */
	}
	return i;
}

/***************************************************************************
* Name: parseGetCommand
*
* Routine Description: This function parses messages received from the CmC
*	port and returns the enumerated command type.
*
* Returns: CnC command
*
**************************************************************************/
int parseGetCommand(char *recvbuf)
{
	int cmd, transducer;
	int strIndex = 1;
	int firstQuote = 0;
	int secondQuote = 0;
	int rturnVal = CNC_INVALID;
	size_t sourceLen;

	if (recvbuf[0] != '{')
	{
		return 0; /* not JSON */
	}
	strIndex = parseGetToken(recvbuf, strIndex, &firstQuote, &secondQuote);

	if (strIndex == 0)
	{
		return 0; /* parse error */
	}

	/* compute length of command we found */
	sourceLen = secondQuote - firstQuote - 1;

	if (sourceLen <= 0)
	{
		return 0; /* parse error */
	}

	/* we should now have a command to parse, find it */
	for (cmd = 0; cmd < CNC_NUM_CMDS; cmd++)
	{
		size_t cmdLen;
		cmdLen = strlen(CnCCmds[cmd]);
		if (sourceLen != cmdLen)
		{
			continue;
		}
		if (strncmp(&recvbuf[firstQuote + 1], CnCCmds[cmd], sourceLen) == 0)
		{
			break;	/* found command */
		}
	}

	if (sourceLen == CNC_NUM_CMDS)
	{
		return 0; /* parse error, command not fund */
	}

	/* at this point, we found a command.  Some command require parsing of arguments */
	if (cmd != CNC_KEY_CMD_LIST_ENABLED)
	{
		transducer = strIndexParseForName(recvbuf, strIndex);
		if (transducer < 0)
		{
			return 0; /* parse error, transducer not found */
		}
	}

	switch (cmd)
	{
	case CNC_KEY_CMD_ENABLE:
		return CNC_ENABLE_FILEFILTER + transducer;
	case CNC_KEY_CMD_DISABLE:
		return CNC_DISABLE_FILEFILTER + transducer;
	case CNC_KEY_CMD_GET_ENABLE:
		return CNC_GET_ENABLE_FILEFILTER + transducer;
	case CNC_KEY_CMD_GET_CONFIG:
		return CNC_GET_CONFIG_FILEFILTER + transducer;
	case CNC_KEY_CMD_LIST_ENABLED:
		return CNC_LIST_ENABLED;
	}

	return 0; /* parse error, transducer not found */
}
/***************************************************************************
* Name: parseCncMessage
*
* Routine Description: This function parses messages received from the CmC
*	port and returns a formatted response message in the same passed buffer.
*
* Returns: N/A
*
**************************************************************************/
void parseCncMessage(char *buf)
{
	int cncCmd;
	BOOL netfilterEnabled, filefilterEnabled;
	char *pString;
	char sucessString[] = { JSON_SUCCESS };
	char errorString[] = { JSON_ERROR };
	char trueString[] = { JSON_TRUE };
	char falseString[] = { JSON_FALSE };
	char neitherString[] = { JSON_NEITHER };
	char filefilterString[] = { JSON_FF };
	char netfilterString[] = { JSON_NF };
	char bothString[] = { JSON_BOTH };

	/* parse the C&C nessage */
	cncCmd = parseGetCommand(buf);

	switch (cncCmd)
	{
	case CNC_INVALID:
		/* invalid command */
		break;
	case CNC_ENABLE_FILEFILTER:
		if (transducerError(TRANSDUCER_FILEFILTER))
		{
			pString = errorString;
		}
		else
		{
			/* set the mode of the filefilter */
			transducerSetMode(TRANSDUCER_FILEFILTER, TRUE);
			/* verify that the filter is in the expected mode */
			if (transducerEnabled(TRANSDUCER_FILEFILTER))
			{
				pString = sucessString;
			}
			else
			{
				pString = errorString;
			}
		}
		snprintf(buf, PAYLOAD_MSG_SZ, JSON_ENABLE JSON_NAME
			JSON_FILEFILTER JSON_STATUS "%s", pString);
		break;
	case CNC_ENABLE_NETFILTER:
		if (transducerError(TRANSDUCER_NETFILTER))
		{
			pString = errorString;
		}
		else
		{
			/* set the mode of the netFilter */
			transducerSetMode(TRANSDUCER_NETFILTER, TRUE);
			/* verify that the filter is in the expected mode */
			if (transducerEnabled(TRANSDUCER_NETFILTER))
			{
				pString = sucessString;
			}
			else
			{
				pString = errorString;
			}
		}
		snprintf(buf, PAYLOAD_MSG_SZ, JSON_ENABLE JSON_NAME
			JSON_NETFILTER JSON_STATUS "%s", pString);
		break;
	case CNC_DISABLE_FILEFILTER:
		if (transducerError(TRANSDUCER_FILEFILTER))
		{
			pString = errorString;
		}
		else
		{
			/* set the mode of the filefilter */
			transducerSetMode(TRANSDUCER_FILEFILTER, FALSE);
			/* verify that the filter is in the expected mode */
			if (!transducerEnabled(TRANSDUCER_FILEFILTER))
			{
				pString = sucessString;
			}
			else
			{
				pString = errorString;
			}
		}
		snprintf(buf, PAYLOAD_MSG_SZ, JSON_DISABLE JSON_NAME
			JSON_FILEFILTER JSON_STATUS "%s", pString);
		break;
	case CNC_DISABLE_NETFILTER:
		if (transducerError(TRANSDUCER_NETFILTER))
		{
			pString = errorString;
		}
		else
		{
			/* set the mode of the netFilter */
			transducerSetMode(TRANSDUCER_NETFILTER, FALSE);
			/* verify that the filter is in the expected mode */
			if (!transducerEnabled(TRANSDUCER_NETFILTER))
			{
				pString = sucessString;
			}
			else
			{
				pString = errorString;
			}
		}
		snprintf(buf, PAYLOAD_MSG_SZ, JSON_DISABLE JSON_NAME
			JSON_NETFILTER JSON_STATUS "%s", pString);
		break;
	case CNC_GET_ENABLE_FILEFILTER:
		if (transducerError(TRANSDUCER_FILEFILTER))
		{
			pString = errorString;
		}
		else
		{
			/* get the filter mode */
			if (transducerEnabled(TRANSDUCER_FILEFILTER))
			{
				pString = trueString;
			}
			else
			{
				pString = falseString;
			}
		}
		snprintf(buf, PAYLOAD_MSG_SZ, JSON_GET_ENABLE JSON_NAME
			JSON_FILEFILTER JSON_STATUS "%s", pString);
		break;
	case CNC_GET_ENABLE_NETFILTER:
		if (transducerError(TRANSDUCER_NETFILTER))
		{
			pString = errorString;
		}
		else
		{
			/* get the filter mode */
			if (transducerEnabled(TRANSDUCER_NETFILTER))
			{
				pString = trueString;
			}
			else
			{
				pString = falseString;
			}
		}
		snprintf(buf, PAYLOAD_MSG_SZ, JSON_GET_ENABLE JSON_NAME
			JSON_NETFILTER JSON_STATUS "%s", pString);
		break;
	case CNC_GET_CONFIG_FILEFILTER:
		snprintf(buf, PAYLOAD_MSG_SZ, JSON_GET_CONFIG JSON_NAME
			JSON_FILEFILTER JSON_CONFIG JSON_NULL);
		break;
	case CNC_GET_CONFIG_NETFILTER:
		snprintf(buf, PAYLOAD_MSG_SZ, JSON_GET_CONFIG JSON_NAME
			JSON_NETFILTER JSON_CONFIG JSON_NULL);
		break;
	case CNC_LIST_ENABLED:
		/* here we have four choices for a return string, neither, 
			just netFilter, just filefilter, or both */
		if (transducerError(TRANSDUCER_NETFILTER))
		{
			netfilterEnabled = FALSE;
		}
		else
		{
			/* get the filter mode */
			netfilterEnabled = transducerEnabled(TRANSDUCER_NETFILTER);
		}
		if (transducerError(TRANSDUCER_FILEFILTER))
		{
			filefilterEnabled = FALSE;
		}
		else
		{
			/* get the filter mode */
			filefilterEnabled = transducerEnabled(TRANSDUCER_FILEFILTER);
		}

		if (!netfilterEnabled && !filefilterEnabled)
			pString = neitherString;
		if (netfilterEnabled && !filefilterEnabled)
			pString = netfilterString;
		if (!netfilterEnabled && filefilterEnabled)
			pString = filefilterString;
		if (netfilterEnabled && filefilterEnabled)
			pString = bothString;

		snprintf(buf, PAYLOAD_MSG_SZ, JSON_LIST_ENABLED JSON_NAME
			"%s", pString);
		break;
	}
}