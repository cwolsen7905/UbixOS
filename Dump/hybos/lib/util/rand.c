/***
*rand.c - random number generator
*
*       Copyright (c) 1985-1997, Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines rand(), srand() - random number generator
*
*******************************************************************************/

static long holdrand = 1L;


/***
*void srand(seed) - seed the random number generator
*
*Purpose:
*       Seeds the random number generator with the int given.  Adapted from the
*       BASIC random number generator.
*
*Entry:
*       unsigned seed - seed to seed rand # generator with
*
*Exit:
*       None.
*
*Exceptions:
*
*******************************************************************************/

void srand(unsigned int seed)
{
	holdrand = (long)seed;
}


/***
*int rand() - returns a random number
*
*Purpose:
*       returns a pseudo-random number 0 through 32767.
*
*Entry:
*       None.
*
*Exit:
*       Returns a pseudo-random number 0 through 32767.
*
*Exceptions:
*
*******************************************************************************/

int rand(void)
{
	return (((holdrand = holdrand * 214013L + 2531011L) >> 16) & 0x7fff);
}
