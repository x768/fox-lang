project(m_rawimage)

add_library(m_rawimage SHARED
  ${SRC_COMMON}
  m_image_raw.c
  pbm.c
  windows_bmp.c
)
set_target_properties(m_rawimage
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/image
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/image
  OUTPUT_NAME raw
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
