# NeneEngine/src/CMakeList.txt : CMake project for NeneEngine, include source and define project specific logic here.

add_executable(NeneEngine WIN32
    "${CMAKE_CURRENT_SOURCE_DIR}/../WinMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../NeneEngine.rc"
)

target_sources(NeneEngine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/Win32ResourceIds.h"
)

add_subdirectory(App)
add_subdirectory(Core)
add_subdirectory(ECS)
add_subdirectory(Input)
add_subdirectory(Platform)
add_subdirectory(RenderAdapters)
add_subdirectory(Rendering)
add_subdirectory(Scene)
add_subdirectory(GameStates)

target_include_directories(NeneEngine
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
)

find_package(spdlog         CONFIG REQUIRED)
find_package(assimp         CONFIG REQUIRED)
find_package(nlohmann_json  CONFIG REQUIRED)
find_package(EASTL          CONFIG REQUIRED)
find_package(EnTT           CONFIG REQUIRED)
find_package(glm            CONFIG REQUIRED)
find_package(Stb            MODULE REQUIRED)

target_include_directories(NeneEngine PRIVATE ${Stb_INCLUDE_DIR})

target_link_libraries(NeneEngine
    PRIVATE
        assimp::assimp
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        EASTL
        EnTT::EnTT
        glm::glm
        Diligent-Win32Platform
        Diligent-GraphicsEngineD3D12-shared
        Diligent-TextureLoader
)

set(NENEENGINE_VCPKG_TRIPLET "${VCPKG_TARGET_TRIPLET}")
if(NOT NENEENGINE_VCPKG_TRIPLET)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(NENEENGINE_VCPKG_TRIPLET "x64-windows")
    else()
        set(NENEENGINE_VCPKG_TRIPLET "x86-windows")
    endif()
endif()

set(NENEENGINE_VCPKG_ROOT_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/${NENEENGINE_VCPKG_TRIPLET}")
file(GLOB NENEENGINE_VCPKG_DEBUG_DLLS   "${NENEENGINE_VCPKG_ROOT_DIR}/debug/bin/*.dll")
file(GLOB NENEENGINE_VCPKG_RELEASE_DLLS "${NENEENGINE_VCPKG_ROOT_DIR}/bin/*.dll")

set(NENEENGINE_VCPKG_RUNTIME_DLLS "${NENEENGINE_VCPKG_RELEASE_DLLS}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(NENEENGINE_VCPKG_RUNTIME_DLLS "${NENEENGINE_VCPKG_DEBUG_DLLS}")
endif()

if(NENEENGINE_VCPKG_RUNTIME_DLLS)
    add_custom_command(TARGET NeneEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${NENEENGINE_VCPKG_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:NeneEngine>
        COMMAND_EXPAND_LISTS
    )
endif()

install(TARGETS
    NeneEngine
    Diligent-GraphicsEngineD3D12-shared
    Diligent-Archiver-shared
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

if(NENEENGINE_VCPKG_DEBUG_DLLS)
    install(FILES ${NENEENGINE_VCPKG_DEBUG_DLLS}
        CONFIGURATIONS Debug
        DESTINATION bin
        COMPONENT Runtime
    )
endif()

if(NENEENGINE_VCPKG_RELEASE_DLLS)
    install(FILES ${NENEENGINE_VCPKG_RELEASE_DLLS}
        CONFIGURATIONS Release RelWithDebInfo MinSizeRel
        DESTINATION bin
        COMPONENT Runtime
    )
endif()

if(D3D_COMPILER_PATH)
    install(FILES "${D3D_COMPILER_PATH}" DESTINATION bin COMPONENT Runtime)
endif()

if(DXC_COMPILER_PATH AND DXIL_SIGNER_PATH)
    install(FILES
        "${DXC_COMPILER_PATH}"
        "${DXIL_SIGNER_PATH}"
        DESTINATION bin
        COMPONENT Runtime
    )
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../assets" DESTINATION . COMPONENT Runtime)

if(COMMAND copy_required_dlls)
    copy_required_dlls(NeneEngine)
else()
    message(WARNING "copy_required_dlls was not found; Diligent may not be configured correctly")
endif()

target_compile_features(NeneEngine PRIVATE cxx_std_20)
