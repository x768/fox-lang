project(m_math)

set(MPI_COMMON
  ../../common/mpi.c
  ../../common/mpi2.c
  ../../common/mplogic.c
)

add_library(m_math SHARED
  ${SRC_COMMON}
  ${MPI_COMMON}
  m_math.c
  fox_rand.c
  fox_vector.c
)
set_target_properties(m_math
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${DEST_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${DEST_DIR}
  OUTPUT_NAME math
  PREFIX ""
  SUFFIX .so
  LINK_FLAGS "${LINK_FLAGS}"
  LINK_FLAGS_DEBUG "${LINK_FLAGS_DEBUG}"
  LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE}"
)
target_link_libraries(m_math
  m
)
