cmake_minimum_required(VERSION 3.20)

project(SAWindowFix
    VERSION 1.0
    DESCRIPTION "Borderless windowed mode for GTA San Andreas"
    LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "SAWindowFix requires MSVC.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "SAWindowFix must be built as 32-bit: cmake -A Win32 ...")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Git QUIET)
set(VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h)
add_custom_target(SAWindowFixVersion ALL
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/src/Version.h.in
        -DOUTPUT=${VERSION_HEADER}
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -DBASE_MAJOR=${PROJECT_VERSION_MAJOR}
        -DBASE_MINOR=${PROJECT_VERSION_MINOR}
        -DNAME=${PROJECT_NAME}
        "-DDESCRIPTION=${PROJECT_DESCRIPTION}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.cmake
    BYPRODUCTS ${VERSION_HEADER}
    COMMENT "Generating version from git"
    VERBATIM)

add_library(SAWindowFix SHARED
    src/DllMain.cpp
    src/Mod.cpp
    src/Mod.h
    src/Device.cpp
    src/Device.h
    src/Window.cpp
    src/Window.h
    src/Config.cpp
    src/Config.h
    src/Log.cpp
    src/Log.h
    src/CrashHandler.cpp
    src/CrashHandler.h
    src/Game.h
    src/Memory.h
    src/Paths.h
    src/Version.h.in
    src/Version.rc
)

add_dependencies(SAWindowFix SAWindowFixVersion)
target_include_directories(SAWindowFix PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_definitions(SAWindowFix PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(SAWindowFix PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8>)
target_link_libraries(SAWindowFix PRIVATE user32)
set_target_properties(SAWindowFix PROPERTIES SUFFIX ".asi")

set(GTA_SA_DIR "" CACHE PATH "GTA:SA install dir; the .asi is copied there after build")
if(GTA_SA_DIR)
    add_custom_command(TARGET SAWindowFix POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SAWindowFix> "${GTA_SA_DIR}/"
        COMMENT "Copying SAWindowFix.asi to ${GTA_SA_DIR}")
endif()
