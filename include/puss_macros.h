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

#endif//_INC_PUSS_MACROS_H_

