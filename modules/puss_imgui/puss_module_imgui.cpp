// puss_module_imgui.cpp

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ImGui GLFW binding with OpenGL3 + shaders
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)
// (GL3W is a helper library to access OpenGL functions since there is no standard header to access modern OpenGL functions easily. Alternatives are GLEW, Glad, etc.)

// Implemented features:
//  [X] User texture binding. Cast 'GLuint' OpenGL texture identifier as void*/ImTextureID. Read the FAQ about ImTextureID in imgui.cpp.
//  [X] Gamepad navigation mapping. Enable with 'io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad'.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2018-03-20: Misc: Setup io.BackendFlags ImGuiBackendFlags_HasMouseCursors and ImGuiBackendFlags_HasSetMousePos flags + honor ImGuiConfigFlags_NoSetMouseCursor flag.
//  2018-03-06: OpenGL: Added const char* glsl_version parameter to ImGui_ImplGlfwGL3_Init() so user can override the GLSL version e.g. "#version 150".
//  2018-02-23: OpenGL: Create the VAO in the render function so the setup can more easily be used with multiple shared GL context.
//  2018-02-20: Inputs: Added support for mouse cursors (ImGui::GetMouseCursor() value and WM_SETCURSOR message handling).
//  2018-02-20: Inputs: Renamed GLFW callbacks exposed in .h to not include GL3 in their name.
//  2018-02-16: Misc: Obsoleted the io.RenderDrawListsFn callback and exposed ImGui_ImplGlfwGL3_RenderDrawData() in the .h file so you can call it yourself.
//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.
//  2018-02-06: Inputs: Added mapping for ImGuiKey_Space.
//  2018-01-25: Inputs: Added gamepad support if ImGuiConfigFlags_NavEnableGamepad is set.
//  2018-01-25: Inputs: Honoring the io.WantSetMousePos flag by repositioning the mouse (ImGuiConfigFlags_NavEnableSetMousePos is set).
//  2018-01-20: Inputs: Added Horizontal Mouse Wheel support.
//  2018-01-18: Inputs: Added mapping for ImGuiKey_Insert.
//  2018-01-07: OpenGL: Changed GLSL shader version from 330 to 150. (Also changed GL context from 3.3 to 3.2 in example's main.cpp)
//  2017-09-01: OpenGL: Save and restore current bound sampler. Save and restore current polygon mode.
//  2017-08-25: Inputs: MousePos set to -FLT_MAX,-FLT_MAX when mouse is unavailable/missing (instead of -1,-1).
//  2017-05-01: OpenGL: Fixed save and restore of current blend function state.
//  2016-10-15: Misc: Added a void* user_data parameter to Clipboard function handlers.
//  2016-09-05: OpenGL: Fixed save and restore of current scissor rectangle.
//  2016-04-30: OpenGL: Fixed save and restore of current GL_ACTIVE_TEXTURE.

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "imgui.h"
#include "imgui_tabs.h"

// GL3W/GLFW
#include <GL/gl3w.h>    // This example is using gl3w to access OpenGL functions (because it is small). You may use glew/glad/glLoadGen/etc. whatever already works for you.
#include <GLFW/glfw3.h>
#ifdef _WIN32
#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3native.h>
#endif

#include <new>

#include "puss_module_imgui.h"
#include "scintilla_imgui.h"

static char				g_GlslVersion[32] = "#version 150";

class ImguiEnv {
public:
	// GLFW data
	GLFWwindow*			g_Window;
	ImGuiContext*		g_Context;
	double				g_Time;
	bool				g_MouseJustPressed[3];
	GLFWcursor*			g_MouseCursors[ImGuiMouseCursor_COUNT];

	// OpenGL3 data
	GLuint				g_FontTexture;
	int					g_ShaderHandle;
	int					g_VertHandle;
	int					g_FragHandle;
	int					g_AttribLocationTex;
	int					g_AttribLocationProjMtx;
	int					g_AttribLocationPosition;
	int					g_AttribLocationUV;
	int					g_AttribLocationColor;
	unsigned int		g_VboHandle;
	unsigned int		g_ElementsHandle;
	int					g_ScriptErrorHandle;
	int					g_StackProtected;
	ImVector<char>		g_Stack;

public:
	ImguiEnv()
		: g_Window(NULL)
		, g_Context(NULL)
		, g_Time(0.0)
		, g_FontTexture(0)
		, g_ShaderHandle(0)
		, g_VertHandle(0)
		, g_FragHandle(0)
		, g_AttribLocationTex(0)
		, g_AttribLocationProjMtx(0)
		, g_AttribLocationPosition(0)
		, g_AttribLocationUV(0)
		, g_AttribLocationColor(0)
		, g_VboHandle(0)
		, g_ElementsHandle(0)
	{
		for( int i=0; i<3; ++i ) {
			g_MouseJustPressed[0] = false;
		}
		for( int i=0; i<ImGuiMouseCursor_COUNT; ++i ) {
			g_MouseCursors[i] = 0;
		}
	}

	bool ImGui_ImplGlfwGL3_Init(GLFWwindow* window, bool install_callbacks, const char* glsl_version=NULL);
	void ImGui_ImplGlfwGL3_RenderDrawData(ImDrawData* draw_data);
	bool ImGui_ImplGlfwGL3_CreateFontsTexture();
	bool ImGui_ImplGlfwGL3_CreateDeviceObjects();
	void ImGui_ImplGlfwGL3_InvalidateDeviceObjects();
	void ImGui_ImplGlfwGL3_Shutdown();
	void ImGui_ImplGlfwGL3_NewFrame();
};

// OpenGL3 Render function.
// (this used to be set in io.RenderDrawListsFn and called by ImGui::Render(), but you can now call this directly from your main loop)
// Note that this implementation is little overcomplicated because we are saving/setting up/restoring every OpenGL state explicitly, in order to be able to run within any OpenGL engine that doesn't do so. 
void ImguiEnv::ImGui_ImplGlfwGL3_RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO& io = ImGui::GetIO();
    int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fb_width == 0 || fb_height == 0)
        return;
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    // Backup GL state
    GLenum last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture);
    glActiveTexture(GL_TEXTURE0);
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    GLint last_sampler; glGetIntegerv(GL_SAMPLER_BINDING, &last_sampler);
    GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    GLint last_element_array_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
    GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
    GLint last_polygon_mode[2]; glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
    GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    GLenum last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
    GLenum last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
    GLenum last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
    GLenum last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
    GLenum last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
    GLenum last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);
    GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Setup viewport, orthographic projection matrix
    glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
    const float ortho_projection[4][4] =
    {
        { 2.0f/io.DisplaySize.x, 0.0f,                   0.0f, 0.0f },
        { 0.0f,                  2.0f/-io.DisplaySize.y, 0.0f, 0.0f },
        { 0.0f,                  0.0f,                  -1.0f, 0.0f },
        {-1.0f,                  1.0f,                   0.0f, 1.0f },
    };
    glUseProgram(g_ShaderHandle);
    glUniform1i(g_AttribLocationTex, 0);
    glUniformMatrix4fv(g_AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
    glBindSampler(0, 0); // Rely on combined texture/sampler state.

    // Recreate the VAO every time 
    // (This is to easily allow multiple GL contexts. VAO are not shared among GL contexts, and we don't track creation/deletion of windows so we don't have an obvious key to use to cache them.)
    GLuint vao_handle = 0;
    glGenVertexArrays(1, &vao_handle);
    glBindVertexArray(vao_handle);
    glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
    glEnableVertexAttribArray(g_AttribLocationPosition);
    glEnableVertexAttribArray(g_AttribLocationUV);
    glEnableVertexAttribArray(g_AttribLocationColor);
    glVertexAttribPointer(g_AttribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(g_AttribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(g_AttribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));

    // Draw
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawIdx* idx_buffer_offset = 0;

        glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ElementsHandle);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
            }
            idx_buffer_offset += pcmd->ElemCount;
        }
    }
    glDeleteVertexArrays(1, &vao_handle);

    // Restore modified GL state
    glUseProgram(last_program);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindSampler(0, last_sampler);
    glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
    glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
    if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
    glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
}

static const char* ImGui_ImplGlfwGL3_GetClipboardText(void* user_data)
{
    return glfwGetClipboardString((GLFWwindow*)user_data);
}

static void ImGui_ImplGlfwGL3_SetClipboardText(void* user_data, const char* text)
{
    glfwSetClipboardString((GLFWwindow*)user_data, text);
}

static void ImGui_ImplGlfw_MouseButtonCallback(GLFWwindow* w, int button, int action, int /*mods*/)
{
    if (action == GLFW_PRESS && button >= 0 && button < 3) {
		ImguiEnv* env = (ImguiEnv*)glfwGetWindowUserPointer(w);
		if( env ) {
			env->g_MouseJustPressed[button] = true;
		}
	}
}

static void ImGui_ImplGlfw_ScrollCallback(GLFWwindow*, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheelH += (float)xoffset;
    io.MouseWheel += (float)yoffset;
}

static void puss_imgui_key_set(ImGuiIO& io, int key, bool st) {
	if( ((unsigned)key) <= PUSS_IMGUI_BASIC_KEY_LAST ) {
		io.KeysDown[key] = st;
	} else {
		switch(key) {
		#define _PUSS_IMGUI_KEY_REG(key)	case GLFW_KEY_ ## key: io.KeysDown[PUSS_IMGUI_KEY_ ## key] = st;	break;
			#include "puss_module_imgui_keys.inl"
		#undef _PUSS_IMGUI_KEY_REG
		default:
			break;
		}
	}
}

static void ImGui_ImplGlfw_KeyCallback(GLFWwindow*, int key, int, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (action == GLFW_PRESS) {
		puss_imgui_key_set(io, key, true);
	} else if (action == GLFW_RELEASE) {
		puss_imgui_key_set(io, key, false);
	}

    (void)mods; // Modifiers are not reliable across systems
    io.KeyCtrl = io.KeysDown[PUSS_IMGUI_KEY_LEFT_CONTROL] || io.KeysDown[PUSS_IMGUI_KEY_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[PUSS_IMGUI_KEY_LEFT_SHIFT] || io.KeysDown[PUSS_IMGUI_KEY_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[PUSS_IMGUI_KEY_LEFT_ALT] || io.KeysDown[PUSS_IMGUI_KEY_RIGHT_ALT];
    io.KeySuper = io.KeysDown[PUSS_IMGUI_KEY_LEFT_SUPER] || io.KeysDown[PUSS_IMGUI_KEY_RIGHT_SUPER];
}

static void ImGui_ImplGlfw_CharCallback(GLFWwindow*, unsigned int c)
{
    ImGuiIO& io = ImGui::GetIO();
    if (c > 0 && c < 0x10000)
        io.AddInputCharacter((unsigned short)c);
}

bool ImguiEnv::ImGui_ImplGlfwGL3_CreateFontsTexture()
{
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

    // Upload texture to graphics system
    GLint last_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGenTextures(1, &g_FontTexture);
    glBindTexture(GL_TEXTURE_2D, g_FontTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Store our identifier
    io.Fonts->TexID = (void *)(intptr_t)g_FontTexture;

    // Restore state
    glBindTexture(GL_TEXTURE_2D, last_texture);

    return true;
}

bool ImguiEnv::ImGui_ImplGlfwGL3_CreateDeviceObjects()
{
    // Backup GL state
    GLint last_texture, last_array_buffer, last_vertex_array;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

    const GLchar* vertex_shader =
        "uniform mat4 ProjMtx;\n"
        "in vec2 Position;\n"
        "in vec2 UV;\n"
        "in vec4 Color;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "	Frag_UV = UV;\n"
        "	Frag_Color = Color;\n"
        "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";

    const GLchar* fragment_shader =
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "void main()\n"
        "{\n"
        "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
        "}\n";

    const GLchar* vertex_shader_with_version[2] = { g_GlslVersion, vertex_shader };
    const GLchar* fragment_shader_with_version[2] = { g_GlslVersion, fragment_shader };

    g_ShaderHandle = glCreateProgram();
    g_VertHandle = glCreateShader(GL_VERTEX_SHADER);
    g_FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(g_VertHandle, 2, vertex_shader_with_version, NULL);
    glShaderSource(g_FragHandle, 2, fragment_shader_with_version, NULL);
    glCompileShader(g_VertHandle);
    glCompileShader(g_FragHandle);
    glAttachShader(g_ShaderHandle, g_VertHandle);
    glAttachShader(g_ShaderHandle, g_FragHandle);
    glLinkProgram(g_ShaderHandle);

    g_AttribLocationTex = glGetUniformLocation(g_ShaderHandle, "Texture");
    g_AttribLocationProjMtx = glGetUniformLocation(g_ShaderHandle, "ProjMtx");
    g_AttribLocationPosition = glGetAttribLocation(g_ShaderHandle, "Position");
    g_AttribLocationUV = glGetAttribLocation(g_ShaderHandle, "UV");
    g_AttribLocationColor = glGetAttribLocation(g_ShaderHandle, "Color");

    glGenBuffers(1, &g_VboHandle);
    glGenBuffers(1, &g_ElementsHandle);

    ImGui_ImplGlfwGL3_CreateFontsTexture();

    // Restore modified GL state
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindVertexArray(last_vertex_array);

    return true;
}

void ImguiEnv::ImGui_ImplGlfwGL3_InvalidateDeviceObjects()
{
    if (g_VboHandle) glDeleteBuffers(1, &g_VboHandle);
    if (g_ElementsHandle) glDeleteBuffers(1, &g_ElementsHandle);
    g_VboHandle = g_ElementsHandle = 0;

    if (g_ShaderHandle && g_VertHandle) glDetachShader(g_ShaderHandle, g_VertHandle);
    if (g_VertHandle) glDeleteShader(g_VertHandle);
    g_VertHandle = 0;

    if (g_ShaderHandle && g_FragHandle) glDetachShader(g_ShaderHandle, g_FragHandle);
    if (g_FragHandle) glDeleteShader(g_FragHandle);
    g_FragHandle = 0;

    if (g_ShaderHandle) glDeleteProgram(g_ShaderHandle);
    g_ShaderHandle = 0;

    if (g_FontTexture)
    {
        glDeleteTextures(1, &g_FontTexture);
        ImGui::GetIO().Fonts->TexID = 0;
        g_FontTexture = 0;
    }
}

static void ImGui_ImplGlfw_InstallCallbacks(GLFWwindow* window)
{
    glfwSetMouseButtonCallback(window, ImGui_ImplGlfw_MouseButtonCallback);
    glfwSetScrollCallback(window, ImGui_ImplGlfw_ScrollCallback);
    glfwSetKeyCallback(window, ImGui_ImplGlfw_KeyCallback);
    glfwSetCharCallback(window, ImGui_ImplGlfw_CharCallback);
}

bool ImguiEnv::ImGui_ImplGlfwGL3_Init(GLFWwindow* window, bool install_callbacks, const char* glsl_version)
{
    g_Window = window;
	glfwSetWindowUserPointer(g_Window, this);

    // Store GL version string so we can refer to it later in case we recreate shaders.
    if (glsl_version == NULL)
        glsl_version = "#version 150";
    IM_ASSERT((int)strlen(glsl_version) + 2 < IM_ARRAYSIZE(g_GlslVersion));
    strcpy(g_GlslVersion, glsl_version);
    strcat(g_GlslVersion, "\n");

    // Setup back-end capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;   // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;    // We can honor io.WantSetMousePos requests (optional, rarely used)

    // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
    io.KeyMap[ImGuiKey_Tab] = PUSS_IMGUI_KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = PUSS_IMGUI_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = PUSS_IMGUI_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = PUSS_IMGUI_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = PUSS_IMGUI_KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = PUSS_IMGUI_KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = PUSS_IMGUI_KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = PUSS_IMGUI_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = PUSS_IMGUI_KEY_END;
    io.KeyMap[ImGuiKey_Insert] = PUSS_IMGUI_KEY_INSERT;
    io.KeyMap[ImGuiKey_Delete] = PUSS_IMGUI_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = PUSS_IMGUI_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = PUSS_IMGUI_KEY_ESCAPE;
    io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
    io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
    io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
    io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
    io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

    io.SetClipboardTextFn = ImGui_ImplGlfwGL3_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplGlfwGL3_GetClipboardText;
    io.ClipboardUserData = g_Window;
#ifdef _WIN32
    io.ImeWindowHandle = glfwGetWin32Window(g_Window);
#endif

    // Load cursors
    // FIXME: GLFW doesn't expose suitable cursors for ResizeAll, ResizeNESW, ResizeNWSE. We revert to arrow cursor for those.
    g_MouseCursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    g_MouseCursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    g_MouseCursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    g_MouseCursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    g_MouseCursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);

    if (install_callbacks)
        ImGui_ImplGlfw_InstallCallbacks(window);

    return true;
}

void ImguiEnv::ImGui_ImplGlfwGL3_Shutdown()
{
    // Destroy GLFW mouse cursors
    for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
        glfwDestroyCursor(g_MouseCursors[cursor_n]);
    memset(g_MouseCursors, 0, sizeof(g_MouseCursors));

    // Destroy OpenGL objects
    ImGui_ImplGlfwGL3_InvalidateDeviceObjects();
}

void ImguiEnv::ImGui_ImplGlfwGL3_NewFrame()
{
    if (!g_FontTexture)
        ImGui_ImplGlfwGL3_CreateDeviceObjects();

    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    glfwGetWindowSize(g_Window, &w, &h);
    glfwGetFramebufferSize(g_Window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

    // Setup time step
    double current_time =  glfwGetTime();
    io.DeltaTime = g_Time > 0.0 ? (float)(current_time - g_Time) : (float)(1.0f/60.0f);
    g_Time = current_time;

    // Setup inputs
    // (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
    if (glfwGetWindowAttrib(g_Window, GLFW_FOCUSED))
    {
        // Set OS mouse position if requested (only used when ImGuiConfigFlags_NavEnableSetMousePos is enabled by user)
        if (io.WantSetMousePos)
        {
            glfwSetCursorPos(g_Window, (double)io.MousePos.x, (double)io.MousePos.y);
        }
        else
        {
            double mouse_x, mouse_y;
            glfwGetCursorPos(g_Window, &mouse_x, &mouse_y);
            io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
        }
    }
    else
    {
        io.MousePos = ImVec2(-FLT_MAX,-FLT_MAX);
    }

    for (int i = 0; i < 3; i++)
    {
        // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
        io.MouseDown[i] = g_MouseJustPressed[i] || glfwGetMouseButton(g_Window, i) != 0;
        g_MouseJustPressed[i] = false;
    }

    // Update OS/hardware mouse cursor if imgui isn't drawing a software cursor
    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0 && glfwGetInputMode(g_Window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED)
    {
        ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
        if (io.MouseDrawCursor || cursor == ImGuiMouseCursor_None)
        {
            glfwSetInputMode(g_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        }
        else
        {
            glfwSetCursor(g_Window, g_MouseCursors[cursor] ? g_MouseCursors[cursor] : g_MouseCursors[ImGuiMouseCursor_Arrow]);
            glfwSetInputMode(g_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    // Gamepad navigation mapping [BETA]
    memset(io.NavInputs, 0, sizeof(io.NavInputs));
    if (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad)
    {
        // Update gamepad inputs
        #define MAP_BUTTON(NAV_NO, BUTTON_NO)       { if (buttons_count > BUTTON_NO && buttons[BUTTON_NO] == GLFW_PRESS) io.NavInputs[NAV_NO] = 1.0f; }
        #define MAP_ANALOG(NAV_NO, AXIS_NO, V0, V1) { float v = (axes_count > AXIS_NO) ? axes[AXIS_NO] : V0; v = (v - V0) / (V1 - V0); if (v > 1.0f) v = 1.0f; if (io.NavInputs[NAV_NO] < v) io.NavInputs[NAV_NO] = v; }
        int axes_count = 0, buttons_count = 0;
        const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_count);
        const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttons_count);
        MAP_BUTTON(ImGuiNavInput_Activate,   0);     // Cross / A
        MAP_BUTTON(ImGuiNavInput_Cancel,     1);     // Circle / B
        MAP_BUTTON(ImGuiNavInput_Menu,       2);     // Square / X
        MAP_BUTTON(ImGuiNavInput_Input,      3);     // Triangle / Y
        MAP_BUTTON(ImGuiNavInput_DpadLeft,   13);    // D-Pad Left
        MAP_BUTTON(ImGuiNavInput_DpadRight,  11);    // D-Pad Right
        MAP_BUTTON(ImGuiNavInput_DpadUp,     10);    // D-Pad Up
        MAP_BUTTON(ImGuiNavInput_DpadDown,   12);    // D-Pad Down
        MAP_BUTTON(ImGuiNavInput_FocusPrev,  4);     // L1 / LB
        MAP_BUTTON(ImGuiNavInput_FocusNext,  5);     // R1 / RB
        MAP_BUTTON(ImGuiNavInput_TweakSlow,  4);     // L1 / LB
        MAP_BUTTON(ImGuiNavInput_TweakFast,  5);     // R1 / RB
        MAP_ANALOG(ImGuiNavInput_LStickLeft, 0,  -0.3f,  -0.9f);
        MAP_ANALOG(ImGuiNavInput_LStickRight,0,  +0.3f,  +0.9f);
        MAP_ANALOG(ImGuiNavInput_LStickUp,   1,  +0.3f,  +0.9f);
        MAP_ANALOG(ImGuiNavInput_LStickDown, 1,  -0.3f,  -0.9f);
        #undef MAP_BUTTON
        #undef MAP_ANALOG
        if (axes_count > 0 && buttons_count > 0) 
            io.BackendFlags |= ImGuiBackendFlags_HasGamepad; 
        else 
            io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    }

    // Start the frame. This call will update the io.WantCaptureMouse, io.WantCaptureKeyboard flag that you can use to dispatch inputs (or not) to your application.
    ImGui::NewFrame();
}

#define IMGUI_LUA_WRAP_STACK_BEGIN(tp) { \
	ImguiEnv* env = (ImguiEnv*)(ImGui::GetIO().UserData); \
	env->g_Stack.push_back(tp); \
}

#define IMGUI_LUA_WRAP_STACK_END(tp) { \
	ImguiEnv* env = (ImguiEnv*)(ImGui::GetIO().UserData); \
	int top_type = (env->g_Stack.size() > env->g_StackProtected) ? env->g_Stack.back() : -1; \
	if( top_type==tp ) { \
		env->g_Stack.pop_back(); \
	} else { \
		luaL_error(L, "Stack Push/Pop NOT matched!"); \
	} \
}

#include "imgui_lua.inl"

#undef IMGUI_LUA_WRAP_STACK_BEGIN
#undef IMGUI_LUA_WRAP_STACK_END

#define IMGUI_LIB_NAME	"ImguiLib"

static int imgui_error_handle_default(lua_State* L) {
	fprintf(stderr, "[ImGui] error: %s\n", lua_tostring(L, -1));
	return lua_gettop(L);
}

static int imgui_destroy_lua(lua_State* L) {
	ImguiEnv* env = (ImguiEnv*)luaL_checkudata(L, 1, IMGUI_MT_NAME);
	if( env->g_Window ) {
		GLFWwindow* win = env->g_Window;
		env->ImGui_ImplGlfwGL3_Shutdown();
		ImGui::DestroyContext(env->g_Context);
		env->g_Window = NULL;
		glfwDestroyWindow(win);
	}
	if( env->g_ScriptErrorHandle!=LUA_NOREF ) {
		int ref = env->g_ScriptErrorHandle;
		env->g_ScriptErrorHandle = LUA_NOREF;
		luaL_unref(L, LUA_REGISTRYINDEX, ref);
	}
	return 0;
}

static int imgui_set_error_handle_lua(lua_State* L) {
	ImguiEnv* env = (ImguiEnv*)luaL_checkudata(L, 1, IMGUI_MT_NAME);
	if( lua_isfunction(L, 2) ) {
		lua_pushvalue(L, 2);
		if( env->g_ScriptErrorHandle==LUA_NOREF ) {
			env->g_ScriptErrorHandle = luaL_ref(L, LUA_REGISTRYINDEX);
		} else {
			lua_rawseti(L, LUA_REGISTRYINDEX, env->g_ScriptErrorHandle);
		}
	}
	if( env->g_ScriptErrorHandle!=LUA_NOREF ) {
		lua_rawgeti(L, LUA_REGISTRYINDEX, env->g_ScriptErrorHandle);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int imgui_update_lua(lua_State* L) {
	ImguiEnv* env = (ImguiEnv*)luaL_checkudata(L, 1, IMGUI_MT_NAME);
	GLFWwindow* win = env->g_Window;
	luaL_argcheck(L, lua_type(L, 2)==LUA_TFUNCTION, 2, "need function!");

	// check close
	if( !win ) return 0;
    if( glfwWindowShouldClose(win) ) return 0;

	ImGui::SetCurrentContext(env->g_Context);

	// input
	glfwPollEvents();
	env->ImGui_ImplGlfwGL3_NewFrame();

	env->g_Stack.clear();
	env->g_StackProtected = 0;

	// run
	lua_rawgeti(L, LUA_REGISTRYINDEX, env->g_ScriptErrorHandle);
	lua_replace(L, 1);
	if( lua_pcall(L, lua_gettop(L)-2, 0, 1) )
		lua_pop(L, 1);

	// check close
	win = env->g_Window;
	if( !win ) return 0;
    if( glfwWindowShouldClose(win) ) return 0;

	// check stack
	env->g_StackProtected = 0;
	while( !env->g_Stack.empty() ) {
		int tp = env->g_Stack.back();
		env->g_Stack.pop_back();
		IMGUI_LUA_WRAP_STACK_POP(tp);
	}

	// render
	glfwMakeContextCurrent(win);
	int width, height;
	glfwGetFramebufferSize(win, &width, &height);

	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	ImGui::Render();
	env->ImGui_ImplGlfwGL3_RenderDrawData(ImGui::GetDrawData());
	ImGui::EndFrame();

	glfwSwapBuffers(win);
	lua_pushboolean(L, 1);
	return 1;
}

static int imgui_protect_pcall_lua(lua_State* L) {
	ImguiEnv* env = (ImguiEnv*)luaL_checkudata(L, 1, IMGUI_MT_NAME);
	int base = env->g_StackProtected;
	int top = env->g_Stack.size();
	env->g_StackProtected = top;
	lua_rawgeti(L, LUA_REGISTRYINDEX, env->g_ScriptErrorHandle);
	lua_replace(L, 1);
	lua_pushboolean(L, lua_pcall(L, lua_gettop(L)-2, LUA_MULTRET, 1)==LUA_OK);
	lua_replace(L, 1);
	env->g_StackProtected = base;
	while( env->g_Stack.size() > top ) {
		int tp = env->g_Stack.back();
		env->g_Stack.pop_back();
		IMGUI_LUA_WRAP_STACK_POP(tp);
	}
	return lua_gettop(L);
}

static int imgui_getio_display_size_lua(lua_State* L) {
	ImGuiIO& io = ImGui::GetIO();
	lua_pushnumber(L, io.DisplaySize.x);
	lua_pushnumber(L, io.DisplaySize.y);
	return 2;
}

static int imgui_getio_delta_time_lua(lua_State* L) {
	ImGuiIO& io = ImGui::GetIO();
	lua_pushnumber(L, io.DeltaTime);
	return 1;
}

static bool shortcut_mod_check(lua_State* L, int arg, bool val) {
	return lua_isnoneornil(L, arg) ? true : (val == ((lua_toboolean(L, 2)!=0)));
}

static int imgui_is_shortcut_pressed_lua(lua_State* L) {
	ImGuiIO& io = ImGui::GetIO();
	int key = (int)luaL_checkinteger(L, 1);
	bool pressed = shortcut_mod_check(L, 2, io.KeyCtrl)
		&& shortcut_mod_check(L, 3, io.KeyShift)
		&& shortcut_mod_check(L, 4, io.KeyAlt)
		&& shortcut_mod_check(L, 5, io.KeySuper)
		&& (key>=0 && key<PUSS_IMGUI_TOTAL_KEY_LAST)
		&& ImGui::IsKeyPressed(key)
		;
	lua_pushboolean(L, pressed ? 1 : 0);
	return 1;
}

static int imgui_create_lua(lua_State* L) {
	const char* title = luaL_optstring(L, 1, "imgui window");
	int width = (int)luaL_optinteger(L, 2, 1024);
	int height = (int)luaL_optinteger(L, 3, 768);
	ImguiEnv* env = new (lua_newuserdata(L, sizeof(ImguiEnv))) ImguiEnv;
	GLFWwindow* win = NULL;
	int err = 0;
	env->g_ScriptErrorHandle = LUA_NOREF;
	luaL_setmetatable(L, IMGUI_MT_NAME);

	lua_pushcfunction(L, imgui_error_handle_default);
	env->g_ScriptErrorHandle = luaL_ref(L, LUA_REGISTRYINDEX);

    win = glfwCreateWindow(width, height, title, NULL, NULL);
    if( !win ) return luaL_error(L, "glfw window create failed!");
    glfwMakeContextCurrent(win);
	glfwSwapInterval(1); // Enable vsync

	static bool glad_inited = false;
	if( !glad_inited ) {
		glad_inited = true;
		gl3wInit();
	}

    glfwGetWindowSize(win, &width, &height);
    glViewport(0, 0, width, height);

    // Setup ImGui binding
    env->g_Context = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.UserData = env;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
    env->ImGui_ImplGlfwGL3_Init(win, true, NULL);

    // Setup style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them. 
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple. 
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

	// char pth[4096];
	// sprintf(pth, "%s/fonts/wqy-micro-hei.ttf", puss_app_path(L));
	// io.Fonts->AddFontFromFileTTF(pth, 18.0f, 0, io.Fonts->GetGlyphRangesChinese());

	if( err )
		return lua_error(L);

	env->ImGui_ImplGlfwGL3_NewFrame();
	ImGui::EndFrame();
	return 1;
}

static int imgui_wait_events_lua(lua_State* L) {
	double timeout = luaL_optnumber(L, 1, 0.01);
	glfwWaitEventsTimeout(timeout);
	return 0;
}

#include "scintilla_imgui_lua.inl"
#include "scintilla.iface.inl"

static luaL_Reg imgui_lua_apis[] =
	{ {"Create", imgui_create_lua}
	, {"WaitEventsTimeout", imgui_wait_events_lua}

	, {"CreateByteArray", byte_array_create}
	, {"CreateFloatArray", float_array_create}
	, {"CreateScintilla", im_scintilla_create}

	, {"GetIODisplaySize", imgui_getio_display_size_lua}
	, {"GetIODeltaTime", imgui_getio_delta_time_lua}
	, {"IsShortcutPressed", imgui_is_shortcut_pressed_lua}

#define __REG_WRAP(w,f)	, { #w, f }
	#include "imgui_wraps.inl"
#undef __REG_WRAP
	, {NULL, NULL}
	};

static void lua_register_imgui(lua_State* L) {
	// consts
	puss_push_const_table(L);
#define __REG_ENUM(e)	lua_pushinteger(L, e);	lua_setfield(L, -2, #e);
	#include "imgui_enums.inl"
#undef __REG_ENUM
#define _PUSS_IMGUI_KEY_REG(key)	lua_pushinteger(L, PUSS_IMGUI_KEY_ ## key);	lua_setfield(L, -2, "PUSS_IMGUI_KEY_" #key);
	#include "puss_module_imgui_keys.inl"
#undef _PUSS_IMGUI_KEY_REG
	lua_pop(L, 1);

	if( luaL_newmetatable(L, IMGUI_MT_NAME) ) {
		static luaL_Reg methods[] =
			{ {"__index", NULL}
			, {"__gc", imgui_destroy_lua}
			, {"__call", imgui_update_lua}
			, {"destroy", imgui_destroy_lua}
			, {"set_error_handle", imgui_set_error_handle_lua}
			, {"protect_pcall", imgui_protect_pcall_lua}
			, {NULL, NULL}
			};
		luaL_setfuncs(L, methods, 0);
	}
	lua_setfield(L, -1, "__index");
}

static void im_scintilla_update_callback(ScintillaIM* sci, void* ud) {
	lua_State* L = (lua_State*)ud;
	ImguiEnv* env = (ImguiEnv*)(ImGui::GetIO().UserData);
	if( !env )	return;
	lua_rawgeti(L, LUA_REGISTRYINDEX, env->g_ScriptErrorHandle);
	lua_insert(L, 1);		// error handle
	lua_pushvalue(L, 2);	// self
	lua_pushvalue(L, 3);	// function
	lua_replace(L, 2);
	lua_replace(L, 3);
	lua_pcall(L, lua_gettop(L)-2, 0, 1);
}

static int im_scintilla_update(lua_State* L) {
	ScintillaIM** ud = (ScintillaIM**)luaL_checkudata(L, 1, LUA_IM_SCI_NAME);
	scintilla_imgui_update(*ud, lua_isfunction(L, 2) ? im_scintilla_update_callback : NULL, L);
	return 0;
}

static void lua_register_scintilla(lua_State* L) {
	// consts
	{
		puss_push_const_table(L);
		for( IFaceVal* p=sci_values; p->name; ++p ) {
			lua_pushinteger(L, p->val);
			lua_setfield(L, -2, p->name);
		}
		lua_pop(L, 1);
	}

	// metatable: fun/get/set
	if( luaL_newmetatable(L, LUA_IM_SCI_NAME) ) {
		static luaL_Reg methods[] =
			{ {"__index", NULL}
			, {"__call",im_scintilla_update}
			, {"__gc",im_scintilla_destroy}
			, {"destroy",im_scintilla_destroy}
			, {NULL, NULL}
			};
		luaL_setfuncs(L, methods, 0);

		for( IFaceDecl* p=sci_functions; p->name; ++p ) {
			lua_pushlightuserdata(L, p);
			lua_pushcclosure(L, _lua__sci_send_wrap, 1);
			lua_setfield(L, -2, p->name);
		}
	}
	lua_setfield(L, -1, "__index");
}

PussInterface* __puss_iface__ = NULL;

static void error_callback(int e, const char *d) {
	fprintf(stderr, "[GFLW] error %d: %s\n", e, d);
}

static int glfw_inited = 0;

PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, PussInterface* puss) {
    if( !glfw_inited ) {
    	glfw_inited = glfwInit();
    	if( !glfw_inited ) return luaL_error(L, "[GFLW] failed to init!\n");
    	atexit(glfwTerminate);

	    glfwSetErrorCallback(error_callback);

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	}

	if( !__puss_iface__ ) {
		__puss_iface__= puss;
	}

	if( lua_getfield(L, LUA_REGISTRYINDEX, IMGUI_LIB_NAME)==LUA_TTABLE )
		return 1;
	lua_pop(L, 1);

	luaL_newlib(L, imgui_lua_apis);
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, IMGUI_LIB_NAME);
	{
		lua_register_imgui(L);
		lua_register_scintilla(L);
	}
	return 1;
}

