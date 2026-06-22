target_sources(NeneEngine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include/Graphics/Runtime/RenderTypes.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include/Graphics/Runtime/MeshRenderBinding.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include/Graphics/Runtime/RenderResize.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/MeshRenderBinding.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RenderResize.cpp"
)
