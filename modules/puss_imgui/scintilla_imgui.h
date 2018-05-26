// scintilla_imgui.h

#ifndef _INC_SCINTILLA_IMGUI_H_
#define _INC_SCINTILLA_IMGUI_H_

#include <Scintilla.h>
#include <SciLexer.h>

class ScintillaIM;

ScintillaIM*	scintilla_imgui_create();
void			scintilla_imgui_destroy(ScintillaIM* sci);
void			scintilla_imgui_update(ScintillaIM* sci);
sptr_t			scintilla_imgui_send(ScintillaIM* sci, unsigned int iMessage, uptr_t wParam, sptr_t lParam);

#endif//_INC_SCINTILLA_IMGUI_H_

