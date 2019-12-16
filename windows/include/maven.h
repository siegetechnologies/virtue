#pragma once

#define TRANSDUCER_FILEFILTER 0
#define TRANSDUCER_NETFILTER 1

#define MAVEN_MAX_EXT_LENGTH 8
#define MAVEN_MAX_EXT_ENTRIES 16
#define MAVEN_NUM_PROC	2048
#define PAYLOAD_MSG_SZ 1024		/* WCHAR must be large enough for max message */
#define MAVEN_LOG_SIZE 64		/* max entries in log buffer */
#define MAX_USERNAME_LEN 64
#define PACKETDUMP_MSGSIZE 8000 /* max packet dump message size */
#define HEARTBEAT_DELAY 30000	/* ms - every 30 seconds */

/* build time flags */
#define REPORT_STATUS 1			/* report status with message at every heartbeat */

#define MAVEN_CONFIG_FILE_NAME "C:\\maven\\mavenCfg.txt"
#define MAVEN_PORT_NAME L"\\MavenFilterPort"	/* file filter driver port name */
#define PERMISSIONS_CONFIG_FILE_FOLDER_NAME L"\\DriverTest" /* file filter whitelist folder */
#define PERMISSIONS_CONFIG_FILE_FOLDER L"\\??\\C:" PERMISSIONS_CONFIG_FILE_FOLDER_NAME
#define PERMISSIONS_CONFIG_FILE L"maven.csv" /* file filter whitelist file name */
#define PERMISSIONS_CONFIG_FILE_NAME PERMISSIONS_CONFIG_FILE_FOLDER L"\\" PERMISSIONS_CONFIG_FILE

#define MAVEN_HOST_NAME NULL						/* host name of Admin port */
#define MAVEN_PORT_NUMBER "65000"					/* Admin App port name */
#define MAVEN_PORT_NUMBER_BIN 65000					/* Admin App port name */
#define CNC_PORT_NUMBER "65001"						/* C&C port name */
#define CNC_PORT_NUMBER_BIN 65001					/* C&C port name */
#define MAVEN_ADMIN_PORT_NUMBER "5170"				/* fluentd port name */
#define MAVEN_ADMIN_PORT_NUMBER_BIN 5170			/* fluentd port name */
#define FILEFILTER_PORT_NUMBER MAVEN_PORT_NUMBER	/* File Filter App port name */
#define NETFILTER_PORT_NUMBER MAVEN_PORT_NUMBER		/* Net Filter App port name */


#define URL_BLACKLIST_FILENAME "urlBlackList.txt"
#define MAVEN_FOLDER "\\??\\C:\\maven\\" 
#define MAVEN_FOLDER_W L"\\??\\C:\\maven\\" 
#define URL_BLACKLIST_NAME MAVEN_FOLDER URL_BLACKLIST_FILENAME
#define IPV4_BLACKLIST_FILENAME "ipV4BlackList.txt"
#define IPV4_BLACKLIST_NAME MAVEN_FOLDER IPV4_BLACKLIST_FILENAME
#define IPV6_BLACKLIST_FILENAME "ipV6BlackList.txt"
#define IPV6_BLACKLIST_NAME MAVEN_FOLDER IPV6_BLACKLIST_FILENAME
#define IP_BLACKLIST_SIZE 256 /* max addresses in IP blacklist */

#define PKTLOG_FILENAME L"packetLog.txt"
#define NETLOG_FILENAME L"webfilter.txt"
#define FILELOG_FILENAME L"filefilter.txt"
#define PKTLOG_NAME MAVEN_FOLDER_W PKTLOG_FILENAME
#define NETLOG_NAME MAVEN_FOLDER_W NETLOG_FILENAME
#define FILELOG_NAME MAVEN_FOLDER_W FILELOG_FILENAME

#define ADMIN_PORT_NAME L"\\MavenAdminPort"

#define SERVER_ID_STRING "Admin"
#define CLIENT_ID_STRING "Maven"
#define CONSOLE_ID_STRING "Console"
#define FILEFILTER_ID_STRING "FileFilter"
#define NETFILTER_ID_STRING "NetFilter"

/* The SMSSInit subphase begins when the kernel passes control to the 
 * session manager process(Smss.exe).During this subphase, the system 
 * initializes the registry, loads and starts the devices and drivers 
 * that are not marked BOOT_START, and starts the subsystem processes.
 * SMSSInit ends when control is passed to Winlogon.exe.
 */
#define BOOT_SEQUENCE_END_NAME L"winlogon.exe" /* end function in boot sequence  */

#define LEARN_KEY L'*'

 /* message IDs from server to mavenFileFilter app */
typedef enum _MAVEN_SERVER_NET_COMMAND {
	netCmdFilter,			/* set mavenNetFilter to filter mode */
	netCmdIdle,				/* set mavenNetFilter to idle mode */
	netCmdExit,				/* exit mavenNetFilter */
	netCmdStatus,			/* read status from driver and send to server */
	netCmdInvalid			/* unable to parse */
}MAVEN_SERVER_NET_COMMAND;
#define SERVER_NET_NUM_CMDS 4
#define SERVER_NET_CMD_FILTER NETFILTER_ID_STRING " Filter"
#define SERVER_NET_CMD_IDLE NETFILTER_ID_STRING " Idle"
#define SERVER_NET_CMD_EXIT NETFILTER_ID_STRING " Exit"
#define SERVER_NET_CMD_STATUS NETFILTER_ID_STRING " Status"
#define SERVER_CMD_NET_PREFIX NETFILTER_ID_STRING

 /* message IDs from server to mavenFileFilter app */
typedef enum _MAVEN_SERVER_COMMAND {
	cmdFilter,			/* set mavenFilter to filter mode */
	cmdLearn,			/* set mavenFilter to lern mode */
	cmdIdle,			/* set mavenFilter to idle mode */
	cmdReadWhitelist,	/* read whitelist from file and load in driver */
	cmdWriteWhitelist,	/* read whitelist from driver and save to file */
	cmdReset,			/* erase whitelist driver and set idle */
	cmdStatus,			/* read status from driver and send to server */
	cmdReadVersion,		/* read mavenFilter version */
	cmdReadLog,			/* read a log entry from driver */
	cmdExit,			/* exit mavenFilter */
	cmdInvalid			/* unable to parse */
}MAVEN_SERVER_COMMAND;
#define SERVER_NUM_CMDS 10
#define SERVER_CMD_FILTER FILEFILTER_ID_STRING " Filter"
#define SERVER_CMD_LEARN FILEFILTER_ID_STRING " Learn"
#define SERVER_CMD_IDLE FILEFILTER_ID_STRING " Idle"
#define SERVER_CMD_READWHITELIST FILEFILTER_ID_STRING " ReadWhitelist"
#define SERVER_CMD_WRITEWHITELIST FILEFILTER_ID_STRING " WriteWhitelist"
#define SERVER_CMD_RESET FILEFILTER_ID_STRING " Reset"
#define SERVER_CMD_STATUS FILEFILTER_ID_STRING " Status"
#define SERVER_CMD_READVERSION FILEFILTER_ID_STRING " ReadVersion"
#define SERVER_CMD_READLOG FILEFILTER_ID_STRING " ReadLog"
#define SERVER_CMD_EXIT FILEFILTER_ID_STRING " Exit"
#define SERVER_CMD_FILE_PREFIX FILEFILTER_ID_STRING

#define SERVER_CMD_SIZE PAYLOAD_MSG_SZ /* max size of server command */

#define SERVER_RESPONSE_SIZE PAYLOAD_MSG_SZ /* max size of server response */

typedef enum _MAVEN_RESPONSE {
	mavenError,		/* ERROR response to last command */
	mavenSuccess,	/* SUCCESS response to last command */
} MAVEN_RESPONSE;
#define MAVEN_RESPONSE_ERROR "FileFilter Error"
#define MAVEN_RESPONSE_SUCCESS "FileFilter Success"
#define MAVEN_RESPONSE_STATUS "FileFilter Status"

/* message IDs from the user app to driver */
typedef enum _MAVEN_COMMAND {
	mavenModeSetIdle,	/* set driver mode to idle */
	mavenModeSetLearn,	/* set driver mode to Learn */
	mavenModeSetFilter,	/* set driver mode to Filter */
	mavenModeSetReset,	/* set driver mode to Reset */
	mavenGetVersion,	/* Get driver version number */
	mavenReadConfig,	/* Read next configuration record, return Error if done */
	mavenWriteConfig,	/* Write configuration record to driver */
	mavenGetStatus,		/* Read current driver state */
	mavenGetLog			/* Get log entry */
} MAVEN_COMMAND;

typedef enum _OPERATING_MODE {
	MAVEN_BOOT,		/* boot sequence */
	MAVEN_IDLE,		/* wait for a processing command (default) */
	MAVEN_LEARN,	/* monitor file accesses and expand configuration accordingly */
	MAVEN_FILTER	/* use configuration to filter file access */
} OPERATING_MODE;

typedef enum _NETFILTER_STATE {
	NETFILTER_IDLE = 0,
	NETFILTER_FILTER = 1
}NETFILTER_STATE;

typedef struct __declspec(align(16)) _MAVEN_CMD_MESSAGE {
	MAVEN_COMMAND msgType;
	WCHAR payload[PAYLOAD_MSG_SZ];	/* message payload (optional) */
} MAVEN_CMD_MESSAGE;

typedef struct __declspec(align(16)) _MAVEN_RESPONSE_MESSAGE {
	MAVEN_RESPONSE msgType;
	WCHAR payload[PAYLOAD_MSG_SZ];	/* message payload (optional) */
} MAVEN_RESPONSE_MESSAGE;

