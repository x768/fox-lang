project(m_crypt)

add_library(m_crypt SHARED
  ${SRC_COMMON}
  m_crypt.c
)
set_target_properties(m_crypt
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME crypt
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
if("${TARGET_OS}" STREQUAL "win")
  target_link_libraries(m_crypt
    ssl
    crypto
    ws2_32
  )
else()
  target_link_libraries(m_crypt
    ssl
    crypto
  )
endif()

