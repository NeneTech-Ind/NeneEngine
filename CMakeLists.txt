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

include(cmake/Diligent.cmake)

add_subdirectory("src")

# include(CTest)
# if(BUILD_TESTING)
#     add_subdirectory("tests")
# endif()

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NeneEngine)
