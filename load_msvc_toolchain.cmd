@echo off
if "%PATH%"=="%PATH:\2022\Professional\VC\Tools\MSVC\=%" (
	call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
)
exit /b 0
