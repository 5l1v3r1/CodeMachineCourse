/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntifs.h"
#include "fltKernel.h"
#include "stdarg.h"
#include "stdio.h"
#include "ntstrsafe.h"

#define __MODULE__		"HideFile"
#define DPF(x)			DbgPrint x


FLT_POSTOP_CALLBACK_STATUS
PostDirectoryControlCallback (
    PFLT_CALLBACK_DATA 			Data,
    PCFLT_RELATED_OBJECTS 		FltObjects,
    PVOID 						CompletionContext,
    FLT_POST_OPERATION_FLAGS 	Flags );

NTSTATUS 
FilterUnloadCallback(
    FLT_FILTER_UNLOAD_FLAGS Flags);

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject );

BOOLEAN
ProcessFileFullDirectoryInformation(
    PUCHAR Buffer,
    ULONG Length );

CONST FLT_OPERATION_REGISTRATION g_FltOperationRegistration[] =
{
    { IRP_MJ_DIRECTORY_CONTROL, 0, NULL, PostDirectoryControlCallback },
    { IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION g_FltRegistration = {
    sizeof( FLT_REGISTRATION ),     //  Size
    FLT_REGISTRATION_VERSION,       //  Version
    0,                              //  Flags
    NULL,                           //  Context
    g_FltOperationRegistration,     //  Operation callbacks
    FilterUnloadCallback,           //  FilterUnload
    NULL,                           //  InstanceSetup
    NULL,                           //  InstanceQueryTeardown
    NULL,                           //  InstanceTeardownStart
    NULL,                           //  InstanceTeardownComplete
    NULL,                           //  GenerateFileName
    NULL,                           //  GenerateDestinationFileName
    NULL                            //  NormalizeNameComponent
};

PFLT_FILTER     g_FltFilter = NULL;
PWCHAR          g_Pattern = NULL;

NTSTATUS
DriverEntry (
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    DPF(( "*** %s.sys Loaded ***\n", __MODULE__ ));

    DriverObject->DriverUnload = DriverUnload;

    g_Pattern = L"HideThisFile.txt";

    // Register the filter driver using the
    // registration structure g_FltRegistration and store the filter pointer
    // in g_FltFilter
    Status = FltRegisterFilter( 
        DriverObject,
        &g_FltRegistration,
        &g_FltFilter );

    if ( ! NT_SUCCESS ( Status ) ) {
        DPF(( "%s!%s FltRegisterFilter() FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Status ));
        goto Exit;
    }

    // Start the filtering operation 
    Status = FltStartFiltering ( 
        g_FltFilter );

    if ( ! NT_SUCCESS ( Status ) ) {
        DPF(( "%s!%s FltStartFiltering() FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Status ));
        goto Exit;
    }

    return STATUS_SUCCESS;

Exit :
    // error handling
    if ( g_FltFilter ) {
        // Deregister the filter 
        FltUnregisterFilter ( g_FltFilter );
    }
    return Status;
} // DriverEntry()

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject )
{

    UNREFERENCED_PARAMETER(DriverObject);

    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));

} // DriverUnload()

NTSTATUS 
FilterUnloadCallback(
    FLT_FILTER_UNLOAD_FLAGS Flags)
{

    UNREFERENCED_PARAMETER(Flags);

    DPF(( "%s!%s\n", __MODULE__, __FUNCTION__ ));

    // Deregister the filter (FltUnregisterFilter())
    FltUnregisterFilter ( g_FltFilter );

    return STATUS_SUCCESS; 
} // FilterUnloadCallback()


FLT_POSTOP_CALLBACK_STATUS
PostDirectoryControlCallback (
    PFLT_CALLBACK_DATA          Data,
    PCFLT_RELATED_OBJECTS       FltObjects,
    PVOID                       CompletionContext,
    FLT_POST_OPERATION_FLAGS    Flags )
{
    PFLT_IO_PARAMETER_BLOCK     Iopb = Data->Iopb;
    PUCHAR                      BufferAddress;
    ULONG                       Length;

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    // If a volume detach is in progress then exit (FLTFL_POST_OPERATION_DRAINING)
    if ( Flags & FLTFL_POST_OPERATION_DRAINING ) {
        goto Exit;
    }

    // If the operation is not successful then exit
    if( ! NT_SUCCESS( Data->IoStatus.Status ) ) {
        goto Exit;
    }

    // If the minor code of the operation is not IRP_MN_QUERY_DIRECTORY then exit
    if ( Iopb->MinorFunction != IRP_MN_QUERY_DIRECTORY ) {
        goto Exit;
    }

    // If the information class is not FileFullDirectoryInformation then exit
    if ( Iopb->Parameters.DirectoryControl.QueryDirectory.FileInformationClass != 
        FileFullDirectoryInformation ) {
        goto Exit;
    }

    // Retrieve the buffer where the data is available
    // If the buffer is invalid (NULL) then exit
    if ( ( BufferAddress = Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer ) == NULL ) {
        goto Exit;
    }

    // Retrieve the length of data in the buffer, if the length is 0 then exit
    if ( ( Length = Data->Iopb->Parameters.DirectoryControl.QueryDirectory.Length ) == 0 ) {
        goto Exit;
    }

    if ( ProcessFileFullDirectoryInformation ( BufferAddress, Length ) == TRUE ) {
        // if the file was found and the caller requested a single entry
        // then tell the caller nothing was found
        if ( Data->Iopb->OperationFlags & SL_RETURN_SINGLE_ENTRY ) {
            Data->IoStatus.Status = STATUS_NO_SUCH_FILE;
        }
    }

Exit :
    return FLT_POSTOP_FINISHED_PROCESSING;
} // PostDirectoryControlCallback()

BOOLEAN
ProcessFileFullDirectoryInformation(
    PUCHAR Buffer,
    ULONG Length )
{
    BOOLEAN Found = FALSE;
    PFILE_FULL_DIR_INFORMATION Current = (PFILE_FULL_DIR_INFORMATION)Buffer;
    PFILE_FULL_DIR_INFORMATION Previous = NULL;

    UNREFERENCED_PARAMETER(Length);

    // Step #1 : iterate through all the directory entries in Buffer and compare (_wcsnicmp())
    // the FILE_FULL_DIR_INFORMATION->FileName to g_Pattern to find if the entry contains the
    // filename that must be removed from the directory listing. This function must return TRUE
    // if the pattern is found and FALSE is the pattern is not found. 
    // Hints : 
    // *The size of every entry is FILE_FULL_DIR_INFORMATION->NextEntryOffset except for the last entry 
    //  which is FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, FileName) + FILE_FULL_DIR_INFORMATION->FileNameLength
    // *The last entry in the list has FILE_FULL_DIR_INFORMATION->NextEntryOffset == 0
    // *Directory entries are always unique i.e. cannot have more than one occurance of a given entry

    return Found;
} // ProcessFileFullDirectoryInformation()
