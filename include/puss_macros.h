// puss_macros.h

#ifndef _INC_PUSS_MACROS_H_
#define _INC_PUSS_MACROS_H_

#ifdef __cplusplus
	#define PUSS_EXTERN_C       extern "C"
	#define PUSS_DECLS_BEGIN    PUSS_EXTERN_C {
	#define PUSS_DECLS_END      }
#else
	#define PUSS_EXTERN_C	    extern
	#define PUSS_DECLS_BEGIN
	#define PUSS_DECLS_END
#endif

#ifdef _WIN32
	#define PUSS_PLUGIN_EXPORT	PUSS_EXTERN_C __declspec(dllexport)
#else
	#define PUSS_PLUGIN_EXPORT	PUSS_EXTERN_C __attribute__ ((visibility("default"))) 
#endif

// C inline
// 
#ifndef  __cplusplus
	#ifdef _MSC_VER
		#ifdef inline
			#undef inline
		#endif
		#define inline __forceinline
	#endif
#endif

// member offset
// 
#ifdef offsetof
	#define puss_offset_of	offsetof
#else
	#define puss_offset_of(type, member)		( (intptr_t)(  &((type *)0)->member  ) )
#endif

#define	puss_member_offset(ptr, type, member)	( (type *)(  (char *)(ptr) - puss_offset_of(type, member)  ) )

#define	puss_memalign_size(size)				( ((size) + (sizeof(void*)-1)) & (~(sizeof(void*)-1)) )

// ignore msvc warnings
// 
#ifdef _MSC_VER
	#pragma warning(disable : 4200)		// nonstandard extension used : zero-sized array in struct/union
	#pragma warning(disable: 4996)		// VC++ depart functions warning
#endif

#endif//_INC_PUSS_MACROS_H_

