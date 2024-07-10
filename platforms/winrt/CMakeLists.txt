project(TangramWinRT)

cmake_minimum_required(VERSION 3.20)

SET(WINDOWS_SDK_VERSION 10.0.22621.0)
set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)

add_definitions("/MDd")
add_definitions("/ZW")
add_definitions("/EHsc")
add_definitions("/nologo")
add_definitions("/GR")
add_definitions("/bigobj")

add_definitions("/AI\"c:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Tools/MSVC/14.29.30133/lib/x86/store/references\"")
add_definitions("/AI\"C:/Program Files (x86)/Windows Kits/10/UnionMetadata/10.0.22621.0\"")

add_definitions(-D_VARIADIC_MAX=10)
add_definitions(-DGOTHIC_GUI_ABSENT)
add_definitions(-D_WINRT_DLL)
add_definitions(-D_WINDLL)
add_definitions(-DWINAPI_FAMILY=WINAPI_FAMILY_APP)

string(REPLACE "DWIN32" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE "D\_WINDOWS" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

add_library(TangramWinRT
        SHARED
        src/TangramMap.h
        src/TangramMap.cpp
        src/TangramPlatform.h
        src/TangramPlatform.cpp
		src/Renderer.h
        src/Renderer.cpp
        ${CMAKE_SOURCE_DIR}/platforms/common/platform_gl.cpp
        ${CMAKE_SOURCE_DIR}/platforms/common/urlClient.cpp
)

set_property(
        TARGET TangramWinRT
        PROPERTY VS_WINRT_COMPONENT FALSE)

set_property(TARGET TangramWinRT PROPERTY VS_PACKAGE_REFERENCES "ANGLE.WindowsStore_2.1.13" )

target_include_directories(TangramWinRT
        PUBLIC
        .
        ${CURL_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/platforms/common
)
set_target_properties(
        TangramWinRT
        PROPERTIES
        LINK_FLAGS "/NOLOGO /MANIFEST /WINMD /MANIFESTUAC:NO /NXCOMPAT /DYNAMICBASE /TLBID:1 /DLL /SUBSYSTEM:CONSOLE /INCREMENTAL:NO"
)

target_link_libraries(TangramWinRT PRIVATE tangram-core wsock32 ws2_32 crypt32 wldap32 ${CURL_LIBRARIES})