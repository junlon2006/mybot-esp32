set(MYBOT_SUPPORTED_BOARDS
    "m5stack-core-s3"
    "zhengchen-1.54tft-ml307"
)
set(_MYBOT_BOARDS_ROOT "${CMAKE_CURRENT_LIST_DIR}")

function(mybot_resolve_board board)
    if(board STREQUAL "m5stack-core-s3")
        set(profile "${_MYBOT_BOARDS_ROOT}/m5stack-core-s3/board.cmake")
    elseif(board STREQUAL "zhengchen-1.54tft-ml307")
        set(profile "${_MYBOT_BOARDS_ROOT}/zhengchen-1.54tft-ml307/board.cmake")
    else()
        message(FATAL_ERROR "Unsupported MYBOT_BOARD: ${board}")
    endif()
    set(MYBOT_BOARD_PROFILE "${profile}" PARENT_SCOPE)
endfunction()
