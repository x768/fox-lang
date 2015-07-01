cmake_minimum_required(VERSION 3.0)

include(../common.cmake)
include(../library.cmake)

set(DEST_DIR ../../../../import)

add_subdirectory(foxtk)
add_subdirectory(textedit)
add_subdirectory(sdl2)

