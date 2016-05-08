@echo off
set usage="usage : install.driver <ServiceName>"
if "%1"=="" goto done
sc create %1 type= kernel start= demand error= normal binPath= System32\Drivers\%1.sys DisplayName= %1
goto exit
:done
echo %usage%
:exit