project(m_util_diff)

add_library(m_util_diff SHARED
  ${SRC_COMMON}
  m_diff.c
)
set_target_properties(m_util_diff
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/util
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/util
  OUTPUT_NAME diff
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

