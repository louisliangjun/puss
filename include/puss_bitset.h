// bitset.h

#ifndef	_PUSS_INC_BITSET_H_
#define	_PUSS_INC_BITSET_H_

#include <assert.h>

// bitset, not suit for iter

#if defined(__CHAR_BIT__)
	#define PUSS_BITSET_CHAR_BITS		__CHAR_BIT__
#else
	#define PUSS_BITSET_CHAR_BITS		8
#endif

typedef unsigned	PussBitSet;

#define PUSS_BITSET_WORD_BITS			(sizeof(PussBitSet) * PUSS_BITSET_CHAR_BITS)
#define PUSS_BITSET_REQUIRE(bits_num)	((bits_num + PUSS_BITSET_WORD_BITS - 1) / PUSS_BITSET_WORD_BITS)	// bits_num MUST > 0
#define PUSS_BITSET_WORD_OF(pos)		((pos) / PUSS_BITSET_WORD_BITS)
#define PUSS_BITSET_BIT_OF(pos)			(1U << ((pos) % PUSS_BITSET_WORD_BITS))

#define PUSS_BITSET_SET(bits, pos)		(bits)[PUSS_BITSET_WORD_OF(pos)] |= PUSS_BITSET_BIT_OF(pos)
#define PUSS_BITSET_RESET(bits, pos)	(bits)[PUSS_BITSET_WORD_OF(pos)] &= (~PUSS_BITSET_BIT_OF(pos))
#define PUSS_BITSET_TEST(bits, pos)		((bits)[PUSS_BITSET_WORD_OF(pos)] & PUSS_BITSET_BIT_OF(pos))

// bitset array list, startswith 1
// 	 inttype	num;		// num of bits
// 	 inttype	arr[1+num];	// arr[0] used as head, arr[1] used as tail

#define _PUSS_BITSETLIST_INT_TYPE(pfx)	pfx ## BITSETLIST_TYPE
#define _PUSS_BITSETLIST_EMPTY(pfx)		((_PUSS_BITSETLIST_INT_TYPE(pfx))(~0))	// (inttype)(~0) = 0xFFFF
#define _PUSS_BITSETLIST_MASK_POS(pfx)	((_PUSS_BITSETLIST_EMPTY(pfx)-1) >> 1)	// (0xFFFE >> 1) = 0x7FFF
#define _PUSS_BITSETLIST_MASK_RST(pfx)	(_PUSS_BITSETLIST_MASK_POS(pfx) + 1)	// (0x7FFF + 1)  = 0x8000

#define _PUSS_BITSETLIST_INIT(pfx, num, arr) do { \
			_PUSS_BITSETLIST_INT_TYPE(pfx) pos; \
			assert( (num > 0) && (num < _PUSS_BITSETLIST_MASK_POS(pfx)) ); \
			arr[0] = _PUSS_BITSETLIST_EMPTY(pfx); \
			for( pos=1; pos<=num; ++pos ) \
				arr[pos] = _PUSS_BITSETLIST_EMPTY(pfx); \
		} while(0)

#define _PUSS_BITSETLIST_TEST(pfx, num, arr, pos) \
			(assert(num < _PUSS_BITSETLIST_MASK_POS(pfx)), assert(pos > 0), assert(pos <= num), (void)num, !(arr[pos] & _PUSS_BITSETLIST_MASK_RST(pfx)))

#define _PUSS_BITSETLIST_SET(pfx, num, arr, pos) do { \
			assert(num < _PUSS_BITSETLIST_MASK_POS(pfx)); assert(pos > 0); assert(pos <= num); (void)num; \
			if( arr[pos] == _PUSS_BITSETLIST_EMPTY(pfx) ) { \
				arr[pos] = (arr[0]==_PUSS_BITSETLIST_EMPTY(pfx)) ? 0 : arr[0] ; \
				arr[0] = pos; \
			} else if( arr[pos] & _PUSS_BITSETLIST_MASK_RST(pfx) ) { \
				arr[pos] &= _PUSS_BITSETLIST_MASK_POS(pfx); \
			} \
		} while(0)

#define _PUSS_BITSETLIST_RESET(pfx, num, arr, pos) do { \
			assert(num < _PUSS_BITSETLIST_MASK_POS(pfx)); assert(pos > 0); assert(pos <= num); (void)num; \
			arr[pos] |= _PUSS_BITSETLIST_MASK_RST(pfx); \
		} while(0) \

#define _PUSS_BITSETLIST_CLEAR(pfx, num, arr) do { \
			_PUSS_BITSETLIST_INT_TYPE(pfx) pos; (void)num; \
			assert(num < _PUSS_BITSETLIST_MASK_POS(pfx)); \
			while( (pos = arr[0]) != _PUSS_BITSETLIST_EMPTY(pfx) ) { \
				pos &= _PUSS_BITSETLIST_MASK_POS(pfx); \
				if( pos ) { assert(pos <= num); arr[0] = arr[pos]; } \
				arr[pos] = _PUSS_BITSETLIST_EMPTY(pfx); \
			} \
		} while(0) \

#define _PUSS_BITSETLIST_ITER(pfx, num, arr, pos) \
			for( pos=(assert(num < _PUSS_BITSETLIST_MASK_POS(pfx)), arr[0]); pos!=_PUSS_BITSETLIST_EMPTY(pfx) ; pos=arr[pos] ) \
				if( (pos &= _PUSS_BITSETLIST_MASK_POS(pfx)) == 0 ) break; else

#define _PUSS_BITSETLIST_MOVE(pfx, dst, len, arr) do { \
			_PUSS_BITSETLIST_INT_TYPE(pfx) pos, tmp; len = 0; \
			while( (pos = arr[0]) != _PUSS_BITSETLIST_EMPTY(pfx) ) { \
				tmp = pos; pos &= _PUSS_BITSETLIST_MASK_POS(pfx); \
				if( pos ) { arr[0] = arr[pos]; dst[len] = tmp; ++len; } \
				arr[pos] = _PUSS_BITSETLIST_EMPTY(pfx); \
			} \
		} while(0)

// tiny(num_of_bits<=126) bitset array list, startswith 1
// 	 uint8_t	num;		// num of bits
// 	 uint8_t	arr[1+num];	// arr[0] used as start

#define PUSS_TINY_BITSETLIST_TYPE      uint8_t
#define PUSS_TINY_BITSETLIST_EMPTY     _PUSS_BITSETLIST_EMPTY(PUSS_TINY_)
#define PUSS_TINY_BITSETLIST_MASK_POS  _PUSS_BITSETLIST_MASK_POS(PUSS_TINY_)
#define PUSS_TINY_BITSETLIST_MASK_RST  _PUSS_BITSETLIST_MASK_RST(PUSS_TINY_)
#define PUSS_TINY_BITSETLIST_INIT(num, arr)       _PUSS_BITSETLIST_INIT(PUSS_TINY_, num, arr)
#define PUSS_TINY_BITSETLIST_TEST(num, arr, pos)  _PUSS_BITSETLIST_TEST(PUSS_TINY_, num, arr, pos)
#define PUSS_TINY_BITSETLIST_SET(num, arr, pos)   _PUSS_BITSETLIST_SET(PUSS_TINY_, num, arr, pos)
#define PUSS_TINY_BITSETLIST_RESET(num, arr, pos) _PUSS_BITSETLIST_RESET(PUSS_TINY_, num, arr, pos)
#define PUSS_TINY_BITSETLIST_CLEAR(num, arr)      _PUSS_BITSETLIST_CLEAR(PUSS_TINY_, num, arr)
#define PUSS_TINY_BITSETLIST_ITER(num, arr, pos)  _PUSS_BITSETLIST_ITER(PUSS_TINY_, num, arr, pos)
#define PUSS_TINY_BITSETLIST_MOVE(dst, len, arr)  _PUSS_BITSETLIST_MOVE(PUSS_TINY_, dst, len, arr)

// (num_of_bits<=32766) bitset array list, startswith 1
// 	 uint16_t	num;		// num of bits
// 	 uint16_t	arr[1+num];	// arr[0] used as start

#define PUSS_BITSETLIST_TYPE      uint16_t
#define PUSS_BITSETLIST_EMPTY     _PUSS_BITSETLIST_EMPTY(PUSS_)
#define PUSS_BITSETLIST_MASK_POS  _PUSS_BITSETLIST_MASK_POS(PUSS_)
#define PUSS_BITSETLIST_MASK_RST  _PUSS_BITSETLIST_MASK_RST(PUSS_)
#define PUSS_BITSETLIST_INIT(num, arr)       _PUSS_BITSETLIST_INIT(PUSS_, num, arr)
#define PUSS_BITSETLIST_TEST(num, arr, pos)  _PUSS_BITSETLIST_TEST(PUSS_, num, arr, pos)
#define PUSS_BITSETLIST_SET(num, arr, pos)   _PUSS_BITSETLIST_SET(PUSS_, num, arr, pos)
#define PUSS_BITSETLIST_RESET(num, arr, pos) _PUSS_BITSETLIST_RESET(PUSS_, num, arr, pos)
#define PUSS_BITSETLIST_CLEAR(num, arr)      _PUSS_BITSETLIST_CLEAR(PUSS_, num, arr)
#define PUSS_BITSETLIST_ITER(num, arr, pos)  _PUSS_BITSETLIST_ITER(PUSS_, num, arr, pos)
#define PUSS_BITSETLIST_MOVE(dst, len, arr)  _PUSS_BITSETLIST_MOVE(PUSS_, dst, len, arr)

#endif//_PUSS_INC_BITSET_H_

