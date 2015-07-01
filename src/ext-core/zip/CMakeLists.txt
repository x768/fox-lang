project(m_zip)

add_library(m_zip SHARED
  ${SRC_COMMON}
  m_zip.c
  zipfile.c
)
set_target_properties(m_zip
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/archive
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/archive
  OUTPUT_NAME zip
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
target_link_libraries(m_zip
  z
)
