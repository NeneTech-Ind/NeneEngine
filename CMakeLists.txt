# NeneEngine/CMakeList.txt : Top-level CMake project file, do global configuration

cmake_minimum_required(VERSION 3.19)

project(
    NeneEngine
    VERSION 0.4
    DESCRIPTION "Game engine"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

function(neneengine_set_default_option option_name option_value description)
    if(NOT DEFINED ${option_name})
        set(${option_name} ${option_value} CACHE BOOL ${description})
    endif()
endfunction()

function(neneengine_try_set_python3_executable candidate_path)
    if(DEFINED Python3_EXECUTABLE AND NOT "${Python3_EXECUTABLE}" STREQUAL "")
        return()
    endif()

    if(EXISTS "${candidate_path}")
        set(Python3_EXECUTABLE "${candidate_path}" CACHE FILEPATH "Python 3 interpreter" FORCE)
    endif()
endfunction()

if(WIN32 AND (NOT DEFINED Python3_EXECUTABLE OR "${Python3_EXECUTABLE}" STREQUAL ""))
    neneengine_try_set_python3_executable("C:/Program Files/Epic Games/UE_5.6/Engine/Binaries/ThirdParty/Python3/Win64/python.exe")
    neneengine_try_set_python3_executable("$ENV{LOCALAPPDATA}/Programs/Python/Python313/python.exe")
    neneengine_try_set_python3_executable("$ENV{LOCALAPPDATA}/Programs/Python/Python312/python.exe")
    neneengine_try_set_python3_executable("$ENV{LOCALAPPDATA}/Programs/Python/Python311/python.exe")
    neneengine_try_set_python3_executable("$ENV{LOCALAPPDATA}/Programs/Python/Python310/python.exe")
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(NOT DEFINED ENV{VCPKG_ROOT} OR "$ENV{VCPKG_ROOT}" STREQUAL "")
        message(FATAL_ERROR
            "VCPKG_ROOT is not set and CMAKE_TOOLCHAIN_FILE was not provided.\n"
            "Configure the project with vcpkg, for example:\n"
            "  set VCPKG_ROOT=C:/path/to/vcpkg\n"
            "  cmake --preset x64-debug"
        )
    endif()

    if(NOT EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        message(FATAL_ERROR
            "VCPKG_ROOT is set to '$ENV{VCPKG_ROOT}', but the vcpkg toolchain file was not found.\n"
            "Expected:\n"
            "  $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        )
    endif()
endif()

include(cmake/Diligent.cmake)

add_subdirectory("src")

# include(CTest)
# if(BUILD_TESTING)
#     add_subdirectory("tests")
# endif()

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NeneEngine)
