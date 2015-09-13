project(m_image_formula)

add_library(m_image_formula SHARED
  ${SRC_COMMON}
  m_formula.c
  mimetex.c
  texfonts.c
)
set_target_properties(m_image_formula
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/image
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/image
  OUTPUT_NAME formula
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
target_link_libraries(m_image_formula
  m
)
