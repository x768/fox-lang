project(m_json)

set(MPI_COMMON
  ../../common/mpi.c
  ../../common/mpi2.c
  ../../common/mplogic.c
)

add_library(m_json SHARED
  ${SRC_COMMON}
  ${MPI_COMMON}
  m_json.c
)
set_target_properties(m_json
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}/marshal
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}/marshal
  OUTPUT_NAME json
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)

