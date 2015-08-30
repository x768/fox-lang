project(foxd)

include_directories(../compat)
include_directories(../fox)


set(DEST_DIR ../../../../bin)
file(GLOB LIST_SRC ../fox/*.c)


add_executable(foxd
  ${LIST_SRC}
  ${SRC_COMMON}
  ../compat/c_${TARGET_OS}_cl.${TARGET_EXT}
  ../compat/main_cl.c
  interactive.c
)

set_target_properties(foxd
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME foxd
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

add_definitions(-DDEBUGGER)

if("${TARGET_OS}" STREQUAL "win")
  set(LIBS
    iconv
    pcre
    readline
  )
  target_link_libraries(foxd ${LIBS})
  add_definitions(-DPCRE_STATIC)
elseif("${TARGET_OS}" STREQUAL "osx")
  set(LIBS
    dl
    iconv
    pcre
    readline
  )
  target_link_libraries(foxd ${LIBS} ${FOUNDATION_LIBRARY})
else()
  set(LIBS
    rt
    dl
    pcre
    readline
  )
  target_link_libraries(foxd ${LIBS})
endif()

