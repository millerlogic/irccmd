/*
  Copyright 2011-2014 Christopher E. Miller
  License: Boost Software License - Version 1.0
  http://www.boost.org/users/license.html
*/

#ifndef _FRANDOM_H_948903
#define _FRANDOM_H_948903


#ifdef __cplusplus
extern "C" {
#endif


#ifndef LL_INLINE
	#ifdef _MSC_VER
		#define LL_INLINE __inline
	#else
		#define LL_INLINE inline
	#endif
#endif


typedef struct FRandom_
{
	long _rh, _rl;
}FRandom;


#define FRANDOM_MAX 0x7FFFFFFF


static LL_INLINE void frandom_init(FRandom *frand, long seed)
{
	frand->_rh = 1594606355;
	frand->_rl = seed ^ 1594606355;
}


static LL_INLINE long frandom_raw(FRandom *frand)
{
	frand->_rh = (long)(((unsigned long)frand->_rh << 16)
		+ ((unsigned long)frand->_rh >> 16));
	frand->_rh += frand->_rl;
	frand->_rl += frand->_rh;
	return frand->_rh;
}


/* Return next random integer between 0 and FRANDOM_MAX. */
static LL_INLINE long frandom(FRandom *frand)
{
	return frandom_raw(frand) & FRANDOM_MAX;
}


/* lower is inclusive, upper is exclusive. */
static LL_INLINE long frandom_bounds(FRandom *frand, long lower, long upper)
{
	return frandom(frand) % (upper - lower) + lower;
}


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* ifndef _FRANDOM_H_948903 */
