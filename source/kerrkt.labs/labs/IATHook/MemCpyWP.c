/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////
#include "ntddk.h"

// memcpy() with write protect bypass
VOID MemCpyWP(
    PUCHAR Destination,
    PUCHAR Source,
    ULONG Size)
{
    ULONGLONG Cr0Register;

    // Step #1 : Read the contents of CR0
    Cr0Register = __readcr0();

    // Step #2 : Write back the contents of CR0 after disabling 
    // the write protect (WP) bit i.e. bit 16
    __writecr0(Cr0Register & (~(1 << 16)));

    // Step #3 : Write the contents to read-only memory address
    RtlCopyMemory(Destination, Source, Size);

    // Step #4 : Write the original contents back to CR0
    __writecr0(Cr0Register);
}
