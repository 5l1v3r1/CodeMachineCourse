wmic recoveros set DebugInfoType = 1
wmic recoveros set OverwriteExistingDebugFile = 1
reg add "HKLM\SYSTEM\CurrentControlSet\Control\CrashControl" /v AlwaysKeepMemoryDump /t REG_DWORD /d 0x1
reg add "HKLM\SYSTEM\CurrentControlSet\Control\CrashControl" /v NMICrashDump /t REG_DWORD /d 0x1
reg add "HKLM\SYSTEM\CurrentControlSet\Services\kbdhid\Parameters" /v CrashOnCtrlScroll /t REG_DWORD /d 0x1
reg add "HKLM\SYSTEM\CurrentControlSet\Services\i8042prt\Parameters" /v CrashOnCtrlScroll /t REG_DWORD /d 0x1
reg add "HKLM\Software\Microsoft\Windows\Windows Error Reporting\LocalDumps"
reg add "HKLM\Software\Microsoft\Windows\Windows Error Reporting\LocalDumps" /v DumpType /t REG_DWORD /d 0x2 
reg add "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Debug Print Filter" /v DEFAULT  /t REG_DWORD /d 0xffffffff
reg add "HKLM\System\CurrentControlSet\Control\Session Manager\Memory Management" /v DisablePagingExecutive  /t REG_DWORD /d 1 /f
 