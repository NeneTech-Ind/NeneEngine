target_sources(NeneEngine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include/Graphics/Loaders/MeshLoader.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include/Graphics/Loaders/ShaderLoader.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include/Graphics/Loaders/TextureLoader.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/MeshLoader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ShaderLoader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TextureLoader.cpp"
)
