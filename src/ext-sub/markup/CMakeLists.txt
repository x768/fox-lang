project(m_markup)

add_library(m_markup SHARED
  ${SRC_COMMON}
  m_markup.c
  md_parse.c
  md_make.c
  gm_parse.c
  gm_make.c
  hilight.c
)
set_target_properties(m_markup
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME markup
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
