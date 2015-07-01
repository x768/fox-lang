project(m_textedit)

add_library(m_textedit SHARED
  ${SRC_COMMON}
  m_textedit.c
  textedit_${TARGET_OS}.${TARGET_EXT}
)
set_target_properties(m_textedit
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/foxtk
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/foxtk
  OUTPUT_NAME textedit
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

if("${TARGET_OS}" STREQUAL "osx")
  find_library(FOUNDATION_LIBRARY Foundation)
  find_library(APPKIT_LIBRARY AppKit)
  target_link_libraries(m_textedit ${LIBS} ${FOUNDATION_LIBRARY} ${APPKIT_LIBRARY})
elseif("${TARGET_OS}" STREQUAL "posix")
  find_package(PkgConfig)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  include_directories(${GTK3_INCLUDE_DIRS})
  target_link_libraries(m_textedit ${GTK3_LIBRARIES})
endif()

