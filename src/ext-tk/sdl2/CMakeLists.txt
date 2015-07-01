project(m_sdl2)

if("${TARGET_OS}" STREQUAL "osx")
  execute_process(
    COMMAND sdl2-config --cflags
    OUTPUT_VARIABLE SDL2_CFLAGS
  )
  execute_process(
    COMMAND sdl2-config --libs
    OUTPUT_VARIABLE SDL2_LIBS
  )
  string(REPLACE "\n" "" SDL2_CFLAGS "${SDL2_CFLAGS}")
  string(REPLACE "\n" "" SDL2_LIBS "${SDL2_LIBS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SDL2_CFLAGS}")
  set(LINK_FLAGS "${LINK_FLAGS} ${SDL2_LIBS}")
elseif("${TARGET_OS}" STREQUAL "win")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/usr/include/SDL2")
  set(LINK_FLAGS "${LINK_FLAGS} -L/usr/lib")
endif()

add_library(m_sdl2 SHARED
  ${SRC_COMMON}
  m_sdl.c
  sdl_gl.c
)
set_target_properties(m_sdl2
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME sdl2
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

if("${TARGET_OS}" STREQUAL "posix")
  find_package(PkgConfig)
  pkg_check_modules(SDL2 REQUIRED sdl2)
  include_directories(${SDL2_INCLUDE_DIRS})
  target_link_libraries(m_sdl2 ${SDL2_LIBRARIES})
elseif("${TARGET_OS}" STREQUAL "win")
  target_link_libraries(m_sdl2
    SDL2main
    SDL2
  )
endif()
