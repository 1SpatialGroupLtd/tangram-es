call load_msvc_toolchain.cmd
call vcpkg_init.cmd

set ANGLE_NUGET_PACKAGE=Google.Angle
set ANGLE_NUGET_PACKAGE_VERSION=2.1.20572

cmake -S . -B .\build\winui -G "Visual Studio 17 2022" -DCMAKE_GENERATOR_PLATFORM=x64,version=10.0.19041.0 -DTANGRAM_PLATFORM=winui -DANGLE_NUGET_PACKAGE=%ANGLE_NUGET_PACKAGE% -DANGLE_NUGET_PACKAGE_VERSION=%ANGLE_NUGET_PACKAGE_VERSION% -DCMAKE_TOOLCHAIN_FILE=c:\Users\%USERNAME%\.vcpkg\scripts\buildsystems\vcpkg.cmake