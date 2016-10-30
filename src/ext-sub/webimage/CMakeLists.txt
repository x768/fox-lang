project(m_webimage)

set(GIFLIB
  giflib/dgif_lib.c
  giflib/egif_lib.c
  giflib/gif_err.c
  giflib/gif_hash.c
  giflib/gifalloc.c
)
include_directories(giflib)

add_library(m_webimage SHARED
  ${SRC_COMMON}
  ${GIFLIB}
  m_webimage.c
)
set_target_properties(m_webimage
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/image
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/image
  OUTPUT_NAME webimage
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

if("${TARGET_OS}" STREQUAL "win")
  target_link_libraries(m_webimage
    jpeg
    png16
    z
  )
elseif("${TARGET_OS}" STREQUAL "osx")
  target_link_libraries(m_webimage
    jpeg
    png15
    z
  )
else()
  target_link_libraries(m_webimage
    jpeg
    png
  )
endif()
