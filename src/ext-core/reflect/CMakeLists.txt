project(m_reflect)

add_library(m_reflect SHARED
  ${SRC_COMMON}
  m_reflect.c
)
set_target_properties(m_reflect
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/lang
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/lang
  OUTPUT_NAME reflect
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

