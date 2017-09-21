project(m_image_text)

add_library(m_image_text SHARED
  ${SRC_COMMON}
  m_image_text.c
  ft.c
)
set_target_properties(m_image_text
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/image
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/image
  OUTPUT_NAME text
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

if("${TARGET_OS}" STREQUAL "win")
  target_link_libraries(m_image_text
    freetype
    z
  )
  include_directories(../../share/include/freetype2)
elseif("${TARGET_OS}" STREQUAL "osx")
  target_link_libraries(m_image_text
    freetype
    png15
    z
    bz2
  )
else()
  find_package(PkgConfig)
  pkg_check_modules(FREETYPE REQUIRED freetype2)
  include_directories(${FREETYPE_INCLUDE_DIRS})
  target_link_libraries(m_image_text
    ${FREETYPE_LIBRARIES}
  )
endif()

