#include "vector_tools.h"

#define DECLARE_HALVES(side, iw, xw) const __m##xw##i side##_HALVES_##iw##_##xw = DUPE_UINT_##xw( side##_HALVES_##iw##_64);
#define DECLARE_MANY_HALVES(side, xw) \
    DECLARE_HALVES(side, 1, xw) \
    DECLARE_HALVES(side, 2, xw) \
    DECLARE_HALVES(side, 4, xw) \
    DECLARE_HALVES(side, 8, xw) \
    DECLARE_HALVES(side, 16, xw) \
    DECLARE_HALVES(side, 32, xw)

DECLARE_MANY_HALVES(LO, 128);
DECLARE_MANY_HALVES(HI, 128);
DECLARE_MANY_HALVES(LO, 256);
DECLARE_MANY_HALVES(HI, 256);

#undef DECLARE_HALVES
#undef DECLARE_MANY_HALVES

