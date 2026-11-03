list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
find_package(spdlog CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(EASTL CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(NeneEngineEditorTests
    EditorTests.cpp
    ../src/Core/EASTLAllocator.cpp
    ../src/Core/NeneLogger.cpp
    ../src/Core/PathResolver.cpp
    ../src/Core/ResourceManager.cpp
    ../src/ECS/World.cpp
    ../src/ECS/MeshRenderRuntimeComponent.cpp
    ../src/ECS/Systems/RenderSystem.cpp
    ../src/Editor/EditorHistory.cpp
    ../src/Editor/EditorSceneSnapshot.cpp
    ../src/Editor/EditorAssetActions.cpp
    ../src/Editor/AssetCatalog.cpp
    ../src/Editor/Panels/ViewportPanel.cpp
    ../src/Editor/Panels/MaterialEditorPanel.cpp
    ../src/Graphics/Backend/IRenderAdapter.cpp
    ../src/Graphics/Runtime/SceneAssetResolver.cpp
    ../src/Graphics/Loaders/MeshLoader.cpp
    ../src/Graphics/Loaders/TextureLoader.cpp
    ../src/Graphics/Loaders/ShaderLoader.cpp
    ../src/Graphics/Loaders/ResourceLoaderRegistration.cpp
    ../src/Scene/SceneSerializer.cpp
    ../src/Scene/Instantiation/ModelInstanceConfig.cpp
    ../external/imguizmo/src/ImGuizmo.cpp
)
target_include_directories(NeneEngineEditorTests PRIVATE ../include ../external/imguizmo/src)
target_link_libraries(NeneEngineEditorTests PRIVATE assimp::assimp spdlog::spdlog nlohmann_json::nlohmann_json
    EASTL EnTT::EnTT glm::glm Diligent-Imgui)
target_compile_features(NeneEngineEditorTests PRIVATE cxx_std_20)
add_dependencies(NeneEngineEditorTests NeneEngine)
set_target_properties(NeneEngineEditorTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src")
add_test(NAME EditorRegression COMMAND NeneEngineEditorTests)
set_tests_properties(EditorRegression PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(NeneEngineColorTests
    ColorPresentationTests.cpp
    ../src/Core/EASTLAllocator.cpp
    ../src/Core/NeneLogger.cpp
    ../src/Graphics/Backend/IRenderAdapter.cpp
    ../src/Graphics/Backend/DiligentDX12Adapter.cpp
    ../src/Graphics/Backend/DiligentImGuiBackend.cpp
)
target_include_directories(NeneEngineColorTests PRIVATE ../include)
target_link_libraries(NeneEngineColorTests PRIVATE EASTL spdlog::spdlog glm::glm
    Diligent-Win32Platform Diligent-GraphicsEngineD3D12-shared Diligent-TextureLoader Diligent-Imgui)
target_compile_features(NeneEngineColorTests PRIVATE cxx_std_20)
add_dependencies(NeneEngineColorTests NeneEngine)
set_target_properties(NeneEngineColorTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src")
add_test(NAME ColorPresentation COMMAND NeneEngineColorTests)
set_tests_properties(ColorPresentation PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" LABELS "gpu" SKIP_RETURN_CODE 77)
