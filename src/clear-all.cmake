function(remove_glob FILENAME)
  file(GLOB FILES "${FILENAME}")
  foreach(FILE ${FILES})
    message("Removing.. ${FILE}")
    file(REMOVE ${FILE})
  endforeach()
endfunction()

remove_glob(../bin/fox*)
remove_glob(../import/*.so)
remove_glob(../import/**/*.so)

set(DIRS
  vm/build
  ext-core/build
  ext-sub/build
  ext-tk/build
)
file(REMOVE_RECURSE ${DIRS})
file(MAKE_DIRECTORY ${DIRS})

