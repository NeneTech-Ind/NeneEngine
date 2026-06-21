target_sources(NeneEngine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/GameStates/IGameState.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/GameStates/MenuState.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/GameStates/PauseState.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/GameStates/PlayState.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/MenuState.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PauseState.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PlayState.cpp"
)
