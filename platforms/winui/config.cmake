check_unsupported_compiler_version()

add_definitions(-DTANGRAM_WINDOWS)
add_definitions(-DTANGRAM_WINRT)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_subdirectory(platforms/winui)