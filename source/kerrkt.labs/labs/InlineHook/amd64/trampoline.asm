;/////////////////////////////////////////////////////////////////////////////
;//
;// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
;// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
;//
;/////////////////////////////////////////////////////////////////////////////

_DATA   SEGMENT
; address within the original function where execution will resume
EXTERN g_OrignalResume : QWORD
; address of hook function
EXTERN g_HookFunction : QWORD
_DATA    ENDS

; VOID Trampoline( VOID )
; ASM wrapper to setup the environment for 
; * Invoking the HookFunction() written in HLL
; * Transferring execution control back to the original function
; The hooking instructions jumps to this trampoline
_TEXT   SEGMENT
public Trampoline
Trampoline:
    ; Step #1 : Save the parameters passed to the original function

    ; Step #2 : Setup the stack for non-leaf function invocation

    ; Step #3 : Invoke hook function written in C/C++ i.e. g_HookFunction

    ; Step #4 : Teardown the stack

    ; Step #5 : Restore the parameters to the values they were set to at entry

    ; Step #6 : Insert the original instructions that were replaced by the hook instructions

    ; Step #7 : Jump back to original function i.e. g_OrignalResume

_TEXT    ENDS

end