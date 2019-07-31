#ifndef PCG_H
#define PCG_H

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
#include <stdint.h>


/*
 * return a int value in the range [0, range)
 */
uint32_t randint(uint32_t range);

/*
 * return a double value in the range [0, 1)
 */
double randnumber();

void pcg32_srandom(uint64_t, uint64_t);
#endif

