/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntifs.h"
#include "ntimage.h"

#define __MODULE__  "ExportScan"

#pragma warning (disable:4055)

PVOID
GetNTOSExport(
    PUCHAR ModuleBase,
    PCHAR  FunctionName);

// compute the base of NTOSKRNL.exe in KVAS
PUCHAR
FindNTOSBase(
    VOID)
{
    PUCHAR KiSystemCall64Address;
    PUCHAR NTOSKrnlBase;

    // Step #1 : Read the value from the LSTAR MSR into KiSystemCall64Address
    KiSystemCall64Address = (PUCHAR)__readmsr(0xc0000082);

    // Step #2 : Search backwards from KiSystemCall64 to locate the PE header
    // of NTOSKrnl
    NTOSKrnlBase = (PUCHAR)((UINT_PTR)KiSystemCall64Address & ~0xFFF); //Round down to nearest align boundary (0x1000) size
    while (*(PUINT32)NTOSKrnlBase != (0xFFFFFF & 0x905a4d)) // {'M', 'Z', 0x90}
    {
        NTOSKrnlBase -= 0x1000;
    }


    return NTOSKrnlBase;
} // FindNTOSBase()

// Step #3 : Add a compiler directive to ensure that the driver does not link 
// to memcmp() as a function
#pragma intrinsic( memcmp )

// Step #4 : Typedef the prototype to call DbgPrint() via a function pointer
typedef void(*DbgPrintFn)(PCHAR Format, ...);

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath)
{
    PUCHAR NTOSKrnlBase;
    PUCHAR DbgPrintFunction;
    CHAR DbgPrintName[] = "DbgPrint";
    CHAR DisplayString[] = "*** Hello From ShellCode ***\n";

    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    NTOSKrnlBase = FindNTOSBase();

    DbgPrintFunction = GetNTOSExport(NTOSKrnlBase, DbgPrintName);

    // Step #5 : Call DbgPrint() using the pointer DbgPrintFunction 
    // to print the string DisplayString in the debugger 
    ((DbgPrintFn)DbgPrintFunction)("%s\n", DisplayString);

    return STATUS_UNSUCCESSFUL;
} // DriverEntry()


PVOID
GetNTOSExport(
    PUCHAR ModuleBase,
    PCHAR  FunctionName)
{
    PIMAGE_DOS_HEADER           DosHeader;
    PIMAGE_NT_HEADERS64         NtHeader;
    PIMAGE_OPTIONAL_HEADER64    OptionalHeader;
    PIMAGE_DATA_DIRECTORY       DataDirectory;
    PIMAGE_EXPORT_DIRECTORY     ExportDirectory;
    PULONG                      FunctionTable;
    PULONG                      NameTable;
    PUSHORT                     NameOrdinalTable;
    ULONG                       FunctionNameLen;
    ULONG                       NameIdx;

    // Using ModuleBase as the base load address setup the pointers
    // DosHeader, NtHeader, OptionalHeader, DataDirectory and ExportDirectory
    DosHeader = (PIMAGE_DOS_HEADER)ModuleBase;
    NtHeader = (PIMAGE_NT_HEADERS64)(ModuleBase + DosHeader->e_lfanew);
    OptionalHeader = (PIMAGE_OPTIONAL_HEADER64)&NtHeader->OptionalHeader;
    DataDirectory = (PIMAGE_DATA_DIRECTORY)&OptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    ExportDirectory = (PIMAGE_EXPORT_DIRECTORY)(ModuleBase + DataDirectory->VirtualAddress);

    // Retrieve the pointers to the FunctionTable, NameTable and NameOrdinalTable
    // from the ExportDirectory of the module pointed to by ModuleBase
    FunctionTable = (PULONG)(ModuleBase + ExportDirectory->AddressOfFunctions);
    NameTable = (PULONG)(ModuleBase + ExportDirectory->AddressOfNames);
    NameOrdinalTable = (PUSHORT)(ModuleBase + ExportDirectory->AddressOfNameOrdinals);

    // Store the length of the FunctionName string in FunctionNameLen
    // Equivalent functionality of strlen()
    for (FunctionNameLen = 0; FunctionName[FunctionNameLen] != 0; FunctionNameLen++);

    // iterate through all the names in the NameTable and look for the function name that was requested
    for (NameIdx = 0; NameIdx < ExportDirectory->NumberOfNames; NameIdx++) {
        PCHAR Name;

        // Use the RVA in the NameTable to get the VA of the name string for comparison
        Name = (PCHAR)(ModuleBase + NameTable[NameIdx]);

        // compare the name in the NameTable with the name of the function provided by the caller
        if (memcmp(Name, FunctionName, (FunctionNameLen + 1)) == 0) {

            USHORT NameOrdinal;
            PVOID Address;

            // Using the index that was used in the NameTable, retrieve the 
            // NameOrdinal from the NameOrdinalTable
            NameOrdinal = NameOrdinalTable[NameIdx];

            // Using NameOrdinal, retrieve the RVA from the FunctionTable
            // and then convert it to the VA of the function that will be returned
            // to the caller
            Address = (PVOID)(ModuleBase + FunctionTable[NameOrdinal]);

            return Address;
        }
    }

    return NULL;
} // GetNTOSExport()
