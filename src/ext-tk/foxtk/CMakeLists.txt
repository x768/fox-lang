project(m_foxtk)

include_directories(./)

add_library(m_foxtk SHARED
  ${SRC_COMMON}
  m_gui.c
  ${TARGET_OS}/m_gui_sub.${TARGET_EXT}
  ${TARGET_OS}/clipboard.${TARGET_EXT}
  ${TARGET_OS}/dialog.${TARGET_EXT}
  ${TARGET_OS}/exio.${TARGET_EXT}
  ${TARGET_OS}/form.${TARGET_EXT}
  ${TARGET_OS}/menu.${TARGET_EXT}
  ${TARGET_OS}/monitor.${TARGET_EXT}
)
set_target_properties(m_foxtk
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME foxtk
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

if("${TARGET_OS}" STREQUAL "osx")
  find_library(FOUNDATION_LIBRARY Foundation)
  find_library(APPKIT_LIBRARY AppKit)
  target_link_libraries(m_foxtk
    ${LIBS}
    ${FOUNDATION_LIBRARY}
    ${APPKIT_LIBRARY}
  )
elseif("${TARGET_OS}" STREQUAL "posix")
  find_package(PkgConfig)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  include_directories(${GTK3_INCLUDE_DIRS})
  target_link_libraries(m_foxtk
    ${GTK3_LIBRARIES}
  )
endif()

