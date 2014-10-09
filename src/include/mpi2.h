#ifndef MPI2_H_
#define MPI2_H_

#include "mpi.h"
#include "mplogic.h"
#include <stdint.h>


enum {
	SCALE_MAX = 32767,
	SCALE_MIN = -32767,
};


uint32_t mp2_get_hash(mp_int *mp);
mp_err mp2_set_int64(mp_int *m, int64_t val);
mp_err mp2_set_double(mp_int *m, double val);
long mp2_tolong(mp_int *mp);
mp_err mp2_set_uint64(mp_int *mp, uint64_t z);
mp_err mp2_toint64(mp_int *mp, int64_t *ret);
double mp2_todouble(mp_int *mp);

mp_err mp2_read_rational(mp_int *md, const char *src, const char **end);
mp_err mp2_double_to_rational(mp_int *md, double d);
mp_err mp2_fix_rational(mp_int *mp1, mp_int *mp2);

#endif /* MPI2_H_ */
