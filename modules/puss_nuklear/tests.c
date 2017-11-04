// tests.c

#include "puss_module_nuklear.h"

PussNuklearInterface* __puss_nuklear_iface__ = NULL;

#define UNUSED(a) (void)a
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a)/sizeof(a)[0])

int test1(lua_State* L) {
	struct nk_context * ctx = lua_touserdata(L, 1);
	if( !__puss_nuklear_iface__) {
		__puss_nuklear_iface__ = puss_interface_check(L, PussNuklearInterface);
	}

    /* GUI */
    if (nk_begin(ctx, "Demo", nk_rect(50, 50, 230, 250),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
        enum {EASY, HARD};
        static int op = EASY;
        static int property = 20;
     	static struct nk_color background;
        nk_layout_row_static(ctx, 30, 80, 1);
        if (nk_button_label(ctx, "button"))
            fprintf(stdout, "button pressed\n");

        nk_layout_row_dynamic(ctx, 30, 2);
        if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
        if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;

        nk_layout_row_dynamic(ctx, 25, 1);
        nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);

        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "background:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 25, 1);
        if (nk_combo_begin_color(ctx, background, nk_vec2(nk_widget_width(ctx),400))) {
            nk_layout_row_dynamic(ctx, 120, 1);
            background = nk_color_picker(ctx, background, NK_RGBA);
            nk_layout_row_dynamic(ctx, 25, 1);
            background.r = (nk_byte)nk_propertyi(ctx, "#R:", 0, background.r, 255, 1,1);
            background.g = (nk_byte)nk_propertyi(ctx, "#G:", 0, background.g, 255, 1,1);
            background.b = (nk_byte)nk_propertyi(ctx, "#B:", 0, background.b, 255, 1,1);
            background.a = (nk_byte)nk_propertyi(ctx, "#A:", 0, background.a, 255, 1,1);
            nk_combo_end(ctx);
        }
    }
    nk_end(ctx);
    return 0;
}

