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

# Diligent Engine configs
neneengine_set_default_option(DILIGENT_BUILD_SAMPLES OFF "Build Diligent samples")
neneengine_set_default_option(DILIGENT_BUILD_TOOLS   OFF "Build Diligent tools")
neneengine_set_default_option(DILIGENT_BUILD_FX      OFF "Build Diligent FX")
neneengine_set_default_option(DILIGENT_BUILD_TESTS   OFF "Build Diligent tests")
neneengine_set_default_option(DILIGENT_INSTALL_CORE  OFF "Install Diligent core")
neneengine_set_default_option(DILIGENT_INSTALL_TOOLS OFF "Install Diligent tools")

neneengine_set_default_option(DILIGENT_NO_DIRECT3D11 ON  "Disable Direct3D11 backend")
neneengine_set_default_option(DILIGENT_NO_VULKAN     ON  "Disable Vulkan backend")
neneengine_set_default_option(DILIGENT_NO_OPENGL     ON  "Disable OpenGL backend")
neneengine_set_default_option(DILIGENT_NO_GLES       ON  "Disable GLES backend")
neneengine_set_default_option(DILIGENT_NO_METAL      ON  "Disable Metal backend")
neneengine_set_default_option(DILIGENT_NO_DIRECT3D12 OFF "Disable Direct3D12 backend")

set(NENEENGINE_DILIGENT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/DiligentEngine")
set(NENEENGINE_DILIGENT_CORE_DIR "${NENEENGINE_DILIGENT_ROOT}/DiligentCore")
set(NENEENGINE_DILIGENT_TOOLS_DIR "${NENEENGINE_DILIGENT_ROOT}/DiligentTools")

if(NOT EXISTS "${NENEENGINE_DILIGENT_CORE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "DiligentEngine sources were not found at '${NENEENGINE_DILIGENT_CORE_DIR}'.\n"
        "The 'external/DiligentEngine' submodule is probably not initialized.\n"
        "Run:\n"
        "  git submodule update --init --recursive"
    )
endif()

add_subdirectory("external/DiligentEngine/DiligentCore")
set(DILIGENT_TOOLS_DIR "${NENEENGINE_DILIGENT_TOOLS_DIR}")
add_subdirectory("external/DiligentEngine/DiligentTools/ThirdParty")
add_subdirectory("external/DiligentEngine/DiligentTools/TextureLoader")

foreach(target_name SPIRV-Tools-opt SPIRV-Tools SPIRV-Tools-core)
    if(TARGET ${target_name})
        get_target_property(aliased_target ${target_name} ALIASED_TARGET)
        if(aliased_target)
            target_compile_options(${aliased_target} PRIVATE /wd5232 /wd4717)
            set_target_properties(${aliased_target} PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
            message(STATUS "[Diligent Fix] Applied to aliased target: ${aliased_target}")
        else()
            target_compile_options(${target_name} PRIVATE /wd5232 /wd4717)
            set_target_properties(${target_name} PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
            message(STATUS "[Diligent Fix] Applied to target: ${target_name}")
        endif()
    endif()
endforeach()

add_subdirectory("src")

# include(CTest)
# if(BUILD_TESTING)
#     add_subdirectory("tests")
# endif()

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NeneEngine)
