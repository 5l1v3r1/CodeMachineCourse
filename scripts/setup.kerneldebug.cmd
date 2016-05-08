bcdedit /copy {current} /d "Windows [debugger disabled]"
bcdedit /debug {current} ON
bcdedit /set {current} debugtype SERIAL 
bcdedit /set {current} debugport 1
bcdedit /set {current} baudrate 115200

