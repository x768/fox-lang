project(m_text_charset)

add_library(m_text_charset SHARED
  ${SRC_COMMON}
  m_text_charset.c
)
set_target_properties(m_text_charset
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/text
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/text
  OUTPUT_NAME charset
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

if("${TARGET_OS}" STREQUAL "win")
  target_link_libraries(m_text_charset
    iconv
  )
elseif("${TARGET_OS}" STREQUAL "osx")
  target_link_libraries(m_text_charset
    iconv
  )
else()
endif()
