project(fox_cl)
project(fox_cgi)
project(fox_gui)
#project(fox_dbg)

include_directories(../compat)
include_directories(../fox)


set(DEST_DIR ../../../../fox/bin)
file(GLOB LIST_SRC ../fox/*.c)

add_executable(fox_cl
  ../compat/c_${TARGET_OS}_cl.${TARGET_EXT}
  main_cl.c
)
add_executable(fox_cgi
  ../compat/c_${TARGET_OS}_cgi.${TARGET_EXT}
  main_cgi.c
)
add_executable(fox_gui
  ../compat/c_${TARGET_OS}_gui.${TARGET_EXT}
  main_gui.c
)
#add_executable(fox_dbg
#  ../compat/c_${TARGET_OS}_cl.${TARGET_EXT}
#  main_cl.c
#  interactive.c
#)

if("${TARGET_OS}" STREQUAL "win")
  set(LINK_FLAGS_GUI "-mwindows")
else()
  set(LINK_FLAGS_GUI "")
endif()

if("${TARGET_OS}" STREQUAL "osx")
  set(DEST_DIR_GUI "${DEST_DIR}/foxw.app/Contents/MacOS")
else()
  set(DEST_DIR_GUI "${DEST_DIR}")
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
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR_GUI}
  OUTPUT_NAME foxw
  LINK_FLAGS "${LINK_FLAGS} ${LINK_FLAGS_GUI}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
#set_target_properties(fox_dbg
#  PROPERTIES
#  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
#  OUTPUT_NAME foxd
#  COMPILE_DEFINITIONS "DEBUGGER"
#  LINK_FLAGS "${LINK_FLAGS}"
#  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
#  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
#)


if("${TARGET_OS}" STREQUAL "win")
  target_link_libraries(fox_cl
    foxrt
  )
  target_link_libraries(fox_cgi
    foxrt
  )
  target_link_libraries(fox_gui
    foxrt
  )
#  target_link_libraries(fox_dbg
#    foxrtd
#    readline
#  )
  add_definitions(-DPCRE_STATIC)
elseif("${TARGET_OS}" STREQUAL "osx")
  find_library(APPKIT_LIBRARY AppKit)
  target_link_libraries(fox_cl
    foxrt
  )
  target_link_libraries(fox_cgi
    foxrt
  )
  target_link_libraries(fox_gui
    foxrt
    ${APPKIT_LIBRARY}
  )
#  target_link_libraries(fox_dbg
#    foxrtd
#    readline
#  )
else()
  find_package(PkgConfig)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  include_directories(${GTK3_INCLUDE_DIRS})

  target_link_libraries(fox_cl
    foxrt
  )
  target_link_libraries(fox_cgi
    foxrt
  )
  target_link_libraries(fox_gui
    foxrt
    ${GTK3_LIBRARIES}
  )
#  target_link_libraries(fox_dbg
#    foxrtd
#    readline
#  )
endif()
