rem call generate_winui_project.bat
if %errorlevel% neq 0 exit /b %errorlevel%

:: Config
if "%1" == "" (
   set config="Release"
) else (
   set config="%1"
)

cmake --build build/winui --config %config%