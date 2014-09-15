#ifndef _H_MPITYPES_
#define _H_MPITYPES_

#include <stdint.h>

typedef char mp_sign;
typedef uint16_t mp_digit;
typedef uint32_t mp_word;
typedef unsigned int mp_size;
typedef int mp_err;

#define MP_DIGIT_BIT       (CHAR_BIT*sizeof(mp_digit))
#define MP_DIGIT_MAX       USHRT_MAX
#define MP_WORD_BIT        (CHAR_BIT*sizeof(mp_word))
#define MP_WORD_MAX        UINT_MAX

#define RADIX              (MP_DIGIT_MAX+1)

#define MP_DIGIT_SIZE      2
#define DIGIT_FMT          "%04X"

#endif /* end _H_MPITYPES_ */
