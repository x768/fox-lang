project(m_osx)

add_library(m_osx SHARED
  ${SRC_COMMON}
  m_osx.c
  apple_event.m
)
set_target_properties(m_osx
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME osx
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
if("${TARGET_OS}" STREQUAL "osx")
  find_library(FOUNDATION_LIBRARY Foundation)
  target_link_libraries(m_osx
    ${LIBS}
    ${FOUNDATION_LIBRARY}
  )
endif()
