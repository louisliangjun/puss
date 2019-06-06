// scintilla_imgui.h

#ifndef _INC_SCINTILLA_IMGUI_H_
#define _INC_SCINTILLA_IMGUI_H_

#include <Scintilla.h>
#include <SciLexer.h>

class ScintillaIM;

typedef void (*ScintillaIMCallback)(ScintillaIM* sci, const SCNotification* ev, void* ud);

ScintillaIM*	scintilla_imgui_create();
void			scintilla_imgui_destroy(ScintillaIM* sci);
void			scintilla_imgui_update(ScintillaIM* sci, int draw, ScintillaIMCallback cb, void* ud);
sptr_t			scintilla_imgui_send(ScintillaIM* sci, unsigned int iMessage, uptr_t wParam, sptr_t lParam);
void			scintilla_imgui_dirty_scroll(ScintillaIM* sci);

struct ScintillaIMEnum {
	const char*	key;
	int			val;
};

ScintillaIMEnum*	scintilla_imgui_extern_enums();

// need implements for platform
// 
void*			__scintilla_imgui_os_window(void);

#endif//_INC_SCINTILLA_IMGUI_H_

