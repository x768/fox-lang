project(m_macos)

add_library(m_macos SHARED
  ${SRC_COMMON}
  m_macos.c
  apple.m
)
set_target_properties(m_macos
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME macos
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
if("${TARGET_OS}" STREQUAL "osx")
  find_library(FOUNDATION_LIBRARY Foundation)
  target_link_libraries(m_macos
    ${LIBS}
    ${FOUNDATION_LIBRARY}
  )
endif()
