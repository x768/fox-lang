project(m_text_unicode)

add_library(m_text_unicode SHARED
  ${SRC_COMMON}
  m_unicode.c
)
set_target_properties(m_text_unicode
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/text
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/text
  OUTPUT_NAME unicode
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

