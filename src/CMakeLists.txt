# NeneEngine/src/CMakeList.txt : CMake project for NeneEngine, include source and define project specific logic here.

add_executable(NeneEngine WIN32
    "${CMAKE_CURRENT_SOURCE_DIR}/WinMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/NeneEngine.rc"
)

set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/NeneEngine.rc" PROPERTIES
    OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/icons/app.ico"
)

target_sources(NeneEngine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/Win32ResourceIds.h"
)

add_subdirectory(App)
add_subdirectory(Core)
add_subdirectory(ECS)
add_subdirectory(Editor)
add_subdirectory(Graphics)
add_subdirectory(Input)
add_subdirectory(Platform)
add_subdirectory(Scene)
add_subdirectory(Scripting)
add_subdirectory(GameStates)

target_include_directories(NeneEngine
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
)

set(NENEENGINE_IMGUIZMO_DIR "${CMAKE_SOURCE_DIR}/external/imguizmo/src")
target_sources(NeneEngine PRIVATE
    "${NENEENGINE_IMGUIZMO_DIR}/ImGuizmo.cpp"
    "${NENEENGINE_IMGUIZMO_DIR}/ImGuizmo.h"
)
target_include_directories(NeneEngine PRIVATE "${NENEENGINE_IMGUIZMO_DIR}")

set(NENEENGINE_VCPKG_TRIPLET "${VCPKG_TARGET_TRIPLET}")
if(NOT NENEENGINE_VCPKG_TRIPLET)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(NENEENGINE_VCPKG_TRIPLET "x64-windows")
    else()
        set(NENEENGINE_VCPKG_TRIPLET "x86-windows")
    endif()
endif()

set(NENEENGINE_VCPKG_ROOT_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/${NENEENGINE_VCPKG_TRIPLET}")
if(EXISTS "${NENEENGINE_VCPKG_ROOT_DIR}")
    list(PREPEND CMAKE_PREFIX_PATH "${NENEENGINE_VCPKG_ROOT_DIR}")
endif()

find_package(spdlog         CONFIG REQUIRED)
find_package(assimp         CONFIG REQUIRED)
find_package(nlohmann_json  CONFIG REQUIRED)
find_package(EASTL          CONFIG REQUIRED)
find_package(EnTT           CONFIG REQUIRED)
find_package(enkiTS         CONFIG REQUIRED)
find_package(glm            CONFIG REQUIRED)
find_package(Stb            MODULE REQUIRED)
find_package(Jolt           CONFIG REQUIRED)
find_package(unofficial-lua CONFIG REQUIRED)
find_package(sol2           CONFIG REQUIRED)

target_compile_definitions(NeneEngine PRIVATE NENEENGINE_VERSION="${PROJECT_VERSION}")

option(NENEENGINE_ENABLE_TRACY "Enable Tracy CPU profiling" ON)
if(NENEENGINE_ENABLE_TRACY)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(NeneEngine PRIVATE Tracy::TracyClient)
    target_compile_definitions(NeneEngine PRIVATE NENEENGINE_ENABLE_TRACY)

    # vcpkg installs the debug TracyClient.dll into debug/bin/Debug, which the runtime DLL globs below do not cover.
    add_custom_command(TARGET NeneEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Tracy::TracyClient>
            $<TARGET_FILE_DIR:NeneEngine>
    )
    install(FILES $<TARGET_FILE:Tracy::TracyClient> DESTINATION . COMPONENT Runtime)
endif()

target_include_directories(NeneEngine PRIVATE ${Stb_INCLUDE_DIR})

target_link_libraries(NeneEngine
    PRIVATE
        assimp::assimp
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        EASTL
        EnTT::EnTT
        enkiTS::enkiTS
        glm::glm
        Jolt::Jolt
        unofficial::lua::lua
        sol2::sol2
        Diligent-Win32Platform
        Diligent-GraphicsEngineD3D12-shared
        Diligent-TextureLoader
        Diligent-Imgui
		comdlg32
)

get_target_property(NENEENGINE_LUA_LIBRARY_TYPE unofficial::lua::lua TYPE)
if(WIN32 AND NENEENGINE_LUA_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET NeneEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:unofficial::lua::lua>"
            "$<TARGET_FILE_DIR:NeneEngine>"
        VERBATIM
    )
    install(FILES "$<TARGET_FILE:unofficial::lua::lua>" DESTINATION . COMPONENT Runtime)
endif()

file(GLOB NENEENGINE_VCPKG_DEBUG_DLLS   "${NENEENGINE_VCPKG_ROOT_DIR}/debug/bin/*.dll")
file(GLOB NENEENGINE_VCPKG_RELEASE_DLLS "${NENEENGINE_VCPKG_ROOT_DIR}/bin/*.dll")
# Lua is deployed from its imported target, which selects the active build configuration.
list(FILTER NENEENGINE_VCPKG_DEBUG_DLLS EXCLUDE REGEX "/lua[.]dll$")
list(FILTER NENEENGINE_VCPKG_RELEASE_DLLS EXCLUDE REGEX "/lua[.]dll$")
list(FILTER NENEENGINE_VCPKG_DEBUG_DLLS EXCLUDE REGEX "/draco[.]dll$")
list(FILTER NENEENGINE_VCPKG_RELEASE_DLLS EXCLUDE REGEX "/draco[.]dll$")

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
    RUNTIME DESTINATION .
    COMPONENT Runtime
)

if(NENEENGINE_VCPKG_DEBUG_DLLS)
    install(FILES ${NENEENGINE_VCPKG_DEBUG_DLLS}
        CONFIGURATIONS Debug
        DESTINATION .
        COMPONENT Runtime
    )
endif()

if(NENEENGINE_VCPKG_RELEASE_DLLS)
    install(FILES ${NENEENGINE_VCPKG_RELEASE_DLLS}
        CONFIGURATIONS Release RelWithDebInfo MinSizeRel
        DESTINATION .
        COMPONENT Runtime
    )
endif()

configure_file("${CMAKE_SOURCE_DIR}/packaging/README.txt.in" "${CMAKE_CURRENT_BINARY_DIR}/README.txt" @ONLY NEWLINE_STYLE CRLF)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/README.txt"
    "${CMAKE_SOURCE_DIR}/LICENSE.txt"
    "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt"
    DESTINATION .
    COMPONENT Runtime
)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)
list(FILTER CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS INCLUDE REGEX "/(msvcp140|vcruntime140|vcruntime140_1)[.]dll$")
install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION . COMPONENT Runtime)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
    DESTINATION .
    COMPONENT Runtime
    PATTERN "readme" EXCLUDE
    PATTERN "icons" EXCLUDE
    PATTERN "config/editor_theme.json" EXCLUDE
)

if(COMMAND copy_required_dlls)
    copy_required_dlls(NeneEngine)
else()
    message(WARNING "copy_required_dlls was not found; Diligent may not be configured correctly")
endif()

target_compile_features(NeneEngine PRIVATE cxx_std_20)

add_executable(NeneVisibilityTests EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/ecs/VisibilityRegistryTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/EASTLAllocator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ECS/VisibilityRegistry.cpp"
)
target_include_directories(NeneVisibilityTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(NeneVisibilityTests PRIVATE EASTL EnTT::EnTT glm::glm)
