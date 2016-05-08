mkdir .\x64\Debug\package
copy .\x64\Debug\NdisLwf.sys .\x64\Debug\package
copy .\NdisLwf.inf .\x64\Debug\package
inf2cat /os:10_X64 /driver:.\x64\Debug\package
SignTool sign /v /s CodeMachineTest /n "CodeMachineTest" /t http://timestamp.verisign.com/scripts/timstamp.dll .\x64\Debug\package\ndislwf.cat