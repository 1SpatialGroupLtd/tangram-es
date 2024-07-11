@echo off

if "%PATH%"=="%PATH:\Tools\MSVC\=%" (
	if not defined VC_VARS_PATH (
		call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
	) else ( call %VC_VARS_PATH% )
)

exit /b 0
