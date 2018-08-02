// puss_module_imgui.h

#ifndef _INC_PUSS_MODULE_IMGUI_H_
#define _INC_PUSS_MODULE_IMGUI_H_

#include "puss_module.h"

#define IMGUI_MT_NAME	"ImguiEnv"

enum PussImGuiKeyType
	{ PUSS_IMGUI_BASIC_KEY_LAST = 255
#define _PUSS_IMGUI_KEY_REG(key)	, PUSS_IMGUI_KEY_ ## key
	#include "puss_module_imgui_keys.inl"
#undef _PUSS_IMGUI_KEY_REG
	, PUSS_IMGUI_TOTAL_KEY_LAST
	};

#endif//_INC_PUSS_MODULE_IMGUI_H_

