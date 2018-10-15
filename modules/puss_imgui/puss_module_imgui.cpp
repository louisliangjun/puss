// puss_module_imgui.cpp

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "puss_module_imgui.h"

#include "imgui.h"

#ifdef PUSS_IMGUI_USE_DX11
	#include <windows.h>
	#include <tchar.h>
	#include <winuser.h>
	#include <d3d11.h>
	#define DIRECTINPUT_VERSION 0x0800
	#include <dinput.h>
	#include <tchar.h>

	#ifndef WM_DPICHANGED
	#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
	#endif

	#define PUSS_IMGUI_WINDOW_CLASS	_T("PussImGuiWindow")

	static WNDCLASSEX puss_imgui_wc = { sizeof(WNDCLASSEX), CS_CLASSDC, NULL, 0L, 0L, NULL, NULL, NULL, NULL, NULL, PUSS_IMGUI_WINDOW_CLASS, NULL };

	static int _win32_vk_map[256];

	static int _win32_vk_convert(WPARAM wParam, LPARAM lParam) {
		int key = _win32_vk_map[wParam];
		if( key ) {
			switch(wParam) {
			case VK_SHIFT:
				key = GetKeyState(VK_LSHIFT) ? PUSS_IMGUI_KEY_LEFT_SHIFT : PUSS_IMGUI_KEY_RIGHT_SHIFT;
				break;
			case VK_CONTROL:
				key = GetKeyState(VK_LCONTROL) ? PUSS_IMGUI_KEY_LEFT_CONTROL : PUSS_IMGUI_KEY_RIGHT_CONTROL;
				break;
			case VK_MENU:
				key = GetKeyState(VK_LMENU) ? PUSS_IMGUI_KEY_LEFT_ALT : PUSS_IMGUI_KEY_RIGHT_ALT;
				break;
			case VK_RETURN:
				key = (lParam & (1<<24)) ? PUSS_IMGUI_KEY_KP_ENTER : PUSS_IMGUI_KEY_ENTER;
				break;
			}
		}
		return key;
	}

	static int _lua_utf8_to_utf16(lua_State* L) {
		size_t len = 0;
		const char* str = luaL_checklstring(L, 1, &len);
		int wlen = MultiByteToWideChar(CP_UTF8, 0, str, (int)len, NULL, 0);
		if( wlen > 0 ) {
			luaL_Buffer B;
			wchar_t* wstr = (wchar_t*)luaL_buffinitsize(L, &B, (size_t)(wlen<<1));
			MultiByteToWideChar(CP_UTF8, 0, str, (int)len, wstr, wlen);
			luaL_addsize(&B, (size_t)(wlen<<1));
			luaL_addchar(&B, '\0');	// add more one byte for \u0000
			luaL_pushresult(&B);
		} else if( wlen==0 ) {
			lua_pushliteral(L, "");
		} else {
			luaL_error(L, "utf8 to utf16 convert failed!");
		}
		return 1;
	}

	#include "imgui_impl_win32.inl"
	#include "imgui_impl_dx11.inl"

	#pragma comment(lib, "d3d11.lib")
	#pragma comment(lib, "d3dcompiler.lib")
	#pragma comment(lib, "dxgi.lib")

#else

	#include "imgui_impl_opengl3.inl"
	#include "imgui_impl_glfw.inl"

	#ifdef _WIN32
		#pragma comment(lib, "glfw3.lib")
		#pragma comment(lib, "opengl32.lib")
	#endif

	// About OpenGL function loaders: modern OpenGL doesn't have a standard header file and requires individual function pointers to be loaded manually. 
	// Helper libraries are often used for this purpose! Here we are supporting a few common ones: gl3w, glew, glad.
	// You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
	#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
	#include <GL/gl3w.h>    // Initialize with gl3wInit()
	#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
	#include <GL/glew.h>    // Initialize with glewInit()
	#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
	#include <glad/glad.h>  // Initialize with gladLoadGL()
	#else
	#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
	#endif

	#include <GLFW/glfw3.h> // Include glfw3.h after our OpenGL definitions

	#ifdef _WIN32
	#undef APIENTRY
	#define GLFW_EXPOSE_NATIVE_WIN32
	#include <GLFW/glfw3native.h>   // for glfwGetWin32Window
	#endif
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

class ImguiEnv {
public:
	ImGuiContext*	g_Context;

private:
	void do_create_context() {
		// Setup ImGui binding
		g_Context = ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.UserData = this;

		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;
		io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;     // FIXME-DPI: THIS CURRENTLY DOESN'T WORK AS EXPECTED. DON'T USE IN USER APP!
		io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports; // FIXME-DPI
		io.ConfigResizeWindowsFromEdges = true;
		io.ConfigDockingWithShift = true;

		// Setup style
		ImGui::GetStyle().WindowRounding = 0.0f;
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();
	}

	void do_update_and_render_viewports() {
		// Update and Render additional Platform Windows
        if (g_Context->IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
	}

#ifdef PUSS_IMGUI_USE_DX11

public:
	void set_should_close(int value) {
		if( g_Context )
			ImGui::GetMainViewport()->PlatformRequestClose = true;
	}

	int get_should_close() {
		return (g_Context==NULL || ImGui::GetMainViewport()->PlatformRequestClose) ? 1 : 0;
	}

public:
	ID3D11Device*            g_pd3dDevice;
	ID3D11DeviceContext*     g_pd3dDeviceContext;
	IDXGISwapChain*          g_pSwapChain;
	ID3D11RenderTargetView*  g_mainRenderTargetView;

	void CreateRenderTarget()
	{
		ID3D11Texture2D* pBackBuffer;
		g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
		g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
		pBackBuffer->Release();
	}

	void CleanupRenderTarget()
	{
		if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
	}

	HRESULT CreateDeviceD3D(HWND hWnd)
	{
		// Setup swap chain
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		UINT createDeviceFlags = 0;
		//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
		D3D_FEATURE_LEVEL featureLevel;
		const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
		if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
			return E_FAIL;

		CreateRenderTarget();

		return S_OK;
	}

	void CleanupDeviceD3D()
	{
		CleanupRenderTarget();
		if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
		if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
		if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
	}

	LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if( ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) )
			return 1;

		switch (msg) {
		case WM_CREATE:
			// Initialize Direct3D
			if (CreateDeviceD3D(hWnd) < 0) {
				CleanupDeviceD3D();
			}
			break;
		case WM_CLOSE:
			set_should_close(1);
			return 1;
		case WM_DESTROY:
			// PostQuitMessage(0);
			return 0;
		case WM_SIZE:
			if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
			{
				if(g_hWnd==hWnd)
					ImGui_ImplDX11_InvalidateDeviceObjects();
				CleanupRenderTarget();
				g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
				CreateRenderTarget();
				if(g_hWnd==hWnd)
					ImGui_ImplDX11_CreateDeviceObjects();
			}
			return 0;
		case WM_SYSCOMMAND:
			if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
				return 0;
			break;
		case WM_DPICHANGED:
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
			{
				//const int dpi = HIWORD(wParam);
				//printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
				const RECT* suggested_rect = (RECT*)lParam;
				::SetWindowPos(hWnd, NULL, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
			}
			break;
		case WM_DROPFILES:
			if( wParam ) {
				UINT cap = 4096;
				UINT len = 0;
				TCHAR* buf = (TCHAR*)malloc(sizeof(TCHAR) * cap);
				HDROP h = (HDROP)wParam;
				UINT n = DragQueryFile(h, 0xFFFFFFFF, NULL, 0);
				for( UINT i=0; i<n; ++i ) {
					UINT sz = DragQueryFile(h, i, NULL, 0);
					if( sz ) {
						UINT nsz = len + sz + 1;
						if( nsz >= cap ) {
							cap *= 2;
							if( nsz >= cap )
								cap = nsz;
							buf = (TCHAR*)realloc(buf, sizeof(TCHAR) * cap);
						}
						DragQueryFile(h, i, buf+len, sz + 1);
						len += sz;
						buf[len++] = _T('\n');
					}
				}
				buf[len] = 0;
				g_DropFiles->resize(len*3+1);
				int res = WideCharToMultiByte(CP_UTF8, 0, buf, len, g_DropFiles->begin(), (int)(g_DropFiles->size()), NULL, NULL);
				free(buf);
				buf = NULL;

				if( res > 0 ) {
					g_DropFiles->resize(res);
				} else {
					g_DropFiles->clear();
				}
				DragFinish(h);
			}
			break;
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

public:
	bool create_window(const TCHAR* title, int width, int height) {
		int sx = GetSystemMetrics(SM_CXSCREEN) - 64;
		int sy = GetSystemMetrics(SM_CYSCREEN) - 64;
		if( width > sx )	width = sx;
		if( height > sy )	height = sy;
		RECT rc;
		rc.left = (sx - width) / 2;
		rc.top = (sy - height) / 2;
		if( rc.top < 64 ) rc.top = 64;
		rc.right = rc.left + width;
		rc.bottom = rc.top + height;

		AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
		HWND hwnd = CreateWindow(PUSS_IMGUI_WINDOW_CLASS, title, WS_OVERLAPPEDWINDOW, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, NULL, NULL, puss_imgui_wc.hInstance, this);

		// Show the window
		ShowWindow(hwnd, SW_SHOWDEFAULT);
		UpdateWindow(hwnd);
		DragAcceptFiles(hwnd, TRUE);
		do_create_context();
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
		return true;
	}

	void destroy_window() {
		if( !g_hWnd )
			return;
		HWND hWnd = g_hWnd;
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext(g_Context);
		CleanupDeviceD3D();
		DestroyWindow(hWnd);
	}

	bool prepare_newframe() {
		// input
		MSG msg;
		ZeroMemory(&msg, sizeof(msg));
		while (msg.message != WM_QUIT)
		{
			// Poll and handle messages (inputs, window resize, etc.)
			// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
			// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
			// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
			// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
			if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				continue;
			}
			break;
		}

		ImGuiContext* ctx = ImGui::GetCurrentContext();
		if( !ctx )
			return false;

		ImGuiIO& io = ImGui::GetIO();
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		io.KeysDown[PUSS_IMGUI_KEY_LEFT_SHIFT] = (GetKeyState(VK_LSHIFT) & 0x8000) != 0;
		io.KeysDown[PUSS_IMGUI_KEY_RIGHT_SHIFT] = (GetKeyState(VK_RSHIFT) & 0x8000) != 0;
		io.KeysDown[PUSS_IMGUI_KEY_LEFT_CONTROL] = (GetKeyState(VK_LCONTROL) & 0x8000) != 0;
		io.KeysDown[PUSS_IMGUI_KEY_RIGHT_CONTROL] = (GetKeyState(VK_RCONTROL) & 0x8000) != 0;
		io.KeysDown[PUSS_IMGUI_KEY_LEFT_ALT] = (GetKeyState(VK_LMENU) & 0x8000) != 0;
		io.KeysDown[PUSS_IMGUI_KEY_RIGHT_ALT] = (GetKeyState(VK_RMENU) & 0x8000) != 0;
		io.KeysDown[PUSS_IMGUI_KEY_LEFT_SUPER] = (GetKeyState(VK_LWIN) & 0x8000) != 0;
		io.KeysDown[PUSS_IMGUI_KEY_RIGHT_SUPER] = (GetKeyState(VK_RWIN) & 0x8000) != 0;
		return true;
	}

	bool prepare_render() {
		return g_hWnd!=NULL;
	}

	void render_drawdata() {
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		do_update_and_render_viewports();
		g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
	}

#else

public:
	GLFWwindow*      g_Window;

	void set_should_close(int value) {
		if( g_Window ) {
			glfwSetWindowShouldClose(g_Window, value);
		}
	}

	int get_should_close() {
		return (g_Window==NULL || glfwWindowShouldClose(g_Window));
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

	static void ImGui_Puss_KeyCallback(GLFWwindow* win, int key, int scancode, int action, int mods) {
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

	static void ImGui_Puss_DropCallback(GLFWwindow* w, int count, const char** files) {
		ImguiEnv* env = (ImguiEnv*)glfwGetWindowUserPointer(w);
		if( env && env->g_DropFiles ) {
			env->g_DropFiles->reserve(4096);
			env->g_DropFiles->clear();
			for(int i=0; i<count; ++i) {
				for( const char* p=files[i]; *p; ++p ) {
					env->g_DropFiles->push_back(*p);
				}
				env->g_DropFiles->push_back('\n');
			}
			glfwFocusWindow(w);
		}
	}

	bool create_window(const char* title, int width, int height) {
		g_Window = glfwCreateWindow(width, height, title, NULL, NULL);
		if( !g_Window )
			return false;

		glfwMakeContextCurrent(g_Window);
		glfwSwapInterval(1); // Enable vsync
		static bool glad_inited = false;
		if( !glad_inited ) {
			glad_inited = true;
			gl3wInit();
		}

		glfwGetWindowSize(g_Window, &width, &height);
		glViewport(0, 0, width, height);

		// Setup ImGui binding
		glfwSetWindowUserPointer(g_Window, this);
		do_create_context();

		ImGui_ImplGlfw_InitForOpenGL(g_Window, false);
		glfwSetMouseButtonCallback(g_Window, ImGui_ImplGlfw_MouseButtonCallback);
		glfwSetScrollCallback(g_Window, ImGui_ImplGlfw_ScrollCallback);
		glfwSetKeyCallback(g_Window, ImGui_Puss_KeyCallback);
		glfwSetCharCallback(g_Window, ImGui_ImplGlfw_CharCallback);
		glfwSetDropCallback(g_Window, ImGui_Puss_DropCallback);

		#if __APPLE__
			ImGui_ImplOpenGL3_Init("#version 150");
		#else
			ImGui_ImplOpenGL3_Init("#version 130");
		#endif
		return true;
	}

	void destroy_window() {
		if( g_Window ) {
			GLFWwindow* win = g_Window;
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext(g_Context);
			g_Window = NULL;
			glfwDestroyWindow(win);
		}
	}

	bool prepare_newframe() {
		if( !g_Window )
			return false;
		ImGui::SetCurrentContext(g_Context);

		// input
		glfwPollEvents();
 
		ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
		return true;
	}

	bool prepare_render() {
		if( !g_Window )
			return false;
		// render
		glfwMakeContextCurrent(g_Window);
		int width, height;
		glfwGetFramebufferSize(g_Window, &width, &height);

		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		return true;
	}

	void render_drawdata() {
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		do_update_and_render_viewports();
        glfwMakeContextCurrent(g_Window);
        glfwSwapBuffers(g_Window);
	}
#endif

public:
	ImVector<char>*         g_DropFiles;

	// Script data
	lua_State*				g_UpdateLuaState;
	int                     g_ScriptErrorHandle;
	int                     g_StackProtected;
	ImVector<char>*         g_Stack;
};

#include "scintilla_imgui.h"

#if defined(__GNUC__)
	#pragma GCC diagnostic ignored "-Wunused-function"          // warning: 'xxxx' defined but not used
#endif


static inline void _wrap_stack_begin(char tp) {
	ImguiEnv* env = (ImguiEnv*)(ImGui::GetIO().UserData);
	if( env ) {
		env->g_Stack->push_back(tp);
	} else {
		IM_ASSERT(0 && "stack push must have current context!");
	}
}

static inline void _wrap_stack_end(char tp) {
	ImguiEnv* env = (ImguiEnv*)(ImGui::GetIO().UserData);
	if( env ) {
		int top_type = (env->g_Stack->size() > env->g_StackProtected) ? env->g_Stack->back() : -1;
		if( top_type==tp ) {
			env->g_Stack->pop_back();
		} else {
			IM_ASSERT(0 && "stack pop type not matched!");
		}
	} else {
		IM_ASSERT(0 && "stack pop must have current context!");
	}
}

int puss_imgui_assert_hook(const char* expr, const char* file, int line) {
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	ImguiEnv* env = (ImguiEnv*)(ctx ? ctx->IO.UserData : NULL);
	if( env && env->g_UpdateLuaState ) {
		luaL_error(env->g_UpdateLuaState, "%s @%s:%d", expr, file, line);
	} else {
		fprintf(stderr, "%s @%s:%d\r\n", expr, file, line);
	}
	return 0;
}

#define IMGUI_LUA_WRAP_STACK_BEGIN(tp)	_wrap_stack_begin(tp);
#define IMGUI_LUA_WRAP_STACK_END(tp)	_wrap_stack_end(tp);
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
	env->destroy_window();

	if( env->g_DropFiles ) {
		delete env->g_DropFiles;
		env->g_DropFiles = NULL;
	}
	if( env->g_ScriptErrorHandle!=LUA_NOREF ) {
		int ref = env->g_ScriptErrorHandle;
		env->g_ScriptErrorHandle = LUA_NOREF;
		luaL_unref(L, LUA_REGISTRYINDEX, ref);
	}
	if( env->g_Stack ) {
		delete env->g_Stack;
		env->g_Stack = NULL;
	}

	return 0;
}

static int imgui_set_should_close_lua(lua_State* L) {
	ImguiEnv* env = (ImguiEnv*)luaL_checkudata(L, 1, IMGUI_MT_NAME);
	int value = lua_toboolean(L, 2);
	env->set_should_close(value);
	return 0;
}

static int imgui_should_close_lua(lua_State* L) {
	ImguiEnv* env = (ImguiEnv*)luaL_checkudata(L, 1, IMGUI_MT_NAME);
	lua_pushboolean(L, env->get_should_close());
	return 1;
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
	luaL_argcheck(L, lua_type(L, 2)==LUA_TFUNCTION, 2, "need function!");
	if( !env->prepare_newframe() )
		return 0;

	env->g_Stack->clear();
	env->g_StackProtected = 0;

	// run
    ImGui::NewFrame();
	env->g_UpdateLuaState = L;
	lua_rawgeti(L, LUA_REGISTRYINDEX, env->g_ScriptErrorHandle);
	lua_replace(L, 1);
	if( lua_pcall(L, lua_gettop(L)-2, 0, 1) )
		lua_pop(L, 1);
	env->g_UpdateLuaState = NULL;

	env->g_DropFiles->clear();
	// check stack
	env->g_StackProtected = 0;
	while( !env->g_Stack->empty() ) {
		int tp = env->g_Stack->back();
		env->g_Stack->pop_back();
		IMGUI_LUA_WRAP_STACK_POP(tp);
	}

	if( env->prepare_render() ) {
		ImGui::Render();
		env->render_drawdata();
		ImGui::EndFrame();
	}
	return 0;
}

static int imgui_protect_pcall_lua(lua_State* L) {
	ImguiEnv* env = (ImguiEnv*)luaL_checkudata(L, 1, IMGUI_MT_NAME);
	int base = env->g_StackProtected;
	int top = env->g_Stack->size();
	env->g_StackProtected = top;
	lua_rawgeti(L, LUA_REGISTRYINDEX, env->g_ScriptErrorHandle);
	lua_replace(L, 1);
	lua_pushboolean(L, lua_pcall(L, lua_gettop(L)-2, LUA_MULTRET, 1)==LUA_OK);
	lua_replace(L, 1);
	env->g_StackProtected = base;
	while( env->g_Stack->size() > top ) {
		int tp = env->g_Stack->back();
		env->g_Stack->pop_back();
		IMGUI_LUA_WRAP_STACK_POP(tp);
	}
	return lua_gettop(L);
}

static int imgui_getio_delta_time_lua(lua_State* L) {
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	lua_pushnumber(L, ctx ? ctx->IO.DeltaTime : 0.0f);
	return 1;
}

static bool shortcut_mod_check(lua_State* L, int arg, bool val) {
	return lua_isnoneornil(L, arg) ? true : (val == ((lua_toboolean(L, arg)!=0)));
}

static int imgui_is_shortcut_pressed_lua(lua_State* L) {
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	int key = (int)luaL_checkinteger(L, 1);
	bool pressed = ctx
		&& shortcut_mod_check(L, 2, ctx->IO.KeyCtrl)
		&& shortcut_mod_check(L, 3, ctx->IO.KeyShift)
		&& shortcut_mod_check(L, 4, ctx->IO.KeyAlt)
		&& shortcut_mod_check(L, 5, ctx->IO.KeySuper)
		&& (key>=0 && key<PUSS_IMGUI_TOTAL_KEY_LAST)
		&& ImGui::IsKeyPressed(key)
		;
	lua_pushboolean(L, pressed ? 1 : 0);
	return 1;
}

static int imgui_fetch_extra_keys_lua(lua_State* L) {
	luaL_checktype(L, 1, LUA_TTABLE);
#define _PUSS_IMGUI_KEY_REG(key)	lua_pushinteger(L, PUSS_IMGUI_KEY_ ## key);	lua_setfield(L, 1, #key);
	#include "puss_module_imgui_keys.inl"
#undef _PUSS_IMGUI_KEY_REG
	return 0;
}

#ifdef PUSS_IMGUI_USE_DX11

	static LRESULT WINAPI PussImguiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		ImguiEnv* env = (ImguiEnv*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if( msg==WM_CREATE ) {
			env = (ImguiEnv*)(((CREATESTRUCTA*)lParam)->lpCreateParams);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)env);
		}
		return env ? env->WndProc(hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam);
	}

	static int win32_inited = 0;

	static void do_platform_init(lua_State* L) {
		if( win32_inited ) return;
		win32_inited = 1;
		// Create application window
		puss_imgui_wc.hInstance = GetModuleHandle(NULL);
		puss_imgui_wc.lpfnWndProc = PussImguiWndProc;
		RegisterClassEx(&puss_imgui_wc);
		memset(_win32_vk_map, 0, sizeof(_win32_vk_map));
		_win32_vk_map[VK_ESCAPE] = PUSS_IMGUI_KEY_ESCAPE;
		_win32_vk_map[VK_RETURN] = PUSS_IMGUI_KEY_ENTER;
		_win32_vk_map[VK_TAB] = PUSS_IMGUI_KEY_TAB;
		_win32_vk_map[VK_BACK] = PUSS_IMGUI_KEY_BACKSPACE;
		_win32_vk_map[VK_INSERT] = PUSS_IMGUI_KEY_INSERT;
		_win32_vk_map[VK_DELETE] = PUSS_IMGUI_KEY_DELETE;
		_win32_vk_map[VK_RIGHT] = PUSS_IMGUI_KEY_RIGHT;
		_win32_vk_map[VK_LEFT] = PUSS_IMGUI_KEY_LEFT;
		_win32_vk_map[VK_DOWN] = PUSS_IMGUI_KEY_DOWN;
		_win32_vk_map[VK_UP] = PUSS_IMGUI_KEY_UP;
		_win32_vk_map[VK_PRIOR] = PUSS_IMGUI_KEY_PAGE_UP;
		_win32_vk_map[VK_NEXT] = PUSS_IMGUI_KEY_PAGE_DOWN;
		_win32_vk_map[VK_HOME] = PUSS_IMGUI_KEY_HOME;
		_win32_vk_map[VK_END] = PUSS_IMGUI_KEY_END;
		_win32_vk_map[VK_CAPITAL] = PUSS_IMGUI_KEY_CAPS_LOCK;
		_win32_vk_map[VK_SCROLL] = PUSS_IMGUI_KEY_SCROLL_LOCK;
		_win32_vk_map[VK_NUMLOCK] = PUSS_IMGUI_KEY_NUM_LOCK;
		_win32_vk_map[VK_PRINT] = PUSS_IMGUI_KEY_PRINT_SCREEN;
		_win32_vk_map[VK_PAUSE] = PUSS_IMGUI_KEY_PAUSE;
		_win32_vk_map[VK_F1] = PUSS_IMGUI_KEY_F1;
		_win32_vk_map[VK_F2] = PUSS_IMGUI_KEY_F2;
		_win32_vk_map[VK_F3] = PUSS_IMGUI_KEY_F3;
		_win32_vk_map[VK_F4] = PUSS_IMGUI_KEY_F4;
		_win32_vk_map[VK_F5] = PUSS_IMGUI_KEY_F5;
		_win32_vk_map[VK_F6] = PUSS_IMGUI_KEY_F6;
		_win32_vk_map[VK_F7] = PUSS_IMGUI_KEY_F7;
		_win32_vk_map[VK_F8] = PUSS_IMGUI_KEY_F8;
		_win32_vk_map[VK_F9] = PUSS_IMGUI_KEY_F9;
		_win32_vk_map[VK_F10] = PUSS_IMGUI_KEY_F10;
		_win32_vk_map[VK_F11] = PUSS_IMGUI_KEY_F11;
		_win32_vk_map[VK_F12] = PUSS_IMGUI_KEY_F12;
		_win32_vk_map[VK_NUMPAD0] = PUSS_IMGUI_KEY_KP_0;
		_win32_vk_map[VK_NUMPAD1] = PUSS_IMGUI_KEY_KP_1;
		_win32_vk_map[VK_NUMPAD2] = PUSS_IMGUI_KEY_KP_2;
		_win32_vk_map[VK_NUMPAD3] = PUSS_IMGUI_KEY_KP_3;
		_win32_vk_map[VK_NUMPAD4] = PUSS_IMGUI_KEY_KP_4;
		_win32_vk_map[VK_NUMPAD5] = PUSS_IMGUI_KEY_KP_5;
		_win32_vk_map[VK_NUMPAD6] = PUSS_IMGUI_KEY_KP_6;
		_win32_vk_map[VK_NUMPAD7] = PUSS_IMGUI_KEY_KP_7;
		_win32_vk_map[VK_NUMPAD8] = PUSS_IMGUI_KEY_KP_8;
		_win32_vk_map[VK_NUMPAD9] = PUSS_IMGUI_KEY_KP_9;
		_win32_vk_map[VK_DECIMAL] = PUSS_IMGUI_KEY_KP_DECIMAL;
		_win32_vk_map[VK_DIVIDE] = PUSS_IMGUI_KEY_KP_DIVIDE;
		_win32_vk_map[VK_MULTIPLY] = PUSS_IMGUI_KEY_KP_MULTIPLY;
		_win32_vk_map[VK_SUBTRACT] = PUSS_IMGUI_KEY_KP_SUBTRACT;
		_win32_vk_map[VK_ADD] = PUSS_IMGUI_KEY_KP_ADD;
		_win32_vk_map[VK_OEM_NEC_EQUAL] = PUSS_IMGUI_KEY_KP_EQUAL;
	}

	static VOID CALLBACK dummy_timer_handle(HWND, UINT, UINT_PTR, DWORD) {
	}

	static int imgui_wait_events_lua(lua_State* L) {
		double timeout = luaL_optnumber(L, 1, 0.01);
		UINT_PTR timer = SetTimer(NULL, 0, (UINT)(timeout*1000), dummy_timer_handle);
		WaitMessage();
		KillTimer(NULL, timer);
		return 0;
	}
#else
	static void error_callback(int e, const char *d) {
		fprintf(stderr, "[GFLW] error %d: %s\n", e, d);
	}

	static int glfw_inited = 0;

	static void do_platform_init(lua_State* L) {
		if( !glfw_inited ) {
			glfwSetErrorCallback(error_callback);

			glfw_inited = glfwInit();
    		if( !glfw_inited ) luaL_error(L, "[GFLW] failed to init!\n");
    		atexit(glfwTerminate);

			#if __APPLE__
				// GL 3.2 + GLSL 150
				glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
				glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
				glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
				glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
			#else
				// GL 3.0 + GLSL 130
				glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
				glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
				//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
				//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
			#endif
		}
	}

	static int imgui_wait_events_lua(lua_State* L) {
		double timeout = luaL_optnumber(L, 1, 0.01);
		glfwWaitEventsTimeout(timeout);
		return 0;
	}

#endif

static int imgui_create_lua(lua_State* L) {
	const char* title = luaL_checkstring(L, 1);
	int width = (int)luaL_optinteger(L, 2, 1024);
	int height = (int)luaL_optinteger(L, 3, 768);
	const char* ini_filename = luaL_optstring(L, 4, NULL);
	int has_init_cb = lua_isfunction(L, 5);
	ImguiEnv* env = (ImguiEnv*)lua_newuserdata(L, sizeof(ImguiEnv));
	memset(env, 0, sizeof(ImguiEnv));
	env->g_ScriptErrorHandle = LUA_NOREF;
	env->g_DropFiles = new ImVector<char>();
	env->g_Stack = new ImVector<char>();
	luaL_setmetatable(L, IMGUI_MT_NAME);

	lua_pushcfunction(L, imgui_error_handle_default);
	env->g_ScriptErrorHandle = luaL_ref(L, LUA_REGISTRYINDEX);

#ifdef PUSS_IMGUI_USE_DX11
	lua_pushcfunction(L, _lua_utf8_to_utf16);
	lua_pushvalue(L, 1);
	lua_call(L, 1, 1);
	lua_replace(L, 1);
	if( !env->create_window((const TCHAR*)lua_tostring(L, 1), width, height) )
		luaL_error(L, "create window failed!");
#else
	if( !env->create_window(title, width, height) )
		luaL_error(L, "create window failed!");
#endif

	if( ini_filename ) {
		lua_pushvalue(L, 4);
		luaL_ref(L, LUA_REGISTRYINDEX);
		ImGui::GetIO().IniFilename = ini_filename;
	}

	if( has_init_cb ) {
		lua_pushvalue(L, 5);
		if( lua_pcall(L, 0, 0, 0) ) {
			fprintf(stderr, "[ImGui] init callback error: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}

	// first frame
	if( env->prepare_newframe() ) {
        ImGui::NewFrame();
		ImGui::Render();
        // Update and Render additional Platform Windows
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
		ImGui::EndFrame();
	}

	return 1;
}

static int imgui_get_drop_files_lua(lua_State* L) {
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	ImguiEnv* env = (ImguiEnv*)(ctx ? ctx->IO.UserData : NULL);
	if( env && env->g_DropFiles && !(env->g_DropFiles->empty()) ) {
		lua_pushlstring(L, env->g_DropFiles->Data, env->g_DropFiles->Size);
		return 1;
	}
	return 0;
}

static int add_ttf_font_file_lua(lua_State* L) {
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	const char* fname = luaL_checkstring(L, 1);
	float size_pixel = (float)luaL_checknumber(L, 2);
	const char* language = luaL_optstring(L, 3, NULL);
	const ImWchar* glyph_ranges = NULL;
	ImFont* font = NULL;
	if( !ctx )	luaL_error(L, "ImGui::GetCurrentContext MUST exist!");
	if( language ) {
		if( strcmp(language, "Korean")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesKorean();
		} else if( strcmp(language, "Japanese")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesJapanese();
		} else if( strcmp(language, "Japanese")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesJapanese();
		} else if( strcmp(language, "Chinese")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesChineseFull();
		} else if( strcmp(language, "ChineseSimplified")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesChineseSimplifiedCommon();
		} else if( strcmp(language, "Cyrillic")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesCyrillic();
		} else if( strcmp(language, "Thai")==0 ) {
			glyph_ranges = ctx->IO.Fonts->GetGlyphRangesThai();
		} else {
			luaL_error(L, "Not support font language: %s", language);
		}
	}
	font = ctx->IO.Fonts->AddFontFromFileTTF(fname, size_pixel, NULL, glyph_ranges);
	lua_pushboolean(L, font ? 1 : 0);
	return 1;
}

#include "scintilla_imgui_lua.inl"
#include "scintilla.iface.inl"

void* __scintilla_imgui_os_window(void) {
#ifdef PUSS_IMGUI_USE_DX11
	return g_hWnd;
#elif defined(GLFW_EXPOSE_NATIVE_WIN32)
	ImguiEnv* env = (ImguiEnv*)(ImGui::GetIO().UserData);
	return glfwGetWin32Window(env->g_Window);
#else
	return NULL;
#endif
}

static int im_scintilla_lexers(lua_State* L) {
	return im_scintilla_get_lexers(L, sci_lexers);
}

static luaL_Reg imgui_lua_apis[] =
	{ {"Create", imgui_create_lua}
	, {"WaitEventsTimeout", imgui_wait_events_lua}
	, {"GetDropFiles", imgui_get_drop_files_lua}
	, {"AddFontFromFileTTF", add_ttf_font_file_lua}

	, {"CreateByteArray", byte_array_create}
	, {"CreateFloatArray", float_array_create}
	, {"CreateDockFamily", dock_family_create}
	, {"CreateScintilla", im_scintilla_create}
	, {"GetScintillaLexers", im_scintilla_lexers}

	, {"GetIODeltaTime", imgui_getio_delta_time_lua}
	, {"IsShortcutPressed", imgui_is_shortcut_pressed_lua}
	, {"FetchExtraKeys", imgui_fetch_extra_keys_lua}

#define __REG_WRAP(w,f)	, { #w, f }
	#include "imgui_wraps.inl"
#undef __REG_WRAP
	, {NULL, NULL}
	};

static void lua_register_imgui(lua_State* L) {
	// consts
	puss_push_consts_table(L);
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
			, {"set_should_close", imgui_set_should_close_lua}
			, {"should_close", imgui_should_close_lua}
			, {"set_error_handle", imgui_set_error_handle_lua}
			, {"protect_pcall", imgui_protect_pcall_lua}
			, {NULL, NULL}
			};
		luaL_setfuncs(L, methods, 0);
	}
	lua_setfield(L, -1, "__index");
}

static void lua_register_scintilla(lua_State* L) {
	// consts
	{
		puss_push_consts_table(L);
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
			, {"set",im_scintilla_set_data}
			, {"get",im_scintilla_get_data}
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

PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, PussInterface* puss) {
	__puss_iface__ = puss;
	do_platform_init(L);

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

