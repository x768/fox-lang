project(m_image)

add_library(m_image SHARED
  ${SRC_COMMON}
  m_image.c
  imgutil.c
  quantize.c
)
set_target_properties(m_image
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME image
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

