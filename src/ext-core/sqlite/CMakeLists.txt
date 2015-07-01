project(m_sqlite)

add_library(m_sqlite SHARED
  ${SRC_COMMON}
  m_sqlite.c
)
set_target_properties(m_sqlite
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/db
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/db
  OUTPUT_NAME sqlite3
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
target_link_libraries(m_sqlite
  sqlite3
)
