project(m_markdown)

add_library(m_markdown SHARED
  ${SRC_COMMON}
  m_markdown.c
  md_parse.c
)
set_target_properties(m_markdown
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME markdown
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

