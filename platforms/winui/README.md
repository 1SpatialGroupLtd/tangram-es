Windows
======================
## Information ##

This builds tangram-es for target winUI 3 and also generates/creates bindings that can be just directly used in c++ WinUI3 apps.
To use it in C# you will need to generate C# bindings using CsWinRT library: https://github.com/microsoft/CsWinRT.
MAUI also supports WinUI3, so once you generate the C# bindings, you can use them in MAUI once you write the handlers.

It uses Google ANGLE library that we forked (https://github.com/1SpatialGroupLtd/angle) as it had an issue compiling it on windows when i tried last time.
ANGLE mimics/transposes the openGL API to D3D as WinUI windows can only use that.

## Setup ##

This project uses CMake (minimum version 3.10), you can download it [here](http://www.cmake.org/download/)
To build on Windows you will need Visual Studio 2022 and SDK 10.0.19041.0 installed

Needs [vcpkg](https://github.com/microsoft/vcpkg) installed as well.

The example commands excepts it to be installed at c:\Users\%USERNAME%\.vcpkg
OR you can use this to initialize the vcpkg automatically at the correct path:
```
call vcpkg_init.cmd
```

The required files from ANGLE are:
	- The "include" dir
	- DLLs and LIBs: libGLESv2.dll, libEGL.dll, libGLESv2.dll.lib, libEGL.dll.lib
	
To build these binaries just follow the readme of the ANGLE repo.

Make sure to update git submodules before you build:

```
git submodule update --init --recursive
```


## Build ##


First, generate a Visual Studio solution using CMake on the command line:

```
mkdir windows-build-winui
cmake -S . -B .\windows-build-winui -G "Visual Studio 17 2022" -DCMAKE_GENERATOR_PLATFORM=x64,version=10.0.19041.0 -DTANGRAM_PLATFORM=winui -DCMAKE_TOOLCHAIN_FILE=c:\Users\%USERNAME%\.vcpkg\scripts\buildsystems\vcpkg.cmake
```

Then run the build using the CMake build option:

```
cmake --build windows-build-winui --config Release
```

## Outcome ##

The built TangramWinUI.dll and its dependencies will be available in the tangram-es\windows-build-winui\platforms\winui\Release dir.

## Bindings ##

To generate C# Bindings you will need to follow tutorial of CsWinRT.
You will need to add the generated TangramWinUI.vcxproj as a reference for your C# binding .csproj,
then it will generate the proper bindings that you can use to create a nuget package.
