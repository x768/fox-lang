project(m_lzma)

add_library(m_lzma SHARED
  ${SRC_COMMON}
  m_lzma.c
)
set_target_properties(m_lzma
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/archive
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/archive
  OUTPUT_NAME lzma
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
target_link_libraries(m_lzma
  lzma
)
