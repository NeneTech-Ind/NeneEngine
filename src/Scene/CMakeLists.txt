target_sources(NeneEngine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/Scene/SceneConfig.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/Scene/SceneSerializer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/Scene/DemoScene.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/Scene/Instantiation/ModelInstanceConfig.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/Scene/Instantiation/ModelSpawner.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/Scene/Instantiation/ModelSpawnManifest.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/SceneConfig.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SceneSerializer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DemoScene.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Instantiation/ModelInstanceConfig.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Instantiation/ModelSpawner.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Instantiation/ModelSpawnManifest.cpp"
)
