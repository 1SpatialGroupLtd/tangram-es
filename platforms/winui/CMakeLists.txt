project(TangramWinUI)

cmake_minimum_required(VERSION 3.20)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.props"
  "${CMAKE_CURRENT_BINARY_DIR}/Directory.Build.props"
  COPYONLY
)

# IMPORTANT TO ONLY COPY IT TO OUR WINUI lib's output, otherwise it would apply to all projects(that we do not need)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.targets"
  "${CMAKE_BINARY_DIR}/platforms/winui/Directory.Build.targets"
  COPYONLY
)

# mapbox's variant type only works with 17
set(CMAKE_CXX_STANDARD 17)

add_definitions(-DWINRT_LEAN_AND_MEAN)
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-D_WINRT_DLL)
add_definitions(-D_WINDLL)
add_definitions(-DWINAPI_FAMILY=WINAPI_FAMILY_PC_APP)
add_definitions("/bigobj")
add_definitions(-DNOMINMAX)

# Standard C++ exception behavior
add_compile_options("/EHsc")

find_package(CURL REQUIRED)

set(NUGET_PACKAGES "Microsoft.Windows.CppWinRT_2.0.240405.15;Microsoft.Windows.SDK.BuildTools_10.0.22621.756;Microsoft.WindowsAppSDK_1.5.240428000")

if(NOT DEFINED ANGLE_NUGET_PACKAGE)
	if(NOT DEFINED ANGLE_INCLUDE_DIR)
		message(FATAL_ERROR "ANGLE_INCLUDE_DIR must be set and point to ANGLE's include directory like: angle/include")
	endif()

	if(NOT DEFINED ANGLE_LIB_DIR)
		message(FATAL_ERROR "ANGLE_LIB_DIR must be set and point to the directory of the ANGLE windows dlls")
	endif()
	
	set(ANGLE_GLESv2_DLL "${ANGLE_LIB_DIR}/libGLESv2.dll")
	set(ANGLE_EGL_DLL "${ANGLE_LIB_DIR}/libEGL.dll")
	set(ANGLE_LIB_FILES "${ANGLE_EGL_DLL}.lib" "${ANGLE_GLESv2_DLL}.lib")	
else()
	set(ANGLE_LIB_DIR "")
	set(ANGLE_INCLUDE_DIR "")
	set(ANGLE_LIB_FILES "")
	set(NUGET_PACKAGES "${NUGET_PACKAGES};${ANGLE_NUGET_PACKAGE}")
	set(ANGLE_GLESv2_DLL "${CMAKE_SOURCE_DIR}/angle/binaries/libGLESv2.dll")
	set(ANGLE_EGL_DLL "${CMAKE_SOURCE_DIR}/angle/binaries/libEGL.dll")
endif()

add_library(
  TangramWinUI
  SHARED
  src/TangramWinUI.def
  src/pch.h
  src/MapController.idl src/MapController.h src/MapController.cpp
  src/Marker.idl src/Marker.h src/Marker.cpp
  src/LngLat.idl src/LngLat.h src/LngLat.cpp
  src/CameraPosition.idl src/CameraPosition.h src/CameraPosition.cpp
  src/MapData.idl src/MapData.h src/MapData.cpp
  src/PickResult.idl src/PickResult.h src/PickResult.cpp
  src/TangramPlatform.h
  src/TangramPlatform.cpp
  src/Renderer.h
  src/Renderer.cpp
  ${CMAKE_SOURCE_DIR}/platforms/common/platform_gl.cpp
  ${CMAKE_SOURCE_DIR}/platforms/common/urlClient.cpp
)

target_include_directories(
  TangramWinUI
  PRIVATE
  .
  ${ANGLE_INCLUDE_DIR}
  ${CURL_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/platforms/common
)

set_target_properties(
  TangramWinUI   
  PROPERTIES
  VS_PACKAGE_REFERENCES "Microsoft.Windows.CppWinRT_2.0.240405.15;Microsoft.Windows.SDK.BuildTools_10.0.22621.756;Microsoft.WindowsAppSDK_1.5.240428000"
  VS_GLOBAL_ProjectName TangramWinUI
  VS_GLOBAL_TargetName TangramWinUI
  VS_GLOBAL_ROOTNAMESPACE TangramWinUI
  VS_GLOBAL_WindowsAppContainer false
  VS_GLOBAL_UseWinUI true
  VS_GLOBAL_CppWinRTOptimized true
  VS_GLOBAL_CppWinRTRootNamespaceAutoMerge true
  VS_GLOBAL_CppWinRTGenerateWindowsMetadata true
  VS_GLOBAL_MinimalCoreWin true
  VS_GLOBAL_AppContainerApplication false
  VS_GLOBAL_AppxPackage false
  VS_GLOBAL_ApplicationType "Windows Store"
  VS_GLOBAL_RuntimeIdentifier win10-x64
)
 
target_link_libraries(
  TangramWinUI
  PRIVATE
  glm
  tangram-core
  wsock32
  ws2_32
  crypt32
  wldap32
  ${CURL_LIBRARIES}
  ${ANGLE_LIB_FILES}
)

# Set source file dependencies properly for Xaml and non-Xaml IDL
# files.
#
# Without this, `module.g.cpp` will not include the necessary headers
# for non-Xaml IDL files, e.g. value converters
get_target_property(SOURCES TangramWinUI SOURCES)


foreach(SOURCE ${SOURCES})
  cmake_path(GET SOURCE EXTENSION LAST_ONLY EXTENSION)

  if(NOT "${EXTENSION}" STREQUAL ".idl")
    continue()
  endif()

get_filename_component(ABS_IDL_SOURCE "${SOURCE}"
                       REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

cmake_path(REMOVE_EXTENSION SOURCE OUTPUT_VARIABLE BASENAME)

  # AdditionalIncludeDirectories is needed for MIDL generator to find our .idl files that we import/cross reference in each other
  set_property(
    SOURCE "${ABS_IDL_SOURCE}"
    PROPERTY VS_SETTINGS
    "SubType=Code"
    "AdditionalIncludeDirectories=${CMAKE_SOURCE_DIR}/platforms/winui/src" 
  )

  set_property(
    SOURCE "${BASENAME}.h"
    PROPERTY VS_SETTINGS
    "DependentUpon=${ABS_IDL_SOURCE}"
  )
  set_property(
    SOURCE "${BASENAME}.cpp"
    PROPERTY VS_SETTINGS
    "DependentUpon=${ABS_IDL_SOURCE}"
  )
endforeach()

# mpve
add_custom_command(
	TARGET TangramWinUI
	POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${ANGLE_GLESv2_DLL}" $<TARGET_FILE_DIR:TangramWinUI>
)

add_custom_command(
	TARGET TangramWinUI
	POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${ANGLE_EGL_DLL}" $<TARGET_FILE_DIR:TangramWinUI>
)