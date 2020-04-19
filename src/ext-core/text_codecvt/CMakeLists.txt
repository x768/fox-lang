project(m_text_charset)

add_library(m_text_codecvt SHARED
  ${SRC_COMMON}
  m_text_codecvt.c
  fconv.c
  fcharset.c
)
set_target_properties(m_text_codecvt
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/text
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/text
  OUTPUT_NAME codecvt
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
