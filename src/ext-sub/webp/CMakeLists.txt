project(m_webp)

add_library(m_webp SHARED
  ${SRC_COMMON}
  m_webp.c
)
set_target_properties(m_webp
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/image
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/image
  OUTPUT_NAME webp
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
target_link_libraries(m_webp
  webp
)
