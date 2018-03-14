// scintilla_nk.h

#ifndef _INC_SCINTILLA_NK_H_
#define _INC_SCINTILLA_NK_H_

#include <Scintilla.h>
#include <SciLexer.h>

#ifdef __cplusplus
	class ScintillaNK;
#else
	typedef struct _ScintillaNK	ScintillaNK;
#endif

#ifdef __cplusplus
extern "C" {
#endif

ScintillaNK*	scintilla_nk_new(void);
void			scintilla_nk_free(ScintillaNK* sci);
void			scintilla_nk_update(ScintillaNK* sci, struct nk_context* ctx);
sptr_t			scintilla_nk_send(ScintillaNK* sci, unsigned int iMessage, uptr_t wParam, sptr_t lParam);

#ifdef __cplusplus
}
#endif

#endif//_INC_SCINTILLA_NK_H_

