project(m_text_util)

add_library(m_text_util SHARED
  ${SRC_COMMON}
  m_text_util.c
  hilight.c
)
set_target_properties(m_text_util
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/text
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/text
  OUTPUT_NAME util
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

