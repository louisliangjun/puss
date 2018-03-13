// puss_module_nuklear_scintilla.c

#include "scintilla_nk.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "puss_module_nuklear.h"

static void custom_draw_test(struct nk_context * ctx) {
	struct nk_command_buffer *out = &(ctx->current->buffer);
	struct nk_rect bounds;
	float rounding = 3.0f;
	struct nk_color background = { 0xff, 0xff, 0x00, 0x7f };
	nk_widget(&bounds, ctx);
	nk_fill_rect(out, bounds, rounding, background);
}

static void nuklear_scintilla_demo(ScintillaNK* sci, struct nk_context* ctx) {
    if (nk_begin(ctx, "Demo", __nk_recti(50, 50, 230, 250),
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
        if (nk_combo_begin_color(ctx, background, __nk_vec2i(nk_widget_width(ctx),400))) {
            nk_layout_row_dynamic(ctx, 120, 1);
            background = nk_color_picker(ctx, background, NK_RGBA);
            nk_layout_row_dynamic(ctx, 25, 1);
            background.r = (nk_byte)nk_propertyi(ctx, "#R:", 0, background.r, 255, 1,1);
            background.g = (nk_byte)nk_propertyi(ctx, "#G:", 0, background.g, 255, 1,1);
            background.b = (nk_byte)nk_propertyi(ctx, "#B:", 0, background.b, 255, 1,1);
            background.a = (nk_byte)nk_propertyi(ctx, "#A:", 0, background.a, 255, 1,1);
            nk_combo_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 20, 1);
        custom_draw_test(ctx);
    }
    nk_end(ctx);
}

struct _ScintillaNK {
	int todo;
};

extern "C" ScintillaNK* scintilla_nk_new(void) {
	ScintillaNK* sci = new ScintillaNK;
	sci->todo = 0;
	return sci;
}

extern "C" void scintilla_nk_free(ScintillaNK* sci) {
}

extern "C" void scintilla_nk_update(ScintillaNK* sci, struct nk_context* ctx) {
	// TODO : input events & draw

	nuklear_scintilla_demo(sci, ctx);
}

extern "C" sptr_t scintilla_nk_send(ScintillaNK* sci, unsigned int iMessage, uptr_t wParam, sptr_t lParam) {
	return 0;
}

