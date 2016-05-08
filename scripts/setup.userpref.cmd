wmic rdpermissions where TerminalName="RDP-Tcp" CALL AddAccount "%USERNAME%",1 
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced" /v SuperHidden /t REG_DWORD /d 0x1 /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced" /v ShowSuperHidden /t REG_DWORD /d 0x1 /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced" /v HideFileExt /t REG_DWORD /d 0x0 /f
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\CabinetState" /v FullPath /t REG_DWORD /d 0x1 /f
reg add "HKCU\Control Panel\Desktop" /v WindowArrangementActive /t REG_SZ /d "0" /f

