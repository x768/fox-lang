cmake_minimum_required(VERSION 3.0)

include(../common.cmake)
include(../library.cmake)

set(DEST_DIR ../../../../fox/import)

add_subdirectory(crypt)
add_subdirectory(image)
add_subdirectory(math)
add_subdirectory(media)
add_subdirectory(net)
add_subdirectory(process)
add_subdirectory(reflect)
add_subdirectory(sqlite)
add_subdirectory(text_codecvt)
add_subdirectory(text_unicode)
add_subdirectory(text_util)
add_subdirectory(xml)
add_subdirectory(zip)

