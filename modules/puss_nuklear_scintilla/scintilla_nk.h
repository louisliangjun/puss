// scintilla_nk.h

#ifndef _INC_SCINTILLA_NK_H_
#define _INC_SCINTILLA_NK_H_

#include "puss_module_nuklear.h"

#include <Scintilla.h>
#include <SciLexer.h>

PUSS_DECLS_BEGIN

typedef struct _ScintillaNK	ScintillaNK;

ScintillaNK*	scintilla_nk_new(void);
void			scintilla_nk_free(ScintillaNK* sci);
void			scintilla_nk_update(ScintillaNK* sci, struct nk_context* ctx);
sptr_t			scintilla_nk_send(ScintillaNK* sci, unsigned int iMessage, uptr_t wParam, sptr_t lParam);

PUSS_DECLS_END

#endif//_INC_SCINTILLA_NK_H_

