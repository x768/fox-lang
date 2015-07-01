project(fox_cl)
project(fox_cgi)
project(fox_gui)

include_directories(../compat)
include_directories(./)


set(DEST_DIR ../../../../bin)
file(GLOB LIST_SRC *.c)

set(MPI_COMMON
  ../../common/mpi.c
  ../../common/mpi2.c
  ../../common/mplogic.c
)

add_executable(fox_cl
  ${LIST_SRC}
  ${SRC_COMMON}
  ${MPI_COMMON}
  ../compat/c_${TARGET_OS}_cl.${TARGET_EXT}
  ../compat/main_cl.c
)
add_executable(fox_cgi
  ${LIST_SRC}
  ${SRC_COMMON}
  ${MPI_COMMON}
  ../compat/c_${TARGET_OS}_cgi.${TARGET_EXT}
  ../compat/main_cgi.c
)
add_executable(fox_gui
  ${LIST_SRC}
  ${SRC_COMMON}
  ${MPI_COMMON}
  ../compat/c_${TARGET_OS}_gui.${TARGET_EXT}
  ../compat/main_gui.c
)

if("${TARGET_OS}" STREQUAL "win")
  set(LINK_FLAGS_GUI "-mwindows")
else()
  set(LINK_FLAGS_GUI "")
endif()

set_target_properties(fox_cl
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME fox
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
set_target_properties(fox_cgi
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME fox
  SUFFIX .cgi
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
set_target_properties(fox_gui
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME foxw
  LINK_FLAGS "${LINK_FLAGS} ${LINK_FLAGS_GUI}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)


if("${TARGET_OS}" STREQUAL "win")
  set(LIBS
    iconv
    pcre
  )
  target_link_libraries(fox_cl
    ${LIBS}
  )
  target_link_libraries(fox_cgi
    ${LIBS}
  )
  target_link_libraries(fox_gui
    ${LIBS}
  )
  add_definitions(-DPCRE_STATIC)
elseif("${TARGET_OS}" STREQUAL "osx")
  set(LIBS
    dl
    iconv
    pcre
  )
  find_library(FOUNDATION_LIBRARY Foundation)
  find_library(APPKIT_LIBRARY AppKit)
  target_link_libraries(fox_cl
    ${LIBS}
    ${FOUNDATION_LIBRARY}
  )
  target_link_libraries(fox_cgi
    ${LIBS}
    ${FOUNDATION_LIBRARY}
  )
  target_link_libraries(fox_gui
    ${LIBS}
    ${FOUNDATION_LIBRARY}
    ${APPKIT_LIBRARY}
  )
else()
  set(LIBS
    rt
    dl
    pcre
  )
  find_package(PkgConfig)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  include_directories(${GTK3_INCLUDE_DIRS})

  target_link_libraries(fox_cl
    ${LIBS}
  )
  target_link_libraries(fox_cgi
    ${LIBS}
  )
  target_link_libraries(fox_gui
    ${LIBS}
    ${GTK3_LIBRARIES}
  )
endif()
