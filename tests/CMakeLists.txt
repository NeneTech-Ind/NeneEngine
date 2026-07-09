add_executable(NeneEngineSpatialGridSmoke
    "${CMAKE_CURRENT_SOURCE_DIR}/SpatialGridSmoke.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/ECS/SpatialGrid.cpp"
)

target_include_directories(NeneEngineSpatialGridSmoke
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)

find_package(EnTT CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

target_link_libraries(NeneEngineSpatialGridSmoke
    PRIVATE
        EnTT::EnTT
        glm::glm
)

target_compile_features(NeneEngineSpatialGridSmoke PRIVATE cxx_std_20)

add_test(NAME NeneEngineSpatialGridSmoke COMMAND NeneEngineSpatialGridSmoke)
