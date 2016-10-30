project(foxrt)
project(foxrtd)

include_directories(../compat)
include_directories(./)


set(DEST_DIR ./)
file(GLOB LIST_SRC *.c)

add_library(foxrt
  ${LIST_SRC}
  ${SRC_COMMON}
)
add_library(foxrtd
  ${LIST_SRC}
  ${SRC_COMMON}
)

set_target_properties(foxrt
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME foxrt
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
set_target_properties(foxrtd
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME foxrtd
  COMPILE_DEFINITIONS "DEBUGGER"
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

if("${TARGET_OS}" STREQUAL "win")
  set(LIBS
    pcre
  )
  target_link_libraries(foxrt
    ${LIBS}
  )
  target_link_libraries(foxrtd
    ${LIBS}
  )
  add_definitions(-DPCRE_STATIC)
elseif("${TARGET_OS}" STREQUAL "osx")
  set(LIBS
    dl
    pcre
  )
  find_library(FOUNDATION_LIBRARY Foundation)
  target_link_libraries(foxrt
    ${LIBS}
    ${FOUNDATION_LIBRARY}
  )
  target_link_libraries(foxrtd
    ${LIBS}
    ${FOUNDATION_LIBRARY}
  )
else()
  set(LIBS
    rt
    dl
    pcre
  )

  target_link_libraries(foxrt
    ${LIBS}
  )
  target_link_libraries(foxrtd
    ${LIBS}
  )
endif()
