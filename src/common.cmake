set(CMAKE_C_FLAGS "-Wall -Wwrite-strings -Wshadow -Wno-unused-result")
set(LINK_FLAGS "-Wall")

include_directories(../include)
include_directories(../share/include)
link_directories(../share/lib)

if(WIN32)
  set(TARGET_OS "win")
  set(TARGET_SYSTEM "win")
  set(TARGET_EXT "c")

  set(CMAKE_C_FLAGS_DEBUG "-Werror -g")
  set(LINK_FLAGS_DEBUG "")

  set(CMAKE_C_FLAGS_RELEASE "-O3 -DNO_DEBUG")
  set(LINK_FLAGS_RELEASE "-s -O3 -static-libgcc")
elseif(APPLE)
  set(TARGET_OS "osx")
  set(TARGET_SYSTEM "posix")
  set(TARGET_EXT "m")

  set(CMAKE_C_FLAGS_DEBUG "-Werror -g")
  set(LINK_FLAGS_DEBUG "")

  set(CMAKE_C_FLAGS_RELEASE "-O3 -DNO_DEBUG")
  set(LINK_FLAGS_RELEASE "-O3")
else()
  set(TARGET_OS "posix")
  set(TARGET_SYSTEM "posix")
  set(TARGET_EXT "c")

  set(CMAKE_C_FLAGS_DEBUG "-Werror -g")
  set(LINK_FLAGS_DEBUG "")

  set(CMAKE_C_FLAGS_RELEASE "-O3 -DNO_DEBUG")
  set(LINK_FLAGS_RELEASE "-s -O3")
endif()

set(SRC_COMMON
  ../../common/compat_${TARGET_OS}.c
  ../../common/strutil.c
)

if("${MODE}" STREQUAL "debug")
  set(CMAKE_BUILD_TYPE "Debug")
else()
  set(CMAKE_BUILD_TYPE "Release")
endif()

message("Build type: ${CMAKE_BUILD_TYPE}")
