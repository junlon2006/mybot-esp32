set(MYBOT_SUPPORTED_BOARDS
    "esp32-s3-touch-amoled-1.75"
    "m5stack-core-s3"
    "m5stack-stick-s3"
    "respeaker-flex-xvf3800-circular4-xiao"
    "sensecap-watcher"
    "zhengchen-1.54tft-ml307"
    "zhengchen-1.54tft-wifi"
)
set(_MYBOT_BOARDS_ROOT "${CMAKE_CURRENT_LIST_DIR}")

function(mybot_resolve_board board)
    if(board STREQUAL "esp32-s3-touch-amoled-1.75")
        set(profile "${_MYBOT_BOARDS_ROOT}/esp32-s3-touch-amoled-1.75/board.cmake")
    elseif(board STREQUAL "m5stack-core-s3")
        set(profile "${_MYBOT_BOARDS_ROOT}/m5stack-core-s3/board.cmake")
    elseif(board STREQUAL "m5stack-stick-s3")
        set(profile "${_MYBOT_BOARDS_ROOT}/m5stack-stick-s3/board.cmake")
    elseif(board STREQUAL "respeaker-flex-xvf3800-circular4-xiao")
        set(profile
            "${_MYBOT_BOARDS_ROOT}/respeaker-flex-xvf3800-circular4-xiao/board.cmake")
    elseif(board STREQUAL "sensecap-watcher")
        set(profile "${_MYBOT_BOARDS_ROOT}/sensecap-watcher/board.cmake")
    elseif(board STREQUAL "zhengchen-1.54tft-ml307")
        set(profile "${_MYBOT_BOARDS_ROOT}/zhengchen-1.54tft-ml307/board.cmake")
    elseif(board STREQUAL "zhengchen-1.54tft-wifi")
        set(profile "${_MYBOT_BOARDS_ROOT}/zhengchen-1.54tft-wifi/board.cmake")
    else()
        message(FATAL_ERROR "Unsupported MYBOT_BOARD: ${board}")
    endif()
    set(MYBOT_BOARD_PROFILE "${profile}" PARENT_SCOPE)
endfunction()
