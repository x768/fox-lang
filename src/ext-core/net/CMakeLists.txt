project(m_net)

add_library(m_net SHARED
  ${SRC_COMMON}
  m_net.c
  net_${TARGET_SYSTEM}.c
)
set_target_properties(m_net
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/io
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/io
  OUTPUT_NAME net
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

if("${TARGET_OS}" STREQUAL "win")
  target_link_libraries(m_net
    ws2_32
    iphlpapi
  )
endif()
