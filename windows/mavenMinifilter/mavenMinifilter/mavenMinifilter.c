/*++

Module Name: mavenMinifilter.c

Abstract:

This driver implements a mini-filter service for the monitoring and control of processes in the target machine.  
Filtering restricts the initialization of processeses using a process permissions list.  The list is found at a 
specific location at startup and read into the system.  This defines the set of applications that are permitted
to run.  An attempt to initialize an image that is not found in that list is prohibited.

The driver supports a "Learn" mode that is used to record applications that run.  This can be used to construct
a process permissions list for later use.  If this file is not found during driver initialzation, it defaults 
to learn mode and begins to construct a new list.

The driver works in cooperation with a user application that manages driver state changes and controls uploading
downloading of process permissions configuration files.

Environment:

Kernel mode

Revision History:
Author Richard Carter - Siege Technologies, Manchester NH

V1.1	11/05/2017	- REC, 12/4 demo
V1.2	11/28/2017	- REC, Add log function
V1.3	12/16/2017	- REC, Add BOOT_MODE
V1.4	12/23/2017	- REC, Add blacklist
V1.5	12/26/2017	- REC, Create gData structure
V1.6	01/15/2018	- REC, Bugfix, blacklist init problem and file lock fix
V1.7	03/29/2018	- REC, Bugfix, log size error if log full
V1.8	04/26/2018	- REC, test code to stay in idle mode during boot

--*/

#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>

#include <BaseTsd.h>
#include <WinDef.h>
#include <stdio.h> 
#include <wchar.h>
#include <Sddl.h>
#include "mavenMinifilter.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

WCHAR mavenDriverVersion[] = L"mavenMinifilter 1.7";

PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

#define MAVEN_DEBUG
#ifdef MAVEN_DEBUG 
#define DEBUG_LOG(arg) logStatusMessage(arg, NULL)
#else
#define DEBUG_LOG(arg)
#endif


#define BACKSLASH 0x5c

ULONG gTraceFlags = 0;


#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

EXTERN_C_START


/*************************************************************************
Datatypes
*************************************************************************/



/*************************************************************************
Prototypes
*************************************************************************/

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
mavenMinifilterInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

VOID
mavenMinifilterInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

VOID
mavenMinifilterInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

NTSTATUS
mavenMinifilterUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS
mavenMinifilterInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
mavenMinifilterRestrictSync(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);
FLT_PREOP_CALLBACK_STATUS
mavenMinifilterRestrictCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

FLT_PREOP_CALLBACK_STATUS
mavenMinifilterPreOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

VOID
mavenMinifilterOperationStatusCallback(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
	_In_ NTSTATUS OperationStatus,
	_In_ PVOID RequesterContext
);

FLT_POSTOP_CALLBACK_STATUS
mavenMinifilterPostOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
mavenMinifilterPreOperationNoPostOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

BOOLEAN
mavenMinifilterDoRequestOperationStatus(
	_In_ PFLT_CALLBACK_DATA Data
);

BOOL mavenBlacklisted(
	_Inout_ PFLT_CALLBACK_DATA Data
);

// Undocumented internal windows functions needed to get
// executable file path.
NTSTATUS NtQueryInformationProcess(
	_In_      HANDLE           ProcessHandle,
	_In_      PROCESSINFOCLASS ProcessInformationClass,
	_Out_     PVOID            ProcessInformation,
	_In_      ULONG            ProcessInformationLength,
	_Out_opt_ PULONG           ReturnLength
);

void logMessage(WCHAR *pMessage);
void logStatusMessage(
	WCHAR *pProcessImageName,
	WCHAR *nameInfo);

PWCH getFileName(PWCH  pImageName);

MAVEN_PROCESS_PERMISSIONS *findProcess(
	PUNICODE_STRING  pProcessImageName
);

MAVEN_PROCESS_PERMISSIONS *addProcess(
	PUNICODE_STRING  pProcessImageName
);

BOOL checkPermission(
	MAVEN_PROCESS_PERMISSIONS *pPermissionsEntry,
	MAVEN_EXTENSION extension
);

BOOL addPermission(
	MAVEN_PROCESS_PERMISSIONS *pPermissionsEntry,
	MAVEN_EXTENSION extension
);

MAVEN_PROCESS_PERMISSIONS *updateProcessList(
	PUNICODE_STRING  pProcessImageName,
	MAVEN_EXTENSION extension
);

BOOL checkProcessPermissions(
	PUNICODE_STRING  pProcessImageName,
	USHORT Length,
	MAVEN_EXTENSION extension
);

NTSTATUS MavenPortConnect(
	__in PFLT_PORT ClientPort,
	__in_opt PVOID ServerPortCookie,
	__in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
	__in ULONG SizeOfContext,
	__deref_out_opt PVOID *ConnectionCookie
);

VOID MavenPortDisconnect(
	__in_opt PVOID ConnectionCookie
);
NTSTATUS
MavenMessage(
	__in PVOID ConnectionCookie,
	__in_bcount_opt(InputBufferSize) PVOID InputBuffer,
	__in ULONG InputBufferSize,
	__out_bcount_part_opt(OutputBufferSize, *pReturnOutputBufferLength) PVOID OutputBuffer,
	__in ULONG OutputBufferSize,
	__out PULONG pReturnOutputBufferLength
);

ULONG GetUserSID(WCHAR *pStringSid);
CHAR *nextField(CHAR *pString, CHAR delimiter);
USHORT findChar(CHAR *pString, CHAR delimiter);
WCHAR *nextWField(WCHAR *pString, WCHAR delimiter);
int scanForDecimal(CHAR *pString, UINT32 *pValue, CHAR delimiter);
int scanWForDecimal(WCHAR *pString, UINT32 *pValue, WCHAR delimiter);
NTSTATUS configFileLoad(BOOL*pKeyFound,	PUNICODE_STRING filename);
void charToWide(WCHAR* dest, const CHAR* source);
void csvToText(CHAR* buf);
void clearConfigData(void);

EXTERN_C_END

/*
*  Assign text sections for each routine.
*
* Code that runs at IRQL >= DISPATCH_LEVEL must be memory - resident.That is, this 
* code must be either in a nonpageable segment, or in a pageable segment that is 
* locked in memory. If code that is running at IRQL >= DISPATCH_LEVEL causes a page 
* fault, a bug check occurs.Drivers can use the PAGED_CODE macro to verify that 
* pageable functions are called only at appropriate IRQLs.
*/
#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, mavenMinifilterUnload)
#pragma alloc_text(PAGE, mavenMinifilterInstanceQueryTeardown)
#pragma alloc_text(PAGE, mavenMinifilterInstanceSetup)
#pragma alloc_text(PAGE, mavenMinifilterInstanceTeardownStart)
#pragma alloc_text(PAGE, mavenMinifilterInstanceTeardownComplete)
#pragma alloc_text(PAGE, MavenPortConnect)
#pragma alloc_text(PAGE, MavenPortDisconnect)
#pragma alloc_text(PAGE, MavenMessage)
#pragma alloc_text(PAGE, configFileLoad)
#pragma alloc_text(PAGE, charToWide)
#pragma alloc_text(PAGE, csvToText)

#endif

//
// Global data
//
typedef struct _LOG_BUFFER {
	MAVEN_RESPONSE_MESSAGE		mavenLog[MAVEN_LOG_SIZE];	/* ring buffer */
	int							mavenLogFirstIndex;	/* points to first used log entry */
	int							mavenLogLastIndex;	/* points to last used log entry */
}LOG_BUFFER;

/* The kernel debugger doesn't display global data.  Put it
 * in a structure to make debug easier */
typedef struct _GDATA {
	LOG_BUFFER					mavenMessageLog;
	PDRIVER_OBJECT				g_mavenMinifilterDriverObject;
	MAVEN_PROCESS_PERMISSIONS	permissionsList[MAVEN_NUM_PROC];	// list of permitted processes and their permitted target files
	OPERATING_MODE				operatingMode;		/* current driver operating mode */
	PFLT_PORT					mavenServerPort;	/* Listens for incoming commands. */
	PEPROCESS					mavenUserProcess;	/* User process that connected to the port */
	PFLT_PORT					mavenClientPort;	/* Client port for a connection to user-mode */
	PWCHAR						blacklist[IP_BLACKLIST_SIZE]; /* array of blacklist strings */
	UINT						blacklistEnd;		/* next free blacklist entry */
}GDATA;
GDATA gData;	

/* NOTE: if these below don't work, look at loadImageNotifyRoutine and
*  CreateProcessNotifyRoutine as an alternative
*/

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
	/* file open/create filter */
	{ IRP_MJ_CREATE,
	0,
	mavenMinifilterRestrictCreate,
	mavenMinifilterPostOperation },

	/* process mapping filter */
	{ IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION,
	0,
	mavenMinifilterRestrictSync,
	mavenMinifilterPostOperation },

#if 0 // List all unused filters.
	{ IRP_MJ_CREATE_NAMED_PIPE,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_CLOSE,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_READ,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_WRITE,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_QUERY_INFORMATION,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_SET_INFORMATION,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_QUERY_EA,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_SET_EA,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_FLUSH_BUFFERS,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_QUERY_VOLUME_INFORMATION,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_SET_VOLUME_INFORMATION,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_DIRECTORY_CONTROL,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_FILE_SYSTEM_CONTROL,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_DEVICE_CONTROL,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_INTERNAL_DEVICE_CONTROL,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_SHUTDOWN,
	0,
	mavenMinifilterPreOperationNoPostOperation,
	NULL },                               //post operations not supported

	{ IRP_MJ_LOCK_CONTROL,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_CLEANUP,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_CREATE_MAILSLOT,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_QUERY_SECURITY,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_SET_SECURITY,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_QUERY_QUOTA,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_SET_QUOTA,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_PNP,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_ACQUIRE_FOR_MOD_WRITE,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_RELEASE_FOR_MOD_WRITE,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_ACQUIRE_FOR_CC_FLUSH,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_RELEASE_FOR_CC_FLUSH,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_NETWORK_QUERY_OPEN,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_MDL_READ,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_MDL_READ_COMPLETE,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_PREPARE_MDL_WRITE,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_MDL_WRITE_COMPLETE,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_VOLUME_MOUNT,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

	{ IRP_MJ_VOLUME_DISMOUNT,
	0,
	mavenMinifilterPreOperation,
	mavenMinifilterPostOperation },

#endif

	{ IRP_MJ_OPERATION_END }
};

CONST WCHAR *modeString[] = { L"Boot", L"Idle",  L"Learn", L"Filter" };


//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

	sizeof(FLT_REGISTRATION),				//  Size
	FLT_REGISTRATION_VERSION,				//  Version
	0,										//  Flags

	NULL,									//  Context
	Callbacks,								//  Operation callbacks

	mavenMinifilterUnload,                    //  MiniFilterUnload

	mavenMinifilterInstanceSetup,             //  InstanceSetup
	mavenMinifilterInstanceQueryTeardown,     //  InstanceQueryTeardown
	mavenMinifilterInstanceTeardownStart,     //  InstanceTeardownStart
	mavenMinifilterInstanceTeardownComplete,  //  InstanceTeardownComplete

	NULL,                                   //  GenerateFileName
	NULL,                                   //  GenerateDestinationFileName
	NULL                                    //  NormalizeNameComponent

};


NTSTATUS
mavenMinifilterInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
/*++

Routine Description:

This routine is called whenever a new instance is created on a volume. This
gives us a chance to decide if we need to attach to this volume or not.

If this routine is not defined in the registration structure, automatic
instances are always created.

Arguments:

FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance and its associated volume.

Flags - Flags describing the reason for this attach request.

Return Value:

STATUS_SUCCESS - attach
STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);
	UNREFERENCED_PARAMETER(VolumeFilesystemType);

	PAGED_CODE();


	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("mavenMinifilter!mavenMinifilterInstanceSetup: Entered\n"));

	return STATUS_SUCCESS;
}


NTSTATUS
mavenMinifilterInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

This is called when an instance is being manually deleted by a
call to FltDetachVolume or FilterDetach thereby giving us a
chance to fail that detach request.

If this routine is not defined in the registration structure, explicit
detach requests via FltDetachVolume or FilterDetach will always be
failed.

Arguments:

FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance and its associated volume.

Flags - Indicating where this detach request came from.

Return Value:

Returns the status of this operation.

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("mavenMinifilter!mavenMinifilterInstanceQueryTeardown: Entered\n"));

	return STATUS_SUCCESS;
}


VOID
mavenMinifilterInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

This routine is called at the start of instance teardown.

Arguments:

FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance and its associated volume.

Flags - Reason why this instance is being deleted.

Return Value:

None.

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("mavenMinifilter!mavenMinifilterInstanceTeardownStart: Entered\n"));
}


VOID
mavenMinifilterInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

This routine is called at the end of instance teardown.

Arguments:

FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance and its associated volume.

Flags - Reason why this instance is being deleted.

Return Value:

None.

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("mavenMinifilter!mavenMinifilterInstanceTeardownComplete: Entered\n"));
}


/*************************************************************************
MiniFilter initialization 
*************************************************************************/

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
/*++

Routine Description:

This is the initialization routine for this miniFilter driver.  This
registers with FltMgr and initializes all global data structures.

Arguments:

DriverObject - Pointer to driver object created by the system to
represent this driver.

RegistryPath - Unicode string identifying where the parameters for this
driver are located in the registry.

Return Value:

Routine can return non success error codes.

--*/
{
	NTSTATUS status, configStatus;
	OBJECT_ATTRIBUTES objAttr;
	UNICODE_STRING unicodeString;
	PSECURITY_DESCRIPTOR sd;
	UNICODE_STRING filename;
	BOOL keyFound;
	int index;
	GDATA *pGdata = &gData;


	//ASSERT(FALSE); /* This will break to debugger */
	memset(pGdata, 0, sizeof(gData));	/* initialize global data */

	pGdata->operatingMode = MAVEN_IDLE;

    /* Store our driver object. */
	pGdata->g_mavenMinifilterDriverObject = DriverObject;

	UNREFERENCED_PARAMETER(RegistryPath);

	/* initialize blacklist */
	for (index = 0; index < IP_BLACKLIST_SIZE; index++) {
		pGdata->blacklist[index] = NULL;
	}
	pGdata->blacklistEnd = 0;

	/* initialize the message log */
	for (index = 0; index < MAVEN_LOG_SIZE; index++) {
		pGdata->mavenMessageLog.mavenLog[index].msgType = mavenError;
	}
	pGdata->mavenMessageLog.mavenLogFirstIndex = 0;	/* points to first used log entry */
	pGdata->mavenMessageLog.mavenLogLastIndex = MAVEN_LOG_SIZE - 1;	/* points to last used log entry */

	memset(pGdata->permissionsList, 0, sizeof(pGdata->permissionsList)); /* initialize permissions list */

	RtlInitUnicodeString(&filename, PERMISSIONS_CONFIG_FILE_NAME);

	/*  Register with FltMgr to tell it our callback routines */
	status = FltRegisterFilter(DriverObject,
		&FilterRegistration,
		&gFilterHandle);

	FLT_ASSERT(NT_SUCCESS(status));

	/*  Create a communication port. */
	RtlInitUnicodeString(&unicodeString, MAVEN_PORT_NAME);

	/* We secure the port so only ADMINs & SYSTEM can access it. */
	status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);

	if (NT_SUCCESS(status)) {
		InitializeObjectAttributes(
			&objAttr,
			&unicodeString,
			OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
			NULL,
			sd
		);

		status = FltCreateCommunicationPort(
			gFilterHandle,
			&pGdata->mavenServerPort,
			&objAttr,
			NULL,
			MavenPortConnect,
			MavenPortDisconnect,
			MavenMessage,
			1 /* MaxConnections: Only accept one connection! */
		);
	}

	/*
	* Free the security descriptor in all cases. It is not needed once
	* the call to FltCreateCommunicationPort() is made.
	*/
	FltFreeSecurityDescriptor(sd);

	if (NT_SUCCESS(status)) {

		/*  Start filtering i/o */
		status = FltStartFiltering(gFilterHandle);

		if (!NT_SUCCESS(status)) {

			FltUnregisterFilter(gFilterHandle);
		}
	}

	/* Open configuration file */
	configStatus = configFileLoad(&keyFound,	&filename);

	/* When booting, there is a race condition related to the order that processes
	* start.  The maven filter may not always start at in the exact order relative
	* to the other kernel processes.  In order to fill-in the subsequent kernel,
	* processes, it is necessary to boot the board in Learn mode several times
	* to insure that you teach the system to expect all the valid processes
	* it may see at runtime.  Putting a special key as the first character
	* in the cvs file causes the driver to automatically run operate in mode at
	* bootup.  Save the new learned mode off and boot again. Hand edit the
	* saved new config file by putting this key at the start of the file.
	* Repeat until the number of records recorded stops growing.
	*/
	if ((configStatus == STATUS_SUCCESS) && (!keyFound)) {
		pGdata->operatingMode = MAVEN_BOOT;
	}
	else {
#if 0 /* test code to avoid an RDP issue during phase-1 testing. */
		/* this causes the board to stay in idle mode until commanded */
		pGdata->operatingMode = MAVEN_LEARN;
#endif
	}

	return status;
}

NTSTATUS mavenMinifilterUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
)
{
/*++

Routine Description:

This is the unload routine for this miniFilter driver. This is called
when the minifilter is about to be unloaded. We can fail this unload
request if this is not a mandatory unload indicated by the Flags
parameter.

Arguments:

Flags - Indicating if this is a mandatory unload.

Return Value:

Returns STATUS_SUCCESS.

--*/

	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	GDATA *pGdata = &gData;
	UINT index;

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("mavenMinifilter!mavenMinifilterUnload: Entered\n"));

	pGdata->operatingMode = MAVEN_IDLE;
	FltUnregisterFilter(gFilterHandle);

	clearConfigData();

	/* free blacklist entries */
	for (index = 0; index < pGdata->blacklistEnd; index++) {
		if (pGdata->blacklist[index] != NULL) {
			ExFreePoolWithTag(pGdata->blacklist[index], 'nvaM');
			pGdata->blacklist[index] = NULL;
		}
	}

	return STATUS_SUCCESS;
}

/***************************************************************************
 * Name: getProcessImageName
 *
 * Routine Description:
 *	This function obtains the path string of the originating process.
 *
 * Returns: unicode string pointer.
 *
 **************************************************************************/
PUNICODE_STRING getProcessImageName(
	PEPROCESS eProcess /* pointer to process descriptor */
)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	ULONG returnedLength;
	HANDLE hProcess = NULL;
	PUNICODE_STRING pProcessImageName = NULL;

	PAGED_CODE(); /* this eliminates the possibility of the IDLE Thread/Process */

	if (eProcess == NULL)
	{
		return NULL;
	}

	status = ObOpenObjectByPointer(eProcess,
		0, NULL, 0, 0, KernelMode, &hProcess);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("ObOpenObjectByPointer Failed: %08x\n", status);
		return NULL;
	}

	/* Query the actual size of the process path 
	 * Note: NtQueryInformationProcess is undocumented
	 *       and not intended for public use.
	 */
	status = NtQueryInformationProcess(hProcess,
		ProcessImageFileName,
		NULL, // buffer
		0,    // buffer size
		&returnedLength);

	if (STATUS_INFO_LENGTH_MISMATCH != status) {
		DbgPrint("NtQueryInformationProcess status = %x\n", status);
		goto cleanUp;
	}

	pProcessImageName = ExAllocatePoolWithTag(NonPagedPool,
		returnedLength,
		'nvaM');

	if (pProcessImageName == NULL)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanUp;
	}

	/* Retrieve the process path from the handle to the process */
	status = NtQueryInformationProcess(hProcess,
		ProcessImageFileName,
		pProcessImageName,
		returnedLength,
		&returnedLength);

	if (!NT_SUCCESS(status)) {

		ExFreePoolWithTag(pProcessImageName, 'nvaM');
		pProcessImageName = NULL;
	}
cleanUp:

	NtClose(hProcess);
	
	return pProcessImageName;
}

/***************************************************************************
* Name: GetUserSID
*
* Routine Description: This function obtains the user SID associated with
*	a process.
*
* Returns: Number of TCHARs returned.
*
**************************************************************************/

ULONG GetUserSID(WCHAR *pStringSid) {
	NTSTATUS ntStatus;
	HANDLE tokenHandle = NULL;
	PTOKEN_USER pUserAttributes = NULL;
	ULONG bufferLength = 0;
	size_t sidLength = 0;

	UNICODE_STRING unicodeString;  /* Research says that max string is 256 */
	unicodeString.Length = 256;
	unicodeString.MaximumLength = PAYLOAD_MSG_SZ;
	unicodeString.Buffer = pStringSid;

	if (pStringSid == NULL) {
		return 0;	/* error */
	}

	/* get token for current thread or process */
	ntStatus = ZwOpenThreadTokenEx(ZwCurrentThread(), GENERIC_READ, TRUE, OBJ_KERNEL_HANDLE, &tokenHandle);

	if (ntStatus != STATUS_SUCCESS)
	{
		ntStatus = ZwOpenProcessTokenEx(ZwCurrentProcess(), GENERIC_READ, OBJ_KERNEL_HANDLE, &tokenHandle);
		if (!tokenHandle) {
			return 0;	/* error */
		}
	}

	/* find the size of the buffer needed */
	ntStatus = ZwQueryInformationToken(
		tokenHandle,
		TokenUser,
		NULL,
		0,
		&bufferLength);

	if (ntStatus != STATUS_BUFFER_TOO_SMALL) {
		return 0;	/* error */
	}

	/* allocate buffer */
	pUserAttributes = ExAllocatePoolWithTag(
		NonPagedPool,
		bufferLength,
		'nvaM');

	if (pUserAttributes == NULL) {
		return 0;
	}

	/* convert token to SID */
	
	ntStatus = ZwQueryInformationToken(
		tokenHandle, 
		TokenUser, 
		pUserAttributes,
		bufferLength,
		&bufferLength);

	if ( (!NT_SUCCESS(ntStatus)) || (!NT_SUCCESS(RtlValidSid(pUserAttributes->User.Sid)))) {
		ExFreePoolWithTag(pUserAttributes, 'nvaM');
		goto cleanup;
	}

	/* convert to string */
	if (NT_SUCCESS(RtlConvertSidToUnicodeString(&unicodeString, pUserAttributes->User.Sid, FALSE))) {
		sidLength = wcslen(unicodeString.Buffer);
	}

cleanup:
	if (pUserAttributes != NULL) {
		ExFreePoolWithTag(pUserAttributes, 'nvaM');
	}

	if (tokenHandle != NULL) {
		ZwClose(tokenHandle);
	}

	return (ULONG)sidLength;
}

/***************************************************************************
* Name: compareStringWildcard
*
* Routine Description: This function compares two strings and returns
*	true if they match.  The comparison terminates if a wild-card is found
*	in the first string.
* Returns:  True if match
*
**************************************************************************/
BOOL compareStringWildcard(
	PWCHAR firstString,
	PWCHAR secondString
)
{
	int i = 0;
	if ((firstString == NULL) || (secondString == NULL)) {
		return FALSE;	/* can't compare null strings */
	}
	while ((firstString[i] != 0) && (secondString[i] != 0)) {
		/* if we found a wildcard */
		if (firstString[i] == '*') {
			return TRUE; /* strings match */
		}
		/* if strings don't match */
		if (firstString[i] != secondString[i]) {
			return FALSE;
		}
		i++;	/* next character*/
	}
	/* No wildcard and so far match. See if strings are same length */
	if ((firstString[i] == 0) && (secondString[i] == 0)) {
		return TRUE; /* strings match */
	}
	else {
		return FALSE;
	}
}


/***************************************************************************
* Name: logGetSize
*
* Routine Description: This function calculates log size
* Returns:  N/A
*
**************************************************************************/
int logGetSize(void)
{
	GDATA *pGdata = &gData;
	int i;
	i = (pGdata->mavenMessageLog.mavenLogLastIndex + 1) % MAVEN_LOG_SIZE;
	i -= pGdata->mavenMessageLog.mavenLogFirstIndex;
	if (i < 0)
	{
		i += MAVEN_LOG_SIZE;
	}

	/* look for full count (size oveflow) */
	if (i == 0)
	{
		int index = pGdata->mavenMessageLog.mavenLogLastIndex + 1;

		/* wrap if at end */
		if (index == MAVEN_LOG_SIZE) {
			index = 0;
		}
		if (pGdata->mavenMessageLog.mavenLog[index].msgType != mavenError)
		{
			return MAVEN_LOG_SIZE; /* not empty, full! */
		}
	}
	return i;
}


/***************************************************************************
* Name: logMessage
*
* Routine Description: This function logs a text message
* Returns:  N/A
*
**************************************************************************/
void logMessage(WCHAR *pMessage)
{
	GDATA *pGdata = &gData;
	MAVEN_RESPONSE_MESSAGE *pMavenLog;
	int index = pGdata->mavenMessageLog.mavenLogLastIndex + 1;

	/* wrap if at end */
	if (index == MAVEN_LOG_SIZE) {
		index = 0;
	}

	pMavenLog = &pGdata->mavenMessageLog.mavenLog[index];
	if (pMavenLog->msgType != mavenError) {

		return;	/* no room */
	}

	(void)swprintf(pMavenLog->payload, L"DEBUG - %ls\n", pMessage);

	pMavenLog->msgType = mavenSuccess;
	pGdata->mavenMessageLog.mavenLogLastIndex = index;

}
/***************************************************************************
* Name: logStatusMessage
*
* Routine Description: This function logs a blocked attempt to open a
*	file.
*
* Returns:  N/A
*
**************************************************************************/
void logStatusMessage(
	WCHAR *pProcessImageName,
	WCHAR *nameInfo)
{
	GDATA *pGdata = &gData;
	MAVEN_RESPONSE_MESSAGE *pMavenLog;
	int index = pGdata->mavenMessageLog.mavenLogLastIndex + 1;
	UINT sidLength;	

	/* wrap if at end */
	if (index == MAVEN_LOG_SIZE) {
		index = 0;
	}

	pMavenLog = &pGdata->mavenMessageLog.mavenLog[index];
	if (pMavenLog->msgType != mavenError) {
		
		return;	/* no room */
	}

	sidLength = GetUserSID(pMavenLog->payload);

	
	if (nameInfo != NULL) {
		if (sidLength != 0) {
			(void)swprintf(
				&pMavenLog->payload[sidLength],
				L"\t %ls, %ls\n",
				pProcessImageName,
				nameInfo);
		}
		else {
			(void)swprintf(
				pMavenLog->payload,
				L"UNKNOWN_SID \t %ls, %ls\n",
				pProcessImageName,
				nameInfo);
		}
	}
	else {
		if (sidLength != 0) {
			(void)swprintf(
				&pMavenLog->payload[sidLength],
				L"\t %ls\n",
				pProcessImageName);
		}
		else {
			(void)swprintf(
				pMavenLog->payload,
				L"UNKNOWN_SID \t %ls\n",
				pProcessImageName);
		}
	}
	pMavenLog->msgType = mavenSuccess;
	pGdata->mavenMessageLog.mavenLogLastIndex = index;

}

/***************************************************************************
* Name: getFileName
*
* Routine Description: This function scans a file path and advances past 
* back-slashes to return the target file name
*
* Returns: unicode string pointer to file name .
*
**************************************************************************/
PWCH getFileName(
	PWCH  pImageName
)
{
	PWCH  pLastField = pImageName;
	if ((pImageName == 0) || (*pImageName == 0)) {
		return pImageName;
	}
	while (*pImageName != 0) {
		if (*pImageName == BACKSLASH) { // look for backslash
			pLastField = pImageName + 1;
		}
		pImageName++;
	}
	
	return pLastField;
}


/***************************************************************************
* Name: findProcess
*
* Routine Description: This function locates a process in the permissions 
*	list using the process full path name.  It returns a pointer to the 
*   requested list entry or null if not found
*
* Returns: pointer to process permissions record or NULL if not found.
*
**************************************************************************/
MAVEN_PROCESS_PERMISSIONS *findProcess(
	PUNICODE_STRING  pProcessImageName
)
{
	INT index;
	GDATA *pGdata = &gData;

	// search through permissions list for a match
	for (index = 0; index < MAVEN_NUM_PROC; index++) {
		if (pGdata->permissionsList[index].pProcessImageName == NULL)
			break;	// end of list
		if (wcscmp(pProcessImageName->Buffer,
			pGdata->permissionsList[index].pProcessImageName->Buffer) == 0) {
			pGdata->permissionsList[index].count++; // increment access count
			return &pGdata->permissionsList[index]; // found match
		}
	}
	
	return NULL; // nothing found
}

/***************************************************************************
* Name: addProcess
*
* Routine Description: This function adds a process entry to the permissions 
* list.
*
* Returns:  It returns a pointer to the new list entry or NULL if error.
*
**************************************************************************/
MAVEN_PROCESS_PERMISSIONS *addProcess(
	PUNICODE_STRING  pProcessImageName
)
{
	INT index;
	GDATA *pGdata = &gData;

	// search through permissions list for a match
	for (index = 0; index < MAVEN_NUM_PROC; index++) {
		if (pGdata->permissionsList[index].pProcessImageName == NULL) {
			pGdata->permissionsList[index].pProcessImageName = pProcessImageName;
			pGdata->permissionsList[index].count = 1; /* first reference */
			return &pGdata->permissionsList[index];	// new entry found
		}
	}
	
	return NULL; // no room
}

/***************************************************************************
* Name: checkPermission
*
* Routine Description: This function checks to see if a file extension is 
*  in the process permissions list for this process.
*
* Returns:  returns TRUE if OK or if no extension is passed in
*
**************************************************************************/
BOOL checkPermission(
	MAVEN_PROCESS_PERMISSIONS *pPermissionsEntry,/* pointer to permissions record */
	MAVEN_EXTENSION extension				/* file extension, e.g. .bat*/
)
{
	INT index;

	if (extension == NULL)
		return TRUE; /* assume no target extension */

	/* Note: There's a limited amount of space to store file extensions.  Apps like
	 * svchost open files with dozens of different extensions.  To avoid problems, 
	 * we use a wild-card in the first entry if we run out of room *
	 /

	/* if wildcard */
	if (*pPermissionsEntry->extensions[0] == L'*')
		return TRUE; /* Wildcard, only valid at first entry */

	/* search through file extension list for this app */
	for (index = 0; index < MAVEN_MAX_EXT_ENTRIES; index++) {
		if (*pPermissionsEntry->extensions[index] == 0)
			return FALSE; // End of list
		if (wcsncmp(extension, pPermissionsEntry->extensions[index], 
			MAVEN_MAX_EXT_LENGTH -1) == 0) {
			return TRUE; /* found match */
		}
	}
	
	return FALSE; /* list full and not found */

}

/***************************************************************************
* Name: addPermission
*
* Routine Description: This function adds a file extension to the permissions 
* list for this process.
*
* Returns:  returns TRUE if Success
*
**************************************************************************/
BOOL addPermission(
	MAVEN_PROCESS_PERMISSIONS *pPermissionsEntry,
	MAVEN_EXTENSION extension
)
{
	INT index;

	if (extension == NULL)
		return TRUE; /* assume no target extension, nothing to add */

	/* find unused entry */
	for (index = 0; index < MAVEN_MAX_EXT_ENTRIES; index++) {
		if (*pPermissionsEntry->extensions[index] == 0) {
			// add extension to end of list
			wcsncpy(
				pPermissionsEntry->extensions[index],// destination
				extension,							// source
				MAVEN_MAX_EXT_LENGTH - 1);			// max length
			return TRUE; // added entry
		}
	}
	/* no room, put a wildcard in the first position.  All match from now on.*/
	wcscpy(
		pPermissionsEntry->extensions[0],	// destination
		L"*");								// source = wildcard
	
	return FALSE;

}

/***************************************************************************
* Name: updateProcessList
*
* Routine Description: This function is used in learn mode to add entries 
* to the permissions list. 
*
* Returns:  returns pointer to the permissions list entry or NULL if no room
*
**************************************************************************/
MAVEN_PROCESS_PERMISSIONS *updateProcessList(
	PUNICODE_STRING  pProcessImageName,
	MAVEN_EXTENSION extension
)
{
	PWCH pFileName;
	MAVEN_PROCESS_PERMISSIONS *pPermissionsEntry;

	if ((pProcessImageName == NULL) || (pProcessImageName->Buffer == NULL)) {
		return NULL; /* can't process */
	}
	pPermissionsEntry = findProcess(pProcessImageName);

	/* advance past path name to target file name */
	pFileName = getFileName(pProcessImageName->Buffer);

	/* if process not already in list */
	if (pPermissionsEntry == NULL) {
		/* create new list elment for this process */
		pPermissionsEntry = addProcess(pProcessImageName);
	}
	if (pPermissionsEntry == NULL) {
		return NULL; /* no room for new process in list */
	}
	if (extension == NULL) {
		return pPermissionsEntry; /* no target extension */
	}
	if (checkPermission(pPermissionsEntry, extension)) {
		return pPermissionsEntry; /* extension already in ext list */
	}

	/* Add extension to process extension list */
	(void)addPermission(pPermissionsEntry, extension);
	
	return pPermissionsEntry;
}

/***************************************************************************
* Name: checkProcessPermissions
*
* Routine Description: This function checks to see if a process is allowed 
*  access to the target file type
*
* Returns:  returns TRUE if the operation is permitted or FALSE if prohibited
*
**************************************************************************/
BOOL checkProcessPermissions(
	PUNICODE_STRING  pProcessImageName,
	USHORT Length,
	MAVEN_EXTENSION extension
)
{
	MAVEN_PROCESS_PERMISSIONS *pPermissionsEntry;

	/* find app in process list */
	pPermissionsEntry = findProcess(pProcessImageName);

	if (pPermissionsEntry == NULL) {
		return FALSE;  /* App not found */
	}


	if ((Length != 0) && (!checkPermission(pPermissionsEntry, extension))) {
		pPermissionsEntry->fail++;
		return FALSE;
	}
	
	return TRUE;
}

/***************************************************************************
* Name: blacklistInit
*
* Routine Description: This function loads the blacklist with the configuration
*	file and folder path.  It should be called once after the first record
*	in the permissionsList is loaded.
*
* Returns:  returns STATUS_SUCCESS if the blacklist is initialized properly.
*
**************************************************************************/
NTSTATUS blacklistInit(void) 
{
	MAVEN_PROCESS_PERMISSIONS *pPermissionsList;
	GDATA *pGdata = &gData;
	BOOL digitFound = FALSE;
	int i;

	pPermissionsList = &pGdata->permissionsList[0]; /* use first record */

	/* search for the drive number
	* NOTE: assumes drive number is one digit
	*/
	for (i = 0; pPermissionsList->pProcessImageName->Length / sizeof(WCHAR); i++) {
		if ((pPermissionsList->pProcessImageName->Buffer[i] >= '0') &&
			(pPermissionsList->pProcessImageName->Buffer[i]) <= '9') {
			digitFound = TRUE;
			break;
		}
	}

	if (digitFound) {
		int stringSize = (i + 2) * sizeof(WCHAR) +
			sizeof(PERMISSIONS_CONFIG_FILE_FOLDER_NAME L"\\" PERMISSIONS_CONFIG_FILE);
		PWCHAR pBlacklist = ExAllocatePoolWithTag(NonPagedPool,
			stringSize,
			'nvaM');
		if (pBlacklist == NULL) {
			return STATUS_NO_MEMORY; /* Error, malloc fail */
		}
		/* copy the path name up-to the digit to the blacklist */
		memcpy(pBlacklist,
			pPermissionsList->pProcessImageName->Buffer,
			sizeof(WCHAR) * (i + 1));
		/* copy the remaining file name */
		memcpy(&pBlacklist[i + 1],
			PERMISSIONS_CONFIG_FILE_FOLDER_NAME L"\\" PERMISSIONS_CONFIG_FILE,
			sizeof(PERMISSIONS_CONFIG_FILE_FOLDER_NAME L"\\" PERMISSIONS_CONFIG_FILE));
		pBlacklist[stringSize / 2 - 1] = 0; /* terminate */
											/* add the config file to the blacklist */
		pGdata->blacklist[pGdata->blacklistEnd] = pBlacklist;

		/* now blacklist the entire folder */
		stringSize -= sizeof(L"\\" PERMISSIONS_CONFIG_FILE);
		pBlacklist = ExAllocatePoolWithTag(NonPagedPool,
			stringSize,
			'nvaM');
		if (pBlacklist == NULL) {
			return STATUS_NO_MEMORY; /* Error, malloc fail */
		}
		/* copy the path name up-to the config file name */
		memcpy(pBlacklist,
			pGdata->blacklist[pGdata->blacklistEnd++], /* next element */
			stringSize);
		pBlacklist[stringSize / 2 - 1] = 0; /* terminate */
		pGdata->blacklist[pGdata->blacklistEnd++] = pBlacklist;
	}
	return STATUS_SUCCESS;
}

/***************************************************************************
* Name: configFileLoad
*
* Routine Description: This function opens a permissions configuration file
*	and reads it into memory.  The file is expected in a known location at
*   startup.  It must be previously placed there by an admin.
*
* Returns:  returns STATUS_SUCCESS if file read was successful.
*
**************************************************************************/
NTSTATUS configFileLoad
(
	BOOL			*pKeyFound,	/* key found in first line */
	PUNICODE_STRING pFilename	/* filename to read */
)
{
	HANDLE fileHandle;
	IO_STATUS_BLOCK   ioStatusBlock;
	OBJECT_ATTRIBUTES objectAttributes;
	FILE_STANDARD_INFORMATION fileInfo;

	GDATA *pGdata = &gData;
	PUNICODE_STRING pProcessImageName = NULL;
	MAVEN_PROCESS_PERMISSIONS *pPermissionsList;
	CHAR instring[PAYLOAD_MSG_SZ], *pInstring;
	NTSTATUS ntStatus;
	INT64 fileSize;
	int numScanned;
	UINT32 index, nextIndex, i, j;
	USHORT k;
	BOOL done = FALSE;
	LARGE_INTEGER readPos;
	readPos.QuadPart = 0;
	ULONG bytesRemaining;

	*pKeyFound = FALSE;	/* initialize return value */

	if (pGdata->permissionsList[0].pProcessImageName != NULL) {
		return STATUS_FLT_INTERNAL_ERROR; /* Error, bad index */
	}

	/* open config file for reading */
	InitializeObjectAttributes(&objectAttributes,
		pFilename,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);

	/* This opens a file if it already exists */
	ntStatus = NtCreateFile(
		&fileHandle,
		GENERIC_READ | SYNCHRONIZE,//DesiredAccess
		&objectAttributes,
		&ioStatusBlock,
		NULL,					//AllocationSize
		FILE_ATTRIBUTE_NORMAL,  //FileAttributes
		0,						//ShareAccess
		FILE_OPEN,				//CreateDisposition
		FILE_SYNCHRONOUS_IO_NONALERT,//CreateOptions
		NULL,					//EaBuffer
		0						//EaLength
	); 
		
	if (ntStatus != STATUS_SUCCESS){
		return ntStatus; /* Error, file not found */
	}

	/* get the file size */
	ntStatus = NtQueryInformationFile(
		fileHandle,
		&ioStatusBlock,
		&fileInfo,
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation
	);
	if (ntStatus != STATUS_SUCCESS) {
		done = TRUE;
		(void)NtClose(fileHandle);

		return ntStatus; /* Error, for now */
	}

	fileSize = fileInfo.EndOfFile.QuadPart;
	if (fileSize < 0) {
		(void)NtClose(fileHandle);
		return STATUS_FLT_INTERNAL_ERROR; /* Error, bad file size */
	}

	bytesRemaining = (ULONG)fileSize;

	index = nextIndex = 0;
	while (!done) {
		ULONG length;
		ULONG bytesProcessed = 0;
		length = (PAYLOAD_MSG_SZ < bytesRemaining) ? PAYLOAD_MSG_SZ : bytesRemaining;

		pInstring = &instring[0]; /* initialize buffer pointer to start of temp space */

		ntStatus = NtReadFile(
			fileHandle,
			NULL,//   Event,
			NULL,// PIO_APC_ROUTINE  ApcRoutine
			NULL,// PVOID  ApcContext
			&ioStatusBlock,
			pInstring,
			length,
			&readPos, // ByteOffset
			NULL);

		if (ntStatus != STATUS_SUCCESS) {
			done = TRUE;
			ntStatus = NtClose(fileHandle);
			return ntStatus; /* Error, for now */
		}

		/* if first record, look for special key in first column */
		if (index == 0) {
			if (pInstring[0] == LEARN_KEY) {
				*pKeyFound = TRUE;
				pInstring++;
				bytesProcessed = 1;
			}
		}

		/* if secondf record, use physical drive name to form blacklist
		* if the unqualified path to the configuration file is not yet initialized 
		* e.g. \Device\HarddiskVolume2\DriverTest\maven.csv
		*/
		if ((index == 1) && (pGdata->blacklistEnd == 0)) {
			ntStatus = blacklistInit();
			if (!NT_SUCCESS(ntStatus)) {
				(void)NtClose(fileHandle);
				return ntStatus;
			}
		}

		csvToText(pInstring); /* convert comma to tab */

		/* get record index */
		numScanned = scanForDecimal(pInstring, &index, '\t');
		if ((numScanned == 0) || (index >= MAVEN_NUM_PROC) || (index != nextIndex)) {
			(void)NtClose(fileHandle);
			return STATUS_FLT_INTERNAL_ERROR; /* Error, bad index */
		}
		nextIndex++;

		pInstring += numScanned; /* advance past index */
		bytesProcessed += numScanned;

		pPermissionsList = &pGdata->permissionsList[index];
		if (pPermissionsList->pProcessImageName != NULL) {
			(void)NtClose(fileHandle);
			return STATUS_FLT_INTERNAL_ERROR; /* Error, config not empty */
			break;
		}

		/* find length of path string */
		for (k = 0; (k < MAX_PATH) && (pInstring[k] != 0) &&
			(pInstring[k] != L'\t') && (pInstring[k] != L'\n'); k++);

		if ((k == 0) || (k == MAX_PATH)) {
			return STATUS_FLT_INTERNAL_ERROR; /* bad path length */
		}

		/* leave room for terminator */
		k++;

		/* allocate buffer, this includes UNICODE header + buffer space */
		pProcessImageName = ExAllocatePoolWithTag(
			NonPagedPool,
			sizeof(UNICODE_STRING) + k * 2,
			'nvaM');
		if (pProcessImageName == NULL) {
			(void)NtClose(fileHandle);
			return STATUS_NO_MEMORY; /* Error, malloc fail */
		}

		pProcessImageName->Length = k;
		pProcessImageName->MaximumLength = k;
		/* buffer space begins right after UNICODE header */
		pProcessImageName->Buffer = (PWCH)&pProcessImageName[1];

		/* copy image name to new buffer, convertion to WCHAR on the way. */
		charToWide(pProcessImageName->Buffer, pInstring);
		pProcessImageName->Buffer[k - 1] = 0; /* terminate */


		/* load process image name */
		/* UNICODE buffer filled in, put in config permissions list */
		pPermissionsList->pProcessImageName = pProcessImageName;

		/* advance past path name */
		pInstring += k;
		bytesProcessed += k;

		/* load extensions list */
		/* skip over count and fail in config record */
		k = findChar(pInstring, '\t');
		bytesProcessed += k;
		pInstring += k;
		k = findChar(pInstring, '\t');
		bytesProcessed += k;
		pInstring += k;

		/* now pointing to extensions */

		/* copy in each remaining file extension */
		for (j = 0; j < MAVEN_MAX_EXT_ENTRIES; j++) {
			/* copy extension */
			for (i = 0; i < (MAVEN_MAX_EXT_LENGTH); i++) {
				if ((*pInstring == 0) || (*pInstring == '\t') ||
					(*pInstring == '\n') || (*pInstring == '\r')) {
					pPermissionsList->extensions[j][i] = 0;
					break;  /* end of string */
				}
				pPermissionsList->extensions[j][i] =
					*pInstring++;
				bytesProcessed++;

			} /* end copy extension */
			/* force string termination even if too long */
			pPermissionsList->extensions[j][MAVEN_MAX_EXT_LENGTH - 1] = 0;

			if ((*pInstring == 0) || (*pInstring == '\n')) { /* end of line */
				pInstring++; /* advance to next record */
				bytesProcessed++;
				break;
			}

			/* advance to next extension field */
			pInstring++;
			bytesProcessed++;

		} /* end for j */

		/* advance to next file record */
		while ((*pInstring == '\n') || (*pInstring == '\r') || (*pInstring == 0)) {
			pInstring++;
			bytesProcessed++;
		}

		bytesRemaining -= bytesProcessed;
		readPos.QuadPart += bytesProcessed;
		/* advance to next field */

		done = !(fileSize > readPos.QuadPart);
	} /* end while */

	ntStatus = NtClose(fileHandle);

	return ntStatus;
}

/*************************************************************************
Wide Character string routines.

For some bizarre reason, I get link errors with some wide versions
of string library routines.  I wrote my own routines for string manipulation.
*************************************************************************/

/***************************************************************************
* Name: charToWide
*
* Routine Description: This function converts a wide string to a char string.
*	It terminates when it finds a newline or tab character.
*
* Returns:  N/A
*
**************************************************************************/
void charToWide(WCHAR* dest, const CHAR* source)
{
	int i = 0;

	do {
		dest[i] = (WCHAR)source[i];
	} while ((source[i] != '\0') && (source[i++] != '\t'));
}

/***************************************************************************
* Name: csvToText
*
* Routine Description: This function replaces commas with tabs.  This makes
*	it easier to parse the string using functions that terminate with 
*	whitespace.
*
* Returns:  N/A
*
**************************************************************************/
void csvToText(CHAR* buf)
{
	int i = 0;

	while (buf[i] != 0) {
		if ((buf[i] == 0) || (buf[i] == '\n')) {
			break;
		}		if (buf[i] == ',') {
			buf[i] = '\t';
		}
		i++;
	}
}

/***************************************************************************
* Name: findChar
*
* Routine Description: This function searches a string pointer
* to find the next occurance of the passed field character + 1.
*
* Returns:  returns number of characters advanced
*
**************************************************************************/
USHORT findChar(
	CHAR *pString,
	CHAR inchar
)
{
	USHORT returnVal = 0;

	/* search for requested character or end of string */
	while ((*pString != 0) && (*pString != '\n') && 
		(*pString != '\r') && (*pString != inchar)) {
		pString++;
		returnVal++;
	}

	if (*pString != 0) {
		pString++;
		returnVal++;
	}

	return returnVal;
}

/***************************************************************************
* Name: nextField
*
* Routine Description: This function advances a string pointer
* to the next occurance of the passed delimeter character + 1.  If the
* delimeter is not found, this pointer will point to the end of string.
*
* Returns:  returns pointer to next occurance of delimeter or null if not found
*
**************************************************************************/
CHAR *nextField(
	CHAR *pString,
	CHAR delimiter
)
{
	/* search for delimeter */
	while ((*pString != 0) && (*pString != delimiter)) {
		pString++;
	}

	/* If not at end of string */
	if (*pString != 0) {
		pString++;	/* advance past delimeter */
	}
	return pString;

}

/***************************************************************************
* Name: nextWField
*
* Routine Description: This function advances a string pointer
* to the next occurance of the passed delimeter character + 1 in the passed
* wide string.  If the delimeter is not found, this pointer will point to 
* the end of string.
*
* Returns:  returns pointer to next occurance of delimeter + 1 or null if 
* not found
*
**************************************************************************/
WCHAR *nextWField(
	WCHAR *pString,
	WCHAR delimiter
)
{
	/* search for delimeter */
	while ((*pString != 0) && (*pString != delimiter)) {
		pString++;
	}

	/* If not at end of string */
	if (*pString != 0) {
		pString++;	/* advance past delimeter */
	}
	return pString;

}

/***************************************************************************
* Name: scanForDecimal
*
* Routine Description: This function substitutes for swscanf which doesn't
* link, sounds like a bug to me. It's used for a special case here to
* extract decimal numbers from the received csv configuration file.
*
* Works only for unsigned 32-bit numbers
*
* Returns:  returns number of characters parsed or zero if error
*
**************************************************************************/
int scanForDecimal(
	CHAR *pString, /* input string */
	UINT32 *pValue, /* output value */
	CHAR delimiter
)
{
	int numChars = 0;	/* number of characters advanced in string or zero if error */

	*pValue = 0;

	/* search for delimeter or digit */
	while ((*pString != 0) && (*pString != delimiter)) {
		if (isdigit(*pString)) {
			*pValue = *pValue * 10 + (*pString - L'0');
		}
		else {
			return 0;
		}
		pString++;	/* next charater */
		numChars++;
	}
	if (*pString != 0) {
		numChars++;	/* advance past delimeter */
	}
	return numChars;

}

/***************************************************************************
* Name: scanWForDecimal
*
* Routine Description: This function substitutes for swscanf which doesn't
* link, sounds like a bug to me. It's used for a special case here to
* extract decimal numbers from the received csv configuration file.
*
* Works only for unsigned 32-bit numbers
*
* Returns:  returns number of characters parsed or zero if error
*
**************************************************************************/
int scanWForDecimal(
	WCHAR *pString, /* input string */
	UINT32 *pValue, /* output value converted from string to decimal */
	WCHAR delimiter
)
{
	int numChars = 0;	/* number of characters advanced in string or zero if error */

	*pValue = 0;

	/* search for delimeter or digit */
	while ((*pString != 0) && (*pString != delimiter)) {
		if (isdigit(*pString)) {
			*pValue = *pValue * 10 + (*pString - L'0');
		}
		else {
			return 0;
		}
		pString++;	/* next charater */
		numChars++;
	}
	if (*pString != 0) {
		numChars++;	/* advance past delimeter */
	}
	return numChars;

}

/*************************************************************************
MiniFilter callback routines.
*************************************************************************/


FLT_PREOP_CALLBACK_STATUS
mavenMinifilterRestrictSync(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
/*++

Routine Description:

This routine determines if file access for this service is allowed.

This is non-pageable because it could be called on the paging path

Arguments:

Data - Pointer to the filter callbackData that is passed to us.

FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance, its associated volume and
file object.

CompletionContext - The context for the completion routine for this
operation.

Return Value:

The return value is the status of the operation.

--*/
{
	NTSTATUS status;
	PFLT_FILE_NAME_INFORMATION nameInfo;
	GDATA *pGdata = &gData;
	PUNICODE_STRING  pProcessImageName = NULL;	/* Executing file name including path */
	FLT_PREOP_CALLBACK_STATUS returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
	//PWCH  pProcessName;					/*  Executing file name no path */

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	status = FltGetFileNameInformation(
		Data,
		FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
		&nameInfo
	);

	if ((!NT_SUCCESS(status)) || (nameInfo == NULL)) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK; /* can't process */
	}

	FltParseFileNameInformation(nameInfo);


	switch (pGdata->operatingMode) {
	case MAVEN_IDLE:
		break;

	case MAVEN_LEARN:
		if ((Data->Iopb->Parameters.AcquireForSectionSynchronization.PageProtection & PAGE_EXECUTE) &&
		   (Data->Iopb->Parameters.AcquireForSectionSynchronization.SyncType == SyncTypeCreateSection) &&
		   (nameInfo != NULL) && (nameInfo->Name.Length != 0)) {
			size_t sizeBytes, sizeChars;

			sizeBytes = nameInfo->Name.Length; /* not including null terminator */
			sizeChars = sizeBytes / 2;

			// todo - this allocation could be optimized to put this allocation later 
			pProcessImageName = ExAllocatePoolWithTag(NonPagedPool,
				sizeBytes + sizeof(UNICODE_STRING) + 2,	'nvaM');

			if (pProcessImageName == NULL)
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			pProcessImageName->Length = nameInfo->Name.Length;
			pProcessImageName->MaximumLength = nameInfo->Name.MaximumLength;
			pProcessImageName->Buffer = (PWCH)&pProcessImageName[1];
			wcsncpy(pProcessImageName->Buffer, nameInfo->Name.Buffer, 
				sizeChars + 1);
			/* null terminate */
			pProcessImageName->Buffer[sizeChars] = 0;

			if (updateProcessList(pProcessImageName, NULL) != NULL) {
				/* allocated buffer was saved in process permissions structure.  NULL here
				* so it doesn't get deleted on exit
				*/
				pProcessImageName = NULL;
			}
		}
		break;
	case MAVEN_BOOT:
	case MAVEN_FILTER:
		
		if ((Data->Iopb->Parameters.AcquireForSectionSynchronization.PageProtection & PAGE_EXECUTE) &&
		  (Data->Iopb->Parameters.AcquireForSectionSynchronization.SyncType == SyncTypeCreateSection)) {

			if (!checkProcessPermissions(&nameInfo->Name,
				0,
				NULL)) {
				logStatusMessage(nameInfo->Name.Buffer, NULL);
				if (pGdata->operatingMode != MAVEN_BOOT) 
				{
					/* Block this process */
					Data->IoStatus.Status = STATUS_ACCESS_DENIED;
					Data->IoStatus.Information = 0;
					/* no more filters should execute */
					returnStatus = FLT_PREOP_COMPLETE;
				}

			}
			/* if in boot mode and windows has intialized */
			else if ((pGdata->operatingMode == MAVEN_BOOT) && 
				(nameInfo->FinalComponent.Buffer != NULL) &&
				(wcscmp(nameInfo->FinalComponent.Buffer,
				BOOT_SEQUENCE_END_NAME) == 0)) 
			{
				pGdata->operatingMode = MAVEN_FILTER; /* transition to filter mode */
			}
		}
		
		break;

	default:
		break;

	}

	/* cleanup */
	if (pProcessImageName != NULL)
	{
		DbgPrint("**************** ProcessName = %ws\n", pProcessImageName);
		ExFreePoolWithTag(pProcessImageName, 'nvaM');
	}
	
	/* This passes the request down to the next miniFilter in the chain. */
	return returnStatus;
}

/*
* mavenBlacklisted
*
* This function checks the passed callback data to see if the passed file
* or directory is blacklisted.
*
*/
BOOL mavenBlacklisted(
	_Inout_ PFLT_CALLBACK_DATA Data
)
{
	GDATA *pGdata = &gData;
	NTSTATUS status;
	PFLT_FILE_NAME_INFORMATION nameInfo;
	UINT index;
	PWCHAR pBlacklistEntry;

	status = FltGetFileNameInformation(
		Data,
		FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
		&nameInfo);
	if (NT_SUCCESS(status) && (nameInfo->Name.Buffer != NULL)) {
		FltParseFileNameInformation(nameInfo);

		/* search through blacklist */
		for (index = 0; index < pGdata->blacklistEnd; index++) {
			pBlacklistEntry = pGdata->blacklist[index];
			/* if name found in blacklist */
			if (compareStringWildcard(pBlacklistEntry, nameInfo->Name.Buffer)) {
				return TRUE; /* entry was blacklisted */
			}
		}
	}
	return FALSE;
}

FLT_PREOP_CALLBACK_STATUS
mavenMinifilterRestrictCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
/*++

Routine Description:

This routine determines if file access for this service is allowed.

This is non-pageable because it could be called on the paging path

Arguments:

Data - Pointer to the filter callbackData that is passed to us.

FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance, its associated volume and
file object.

CompletionContext - The context for the completion routine for this
operation.

Return Value:

The return value is the status of the operation.

--*/
{
	NTSTATUS status;
	PFLT_FILE_NAME_INFORMATION nameInfo;
	PEPROCESS process;
	GDATA *pGdata = &gData;
	PUNICODE_STRING  pProcessImageName = NULL;	/* Executing file name including path */
	FLT_PREOP_CALLBACK_STATUS returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
	//PWCH  pProcessName;					/*  Executing file name no path */

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

#if 0 // test code for debugging purposes. (leave in for now)
	{
		status = FltGetFileNameInformation(
			Data,
			FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
			&nameInfo);
		if (NT_SUCCESS(status) && (nameInfo->Name.Buffer != NULL)) {
			FltParseFileNameInformation(nameInfo);
			if (wcscmp(L"\\Device\\HarddiskVolume3\\Windows\\System32\\notepad.exe", 
				nameInfo->Name.Buffer) == 0) {
				DbgPrint("notepad\n");
			}
		}
	}
#endif
	// Get the requesting process
	process = IoThreadToProcess(Data->Thread);

	status = FltGetFileNameInformation(
		Data,
		FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
		&nameInfo
	);

	if ((!NT_SUCCESS(status)) || (nameInfo == NULL)) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK; /* can't process */
	}

	FltParseFileNameInformation(nameInfo);


	switch (pGdata->operatingMode) {
	case MAVEN_IDLE:
		break;

	case MAVEN_LEARN:
		
		/* get process image name.  Buffer allocated internally */
		pProcessImageName = getProcessImageName(process);
		if (pProcessImageName == NULL)
			break; // can't process
		if (pProcessImageName->Buffer == NULL)
			break; // can't process
		if (updateProcessList(pProcessImageName, nameInfo->Extension.Buffer) != NULL) {
			/* allocated buffer was saved in process permissions structure.  NULL here
			* so it doesn't get deleted on exit
			*/
			pProcessImageName = NULL;
		}
		
		break;

	case MAVEN_BOOT:
	case MAVEN_FILTER:
		/* get process image name.  Buffer allocated internally */
		pProcessImageName = getProcessImageName(process);

		/* check to see if access to this file or directory is prohibited */
		if (mavenBlacklisted(Data)) {
			/* Block this process */
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			Data->IoStatus.Information = 0;
			/* no more filters should execute */
			returnStatus = FLT_PREOP_COMPLETE;
			break;
		}

		if (pProcessImageName == NULL)
			break; // can't process
		if (pProcessImageName->Buffer == NULL)
			break; // can't process

		else if (!checkProcessPermissions(pProcessImageName,
			nameInfo->Extension.Length,
			nameInfo->Extension.Buffer)) {
			logStatusMessage(pProcessImageName->Buffer, nameInfo->Name.Buffer);
			if (pGdata->operatingMode != MAVEN_BOOT) {
				/* Block this process */
				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				Data->IoStatus.Information = 0;
				/* no more filters should execute */
				returnStatus = FLT_PREOP_COMPLETE;
			}			
			/* if in boot mode and windows has intialized */
			else if (wcscmp(nameInfo->FinalComponent.Buffer,
				BOOT_SEQUENCE_END_NAME) == 0) {
				pGdata->operatingMode = MAVEN_FILTER; /* transition to filter mode */
			}
		}
		break;

	default:
		break;

	}

	/* cleanup */
	if (pProcessImageName != NULL)
	{
		DbgPrint("**************** ProcessName = %ws\n", pProcessImageName);
		ExFreePoolWithTag(pProcessImageName, 'nvaM');
	}
	
	/* This passes the request down to the next miniFilter in the chain. */
	return returnStatus;
}

FLT_PREOP_CALLBACK_STATUS mavenMinifilterPreOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
/*++

Routine Description:

This routine is a pre-operation dispatch routine for this miniFilter.

This is non-pageable because it could be called on the paging path

Arguments:

Data - Pointer to the filter callbackData that is passed to us.

FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance, its associated volume and
file object.

CompletionContext - The context for the completion routine for this
operation.

Return Value:

The return value is the status of the operation.

--*/
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("mavenMinifilter!mavenMinifilterPreOperation: Entered\n"));

	//
	//  See if this is an operation we would like the operation status
	//  for.  If so request it.
	//
	//  NOTE: most filters do NOT need to do this.  You only need to make
	//        this call if, for example, you need to know if the oplock was
	//        actually granted.
	//

	if (mavenMinifilterDoRequestOperationStatus(Data)) {

		status = FltRequestOperationStatusCallback(Data,
			mavenMinifilterOperationStatusCallback,
			(PVOID)(++OperationStatusCtx));
		if (!NT_SUCCESS(status)) {

			PT_DBG_PRINT(PTDBG_TRACE_OPERATION_STATUS,
				("mavenMinifilter!mavenMinifilterPreOperation: FltRequestOperationStatusCallback Failed, status=%08x\n",
					status));
		}
	}

	// This template code does not do anything with the callbackData, but
	// rather returns FLT_PREOP_SUCCESS_WITH_CALLBACK.
	// This passes the request down to the next miniFilter in the chain.
	
	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

 VOID mavenMinifilterOperationStatusCallback(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
	_In_ NTSTATUS OperationStatus,
	_In_ PVOID RequesterContext
)
/*++

Routine Description:

This routine is called when the given operation returns from the call
to IoCallDriver.  This is useful for operations where STATUS_PENDING
means the operation was successfully queued.  This is useful for OpLocks
and directory change notification operations.

This callback is called in the context of the originating thread and will
never be called at DPC level.  The file object has been correctly
referenced so that you can access it.  It will be automatically
dereferenced upon return.

This is non-pageable because it could be called on the paging path

Arguments:

FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance, its associated volume and
file object.

RequesterContext - The context for the completion routine for this
operation.

OperationStatus -

Return Value:

The return value is the status of the operation.

--*/
{
	UNREFERENCED_PARAMETER(FltObjects);

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("mavenMinifilter!mavenMinifilterOperationStatusCallback: Entered\n"));

	PT_DBG_PRINT(PTDBG_TRACE_OPERATION_STATUS,
		("mavenMinifilter!mavenMinifilterOperationStatusCallback: Status=%08x ctx=%p IrpMj=%02x.%02x \"%s\"\n",
			OperationStatus,
			RequesterContext,
			ParameterSnapshot->MajorFunction,
			ParameterSnapshot->MinorFunction,
			FltGetIrpName(ParameterSnapshot->MajorFunction)));
}


FLT_POSTOP_CALLBACK_STATUS
mavenMinifilterPostOperation(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
/*++

Routine Description:

This routine is the post-operation completion routine for this
miniFilter.

This is non-pageable because it may be called at DPC level.

Arguments:

Data - Pointer to the filter callbackData that is passed to us.

FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
opaque handles to this filter, instance, its associated volume and
file object.

CompletionContext - The completion context set in the pre-operation routine.

Flags - Denotes whether the completion is successful or is being drained.

Return Value:

The return value is the status of the operation.

--*/
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
		("mavenMinifilter!mavenMinifilterPostOperation: Entered\n"));

	return FLT_POSTOP_FINISHED_PROCESSING;
}

BOOLEAN mavenMinifilterDoRequestOperationStatus(
	_In_ PFLT_CALLBACK_DATA Data
)
/*++

Routine Description:

This identifies those operations we want the operation status for.  These
are typically operations that return STATUS_PENDING as a normal completion
status.

Arguments:

Return Value:

TRUE - If we want the operation status
FALSE - If we don't

--*/
{
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;

	//
	//  return boolean state based on which operations we are interested in
	//

	return (BOOLEAN)

		//
		//  Check for oplock operations
		//

		(((iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
		((iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_FILTER_OPLOCK) ||
			(iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_BATCH_OPLOCK) ||
			(iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_1) ||
			(iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2)))

			||

			//
			//    Check for directy change notification
			//

			((iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL) &&
			(iopb->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY))
			);
}

/***************************************************************************
* Name: MavenPortConnect
*
* Routine Description: This function is called when user-mode connects to 
* the server port to establish a connection.
*
* Returns:  STATUS_CONNECT (always)
*
**************************************************************************/
NTSTATUS MavenPortConnect(
	__in PFLT_PORT ClientPort,
	__in_opt PVOID ServerPortCookie,
	__in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
	__in ULONG SizeOfContext,
	__deref_out_opt PVOID *ConnectionCookie
)
{
	PAGED_CODE();
	GDATA *pGdata = &gData;

	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionCookie);

	ASSERT(pGdata->mavenClientPort == NULL);
	ASSERT(pGdata->mavenUserProcess == NULL);

	/* Set the user process and port. */
	pGdata->mavenUserProcess = PsGetCurrentProcess();
	pGdata->mavenClientPort = ClientPort;

	return STATUS_SUCCESS;
}

/***************************************************************************
* Name: MavenPortDisconnect
*
* Routine Description: This function is called when user-mode disconnects to
* the server port.
*
* Returns:  N/A
*
**************************************************************************/
VOID MavenPortDisconnect(
	__in_opt PVOID ConnectionCookie
)
{
	UNREFERENCED_PARAMETER(ConnectionCookie);

	PAGED_CODE();

	GDATA *pGdata = &gData;
	/*
	* Close our handle to the connection: note, since we limited max connections to 1,
	* another connect will not be allowed until we return from the disconnect routine.
	* Sets ClientPort to NULL.
	*/
	FltCloseClientPort(gFilterHandle, &pGdata->mavenClientPort);
	ASSERT(pGdata->mavenClientPort == NULL);

	/* Reset the user-process field. */
	pGdata->mavenUserProcess = NULL;
}

/*
* clearConfigData
*
* This function initializes the process permissions list.
*
*/
void clearConfigData(void) {
	int i;
	MAVEN_PROCESS_PERMISSIONS *pProcessPermissions;
	GDATA *pGdata = &gData;

	for (i = 0; i < MAVEN_NUM_PROC; i++) {
		pProcessPermissions = &pGdata->permissionsList[i];
		/* if end of used list */
		if (pProcessPermissions->pProcessImageName == NULL) {
			break; /* done */
		}
		/* free any path record */
		ExFreePoolWithTag(pProcessPermissions->pProcessImageName, 'nvaM');
		memset(pProcessPermissions, 0, sizeof(MAVEN_PROCESS_PERMISSIONS));
	}
	
}


/*
* MavenMessage
*
* This is called whenever a message is received.
*
* Returns STATUS_SUCCESS
*/
NTSTATUS
MavenMessage(
	__in PVOID ConnectionCookie,
	__in_bcount_opt(InputBufferSize) PVOID InputBuffer,
	__in ULONG InputBufferSize,
	__out_bcount_part_opt(OutputBufferSize, *pReturnOutputBufferLength) PVOID OutputBuffer,
	__in ULONG OutputBufferSize,
	__out PULONG pReturnOutputBufferLength
)
{
	PAGED_CODE();

	GDATA *pGdata = &gData;
	MAVEN_PROCESS_PERMISSIONS *pPermissionsList;
	MAVEN_CMD_MESSAGE * pCommandMsg;
	MAVEN_RESPONSE_MESSAGE * pResponseMsg;
	MAVEN_COMMAND command;
	NTSTATUS status = STATUS_SUCCESS;
	int numScanned;
	UINT32 index;
	WCHAR *pPayload;
	int i, j;
	USHORT k;


	UNREFERENCED_PARAMETER(ConnectionCookie);

	/*
	*                     **** PLEASE READ ****
	*
	*  The INPUT and OUTPUT buffers are raw user mode addresses.  The filter
	*  manager has already done a ProbedForRead (on InputBuffer) and
	*  ProbedForWrite (on OutputBuffer) which guarentees they are valid
	*  addresses based on the access (user mode vs. kernel mode).  The
	*  minifilter does not need to do their own probe.
	*
	*  The filter manager is NOT doing any alignment checking on the pointers.
	*  The minifilter must do this themselves if they care (see below).
	*
	*  The minifilter MUST continue to use a try/except around any access to
	*  these buffers.
	*/

	*pReturnOutputBufferLength = 0; /* default value */

	if ((InputBuffer == NULL) ||
		(InputBufferSize < sizeof(MAVEN_CMD_MESSAGE))) {
		return STATUS_INVALID_PARAMETER;
	}
	try {

		/*
		* Probe and capture input message: the message is raw user mode
		* buffer, so need to protect with exception handler
		*/

		pCommandMsg = (MAVEN_CMD_MESSAGE *)InputBuffer;
		command = pCommandMsg->msgType;

	} except(EXCEPTION_EXECUTE_HANDLER) {

		return GetExceptionCode();
	}

	pResponseMsg = ((MAVEN_RESPONSE_MESSAGE *)OutputBuffer);

	if (OutputBufferSize < sizeof(MAVEN_RESPONSE_MESSAGE)) {
		return STATUS_BUFFER_TOO_SMALL;
	}

	/*
	* Validate Buffer alignment.  If a minifilter cares about
	* the alignment value of the buffer pointer they must do
	* this check themselves.  Note that a try/except will not
	* capture alignment faults.
	*/

	if (!IS_ALIGNED(OutputBuffer, sizeof(ULONG))) {
		return STATUS_DATATYPE_MISALIGNMENT;
	}

	/*
	* Protect access to raw user-mode output buffer with an
	* exception handler.  Probe the output buffer putting
	* a default value of error in the message type.
	*/
	try {
		pResponseMsg->msgType = mavenError;
	} except(EXCEPTION_EXECUTE_HANDLER) {
		return GetExceptionCode();
	}
	pPayload = pResponseMsg->payload;

	if (pPayload == NULL) {
		return STATUS_INVALID_PARAMETER;
	}

	try {
		memset(pPayload, 0, sizeof(PAYLOAD_MSG_SZ));
	} except(EXCEPTION_EXECUTE_HANDLER) {
		return GetExceptionCode();
	}	

	switch (command) {

	case mavenModeSetIdle:	/* set driver mode to idle */
		pGdata->operatingMode = MAVEN_IDLE;
		pResponseMsg->msgType = mavenSuccess;
		break;

	case mavenModeSetLearn:	/* monitor file accesses and expand configuration accordingly */
		pGdata->operatingMode = MAVEN_LEARN;
		pResponseMsg->msgType = mavenSuccess;
		break;

	case mavenModeSetFilter:	/* use configuration to filter file access */
		if (pGdata->permissionsList[0].pProcessImageName == NULL) {
			/* can's start filtering if there's no database */
			pResponseMsg->msgType = mavenError;
			break;
		}
		pGdata->operatingMode = MAVEN_FILTER;
		pResponseMsg->msgType = mavenSuccess;
		break;

	case mavenModeSetReset:	/* erase configuration parameters, then idle */
		/* must be idle */
		if (pGdata->operatingMode != MAVEN_IDLE) {
			pResponseMsg->msgType = mavenError;
		}
		else
		{
			clearConfigData();
			pResponseMsg->msgType = mavenSuccess;
		}
		break;

	case mavenGetVersion:
		pResponseMsg->msgType = mavenSuccess;
		swprintf(pResponseMsg->payload, L"%ls\n", mavenDriverVersion);
		break;

	case mavenGetStatus:
		/* get log size */
		i = logGetSize();

		pResponseMsg->msgType = mavenSuccess;
		/* format status message */
		swprintf(pResponseMsg->payload, L"Mode = %ls, Log Size = %d\n", 
			modeString[pGdata->operatingMode], i
			);

		break;

	case mavenGetLog:
		
		if (pGdata->mavenMessageLog.mavenLog[pGdata->mavenMessageLog.mavenLogFirstIndex].msgType == mavenSuccess) {
			pResponseMsg->msgType = mavenSuccess;
			swprintf(pResponseMsg->payload, L"%ls", pGdata->mavenMessageLog.mavenLog[pGdata->mavenMessageLog.mavenLogFirstIndex].payload);
			pGdata->mavenMessageLog.mavenLog[pGdata->mavenMessageLog.mavenLogFirstIndex++].msgType = mavenError; /* mark as empty */
			if (pGdata->mavenMessageLog.mavenLogFirstIndex == MAVEN_LOG_SIZE) {
				pGdata->mavenMessageLog.mavenLogFirstIndex = 0; /* wrap */
			}
		}
		break;

	case mavenReadConfig:

		/* must be idle */
		if (pGdata->operatingMode != MAVEN_IDLE) {
			pResponseMsg->msgType = mavenError;
			break;
		}

		numScanned = scanWForDecimal(pCommandMsg->payload, &index, L'\t');
		if ((numScanned == 0) || (index >= MAVEN_NUM_PROC)) {
			pResponseMsg->msgType = mavenError;
			break;
		}

		pPermissionsList = &pGdata->permissionsList[index];

		if (pPermissionsList->pProcessImageName == NULL) { // if requested record empty
			pResponseMsg->msgType = mavenError; /* done */
			break;
		}

		pResponseMsg->msgType = mavenSuccess; /* good record */
		pPayload += swprintf(pPayload, L"%06d\t%ls\t%d\t%d",
			index,
			pPermissionsList->pProcessImageName->Buffer,
			pPermissionsList->count,
			pPermissionsList->fail
		);
		if (index >= MAVEN_NUM_PROC) {
			pResponseMsg->msgType = mavenError; /* error */
			break;
		}
		for (j = 0; j < MAVEN_MAX_EXT_ENTRIES; j++) {
			if (pPermissionsList->extensions[j][0] == 0) {
				break;	/* end of list */
			}
			pPayload += swprintf(pPayload, L"\t%s", pPermissionsList->extensions[j]);
			/* wildcard, don't report anything else */
			if (pPermissionsList->extensions[0][0] == L'*') {
				break;	/* end of list */
			}
		}
		*pReturnOutputBufferLength = sizeof(MAVEN_RESPONSE);

		break;
	case mavenWriteConfig:
	{
		PUNICODE_STRING pProcessImageName = NULL;

		/* must be idle */
		if (pGdata->operatingMode != MAVEN_IDLE) {
			pResponseMsg->msgType = mavenError;
			break;
		}

		/* point to command payload */
		pPayload = pCommandMsg->payload;

		numScanned = scanWForDecimal(pPayload, &index, L'\t');
		if ((numScanned == 0) || (index >= MAVEN_NUM_PROC)) {
			pResponseMsg->msgType = mavenError;
			goto cleanUp;
		}

		pPermissionsList = &pGdata->permissionsList[index];
		if (pPermissionsList->pProcessImageName != NULL) {
			pResponseMsg->msgType = mavenError; /* must clear first */
			break;
		}

		pResponseMsg->msgType = mavenSuccess;

		/* advance past whitespace */
		pPayload += numScanned;

		/* find length of path string */
		for (k = 0; (k < MAX_PATH) && (pPayload[k] != 0) && 
			(pPayload[k] !=L'\t'); k++);

		if ((k == 0) ||(k == MAX_PATH)) {
			pResponseMsg->msgType = mavenError; /* bad path length */
			break;

		}

		/* leave room for terminator */
		k++;

		/* 
		 * construct a UNICODE buffer and copy path name to it
		 */

		/* allocate buffer, this includes UNICODE header + buffer space */
		pProcessImageName = ExAllocatePoolWithTag(
			NonPagedPool, 
			sizeof(UNICODE_STRING) + k * 2, 
			'nvaM');
		
		if (pProcessImageName == NULL)
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto cleanUp;
		}

		pProcessImageName->Length = k;
		pProcessImageName->MaximumLength = k;
		/* buffer space begins right after UNICODE header */
		pProcessImageName->Buffer = (PWCH)&pProcessImageName[1];
		/* copy path to new buffer */
		memcpy(pProcessImageName->Buffer, pPayload, (k - 1) * 2);

		/* null terminate */
		pProcessImageName->Buffer[k - 1] = 0;

		/* UNICODE buffer filled in, put in config permissions list */
		pPermissionsList->pProcessImageName = pProcessImageName;

		/* advance past path name */
		pPayload += k;

		/* skip over count and fail in config record */
		pPayload = nextWField(pPayload, L'\t');
		pPayload = nextWField(pPayload, L'\t');

		/* now pointing to extensions */

		/* copy in each remaining file extension */
		for (j = 0; j < MAVEN_MAX_EXT_ENTRIES; j++) {
			/* copy extension */
			for (i = 0; i < (MAVEN_MAX_EXT_LENGTH); i++) {
				if (*pPayload == L'\n') { /* end of line */
					break;
				}
				if ((*pPayload == 0)||(*pPayload == L'\t')) {
					pPermissionsList->extensions[j][i] = 0;
					break;  /* end of string */
				}
				pPermissionsList->extensions[j][i] =
					*pPayload++;
			}
			/* force string termination even if too long */
			pPermissionsList->extensions[j][MAVEN_MAX_EXT_LENGTH - 1] = 0;

			if (*pPayload == L'\n') { /* end of line */
				break;
			}

			/* advance to next extension field */
			pPayload++;

		} /* end for j */
	} /* end case mavenWriteConfig */
	break;

	default:
		pResponseMsg->msgType = mavenError;
		break;
	}

cleanUp:
	
	return STATUS_SUCCESS;

}
