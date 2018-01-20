project(m_windows)

add_library(m_windows SHARED
  ${SRC_COMMON}
  m_windows.c
  winole.c
  codepageio.c
)
set_target_properties(m_windows
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME windows
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
target_link_libraries(m_windows
  mpr
  ole32
  oleaut32
  uuid
)
