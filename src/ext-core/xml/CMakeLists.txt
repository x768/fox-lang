project(m_xml)

add_library(m_xml SHARED
  ${SRC_COMMON}
  m_xml.c
  xml_parse.c
  xml_select.c
)
set_target_properties(m_xml
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/marshal
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/marshal
  OUTPUT_NAME xml
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

