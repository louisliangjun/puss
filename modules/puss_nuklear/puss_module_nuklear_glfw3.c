// puss_module_nuklear_glfw3.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define _PUSS_NUKLEAR_MODULE_IMPLEMENT
#include "puss_module_nuklear.h"

#define NK_IMPLEMENTATION
#include <nuklear/nuklear.h>

#define NK_GLFW_GL3_IMPLEMENTATION
#include "nuklear_glfw_gl3.h"

#include "nuklear_lua.inl"

#define MAX_VERTEX_BUFFER 1 * 1024 * 1024
#define MAX_ELEMENT_BUFFER 512 * 1024

static void error_callback(int e, const char *d) {
	fprintf(stderr, "[GFLW] error %d: %s\n", e, d);
}

static int application_run(lua_State* L) {
	const char* title = luaL_optstring(L, 2, "Puss nuklear demo");
	int width = luaL_optinteger(L, 3, 1024);
	int height = luaL_optinteger(L, 4, 768);
    GLFWwindow *win;
    struct nk_context *ctx;
    struct nk_font_atlas *atlas;
    GLfloat bg[4] = {0.2f, 0.2f, 0.2f, 1.0f};
	luaL_argcheck(L, lua_type(L, 1)==LUA_TFUNCTION, 1, "need function!");

    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        return luaL_error(L, "[GFLW] failed to init!\n");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    win = glfwCreateWindow(width, height, title, NULL, NULL);
    glfwMakeContextCurrent(win);
    glfwGetWindowSize(win, &width, &height);

	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glViewport(0, 0, width, height);

    ctx = nk_glfw3_init(win, NK_GLFW3_INSTALL_CALLBACKS);
    /* Load Fonts: if none of these are loaded a default font will be used  */
	{
		nk_glfw3_font_stash_begin(&atlas);
		/*struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "../../../extra_font/DroidSans.ttf", 14, 0);*/
		/*struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Roboto-Regular.ttf", 14, 0);*/
		/*struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../../../extra_font/kenvector_future_thin.ttf", 13, 0);*/
		/*struct nk_font *clean = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyClean.ttf", 12, 0);*/
		/*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyTiny.ttf", 10, 0);*/
		/*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Cousine-Regular.ttf", 13, 0);*/
		nk_glfw3_font_stash_end();
    }

    /* Load Cursor: if you uncomment cursor loading please hide the cursor */
    /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
    /*nk_style_set_font(ctx, &droid->handle);*/

    /* style.c */
    /*set_style(ctx, THEME_WHITE);*/
    /*set_style(ctx, THEME_RED);*/
    /*set_style(ctx, THEME_BLUE);*/
    /*set_style(ctx, THEME_DARK);*/

    while (!glfwWindowShouldClose(win)) {
    	glfwWaitEventsTimeout(0.1);
        /* Input */
        glfwPollEvents();
        nk_glfw3_new_frame();

		/* GUI */
		lua_pushvalue(L, 1);
		lua_pushlightuserdata(L, ctx);
		if( puss_pcall_stacktrace(L, 1, 0) ) {
			fprintf(stderr, "[Script] error: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}

        /* Draw */
        glfwGetWindowSize(win, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(bg[0], bg[1], bg[2], bg[3]);
        /* IMPORTANT: `nk_glfw_render` modifies some global OpenGL state
         * with blending, scissor, face culling, depth test and viewport and
         * defaults everything back into a default state.
         * Make sure to either a.) save and restore or b.) reset your own state after
         * rendering the UI. */
        nk_glfw3_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
        glfwSwapBuffers(win);
    }
    nk_glfw3_shutdown();
    glfwTerminate();
    return 0;
}

extern int test1(lua_State* L);

static int lua_push_nuklear_interface(lua_State* L) {
	lua_newtable(L);
	lua_pushcfunction(L, application_run);	lua_setfield(L, -2, "application_run");
	lua_pushcfunction(L, test1);			lua_setfield(L, -2, "test1");
	return 1;
}

PussInterface* __puss_iface__ = NULL;

static PussNuklearInterface puss_nuklear_iface;

PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, PussInterface* puss) {
	if( !__puss_iface__ ) {
		__puss_iface__= puss;
		#define __NUKLEARPROXY_SYMBOL(f)	puss_nuklear_iface.nuklear_proxy.f = f;
			#include "nuklear.symbols"
		#undef __NUKLEARPROXY_SYMBOL

		puss_push_const_table(L);
		#define __NUKLEARPROXY_ENUM(e)		lua_pushinteger(L, (lua_Integer)e);	lua_setfield(L, -2, #e);
			#include "nuklear.enums"
		#undef __NUKLEARPROXY_ENUM
		lua_pop(L, 1);
	}

	lua_open_nk_lib(L);

	puss_interface_register(L, "PussNuklearInterface", &puss_nuklear_iface);

	return lua_push_nuklear_interface(L);
}

