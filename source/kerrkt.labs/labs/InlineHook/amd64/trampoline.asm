;/////////////////////////////////////////////////////////////////////////////
;//
;// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
;// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
;//
;/////////////////////////////////////////////////////////////////////////////

_DATA   SEGMENT
; address within the original function where execution will resume
EXTERN g_OriginalResume : QWORD
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
    ; struct _DEVICE_OBJECT  *DeviceObject,
    ; struct _IRP  *Irp
    PUSH rcx
    PUSH rdx
    
    ; Step #2 : Setup the stack for non-leaf function invocation
    SUB     rsp, 20h ; Saved registers, return address
    
    ; Step #3 : Invoke hook function written in C/C++ i.e. g_HookFunction
    call    qword ptr [g_HookFunction]

    ; Step #4 : Teardown the stack
    ADD     rsp, 20h

    ; Step #5 : Restore the parameters to the values they were set to at entry
    POP     rdx
    POP     rcx

    ; Step #6 : Insert the original instructions that were replaced by the hook instructions
    mov     rax,qword ptr [rdx+0B8h]
    mov     r9,qword ptr [rcx+40h]
    movzx   r8d,byte ptr [rax]

    ; Step #7 : Jump back to original function i.e. g_OriginalResume
    mov rax, qword ptr [g_OriginalResume]
    jmp rax
    
_TEXT    ENDS

end