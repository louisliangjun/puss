// puss_module_imgui.cpp

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "imgui.h"

#include "puss_plugin.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"

#if defined(__GNUC__)
	#pragma GCC diagnostic ignored "-Wunused-function"          // warning: 'xxxx' defined but not used
#endif

#ifdef _MSC_VER
	#if _MSC_VER >= 1916
		#pragma comment(lib, "legacy_stdio_definitions.lib")
	#endif
#endif

static ImTextureID	image_texture_check(lua_State* L, int arg);
#define IMGUI_LUA_WRAP_CHECK_TEXTURE	image_texture_check
#include "puss_imgui_lua.inl"

static void do_create_context() {
	// Setup ImGui binding
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;
	io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;     // FIXME-DPI: THIS CURRENTLY DOESN'T WORK AS EXPECTED. DON'T USE IN USER APP!
	io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports; // FIXME-DPI

	// Setup style
	ImGui::GetStyle().WindowRounding = 0.0f;
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();
}

static void do_update_and_render_viewports() {
	// Update and Render additional Platform Windows
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

static ImTextureID				g_missingImageTexture = 0;
static ImVector<ImTextureID>	g_imageTextures;	// TODO if need sort & search, or use hash table

static ImTextureID image_texture_create(int w, int h, const void* data);
static void image_texture_destroy(ImTextureID tex);

static void imgui_texture_uninit(void) {
	if (g_missingImageTexture) {
		image_texture_destroy(g_missingImageTexture);
		g_missingImageTexture = 0;
	}
}

static void imgui_texture_init() {
	const uint32_t missing_data[16] =
		{ 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF
		, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF
		, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF
		, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF
		};
	g_missingImageTexture = image_texture_create(4, 4, missing_data);
}

static ImTextureID image_texture_check(lua_State* L, int arg) {
	ImTextureID tex = (ImTextureID)lua_touserdata(L, arg);
	return (tex==g_missingImageTexture || g_imageTextures.contains(tex)) ? tex : g_missingImageTexture;
}

static int imgui_image_texture_create_lua(lua_State* L) {
	size_t size = 0;
	const char* data = luaL_checklstring(L, 1, &size);
	int w, h, n;
	stbi_uc* img = stbi_load_from_memory((const stbi_uc*)data, (int)size, &w, &h, &n, 4);
	if( !img )
		luaL_error(L, "stb load image failed: %s", stbi_failure_reason());
	ImTextureID res = image_texture_create(w, h, img);
	stbi_image_free(img);
	if( !res )
		luaL_error(L, "stb create texture failed!");
	g_imageTextures.push_back(res);
	lua_pushlightuserdata(L, res);
	lua_pushinteger(L, w);
	lua_pushinteger(L, h);
	return 3;
}

static int imgui_image_texture_destroy_lua(lua_State* L) {
	ImTextureID tex = (ImTextureID)lua_touserdata(L, 1);
	if( tex!=g_missingImageTexture ) {
		ImTextureID* it = g_imageTextures.begin();
		ImTextureID* end = g_imageTextures.end();
		for( ; it<end; ++it ) {
			if( *it==tex ) {
				g_imageTextures.erase(it);
				image_texture_destroy(tex);
				break;
			}
		}
	}
	return 0;
}

#if defined(PUSS_IMGUI_USE_DX11)

#include <windows.h>
#include <tchar.h>
#include <winuser.h>
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

// #include "resource.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#define PUSS_IMGUI_WINDOW_CLASS	_T("PussImGuiWindow")

static WNDCLASSEX puss_imgui_wc = { sizeof(WNDCLASSEX), CS_CLASSDC, NULL, 0L, 0L, NULL, NULL, NULL, NULL, NULL, PUSS_IMGUI_WINDOW_CLASS, NULL };

static ID3D11Device*				g_pd3dDevice = NULL;
static ID3D11DeviceContext*			g_pd3dDeviceContext = NULL;

static HWND							g_Window = 0;
static IDXGISwapChain*				g_pSwapChain = NULL;
static ID3D11RenderTargetView*		g_mainRenderTargetView = NULL;
static LPARAM						g_resizeParam = 0;

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

static void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

static void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

static HRESULT CreateSwapChain(HWND hWnd)
{
    // Get factory from device
    IDXGIDevice* pDXGIDevice = NULL;
    IDXGIAdapter* pDXGIAdapter = NULL;
    IDXGIFactory* pFactory = NULL;
    if( g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pDXGIDevice)) == S_OK
        && pDXGIDevice->GetParent(IID_PPV_ARGS(&pDXGIAdapter)) == S_OK
		&& pDXGIAdapter->GetParent(IID_PPV_ARGS(&pFactory)) == S_OK )
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
		pFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);
    }
	if (pDXGIDevice) pDXGIDevice->Release();
	if (pDXGIAdapter) pDXGIAdapter->Release();
	if (pFactory) pFactory->Release();
	if( g_pSwapChain ) {
		CreateRenderTarget();
	}
	return S_OK;
}

static void CleanupSwapChain()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
}

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if( ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) )
		return 1;

	switch (msg) {
	case WM_CREATE:
		// Initialize Direct3D
		if (CreateSwapChain(hWnd) < 0) {
			CleanupSwapChain();
		}
		break;
	case WM_CLOSE:
		if (ImGui::GetCurrentContext()) {
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			if (viewport) {
				viewport->PlatformRequestClose = true;
			}
		}
		return 1;
	case WM_DESTROY:
		// PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
			g_resizeParam = lParam;
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DPICHANGED:
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports) {
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
			g_DropFiles.resize(len*3+1);
			int res = WideCharToMultiByte(CP_UTF8, 0, buf, len, g_DropFiles.begin(), (int)(g_DropFiles.size()), NULL, NULL);
			free(buf);
			buf = NULL;

			if( res > 0 ) {
				POINT pos;
				if( GetCursorPos(&pos) ) {
					g_DropFiles.resize(res);
					g_DropPos = ImVec2((float)(pos.x), (float)(pos.y));
				}
			} else {
				g_DropFiles.clear();
			}
			DragFinish(h);
		}
		break;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

static bool create_window(const TCHAR* title, int width, int height) {
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
	HWND hwnd = CreateWindow(PUSS_IMGUI_WINDOW_CLASS, title, WS_OVERLAPPEDWINDOW, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, NULL, NULL, puss_imgui_wc.hInstance, NULL);

	// Show the window
	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);
	DragAcceptFiles(hwnd, TRUE);
	do_create_context();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
	g_Window = hwnd;
	return true;
}

static void destroy_window() {
	if (!g_Window)
		return;
	CleanupSwapChain();
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	DestroyWindow(g_Window);
}

static bool prepare_newframe() {
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

	if( g_resizeParam ) { 
		LPARAM lParam = g_resizeParam;
		g_resizeParam = 0;
		ImGui_ImplDX11_InvalidateDeviceObjects();
		CleanupRenderTarget();
		g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
		CreateRenderTarget();
		ImGui_ImplDX11_CreateDeviceObjects();
	}

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

static bool prepare_render() {
	return g_pSwapChain!=NULL;
}

static void render_drawdata() {
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	do_update_and_render_viewports();
	g_pSwapChain->Present(1, 0); // Present with vsync
    //g_pSwapChain->Present(0, 0); // Present without vsync
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

static ImTextureID image_texture_create(int w, int h, const void* data) {
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	
	ID3D11Texture2D* pTexture = NULL;
	D3D11_SUBRESOURCE_DATA subResource;
	subResource.pSysMem = data;
	subResource.SysMemPitch = desc.Width * 4;
	subResource.SysMemSlicePitch = 0;
	g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

	// Create texture view
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	ID3D11ShaderResourceView* pTextureView = NULL;
	g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &pTextureView);
	pTexture->Release();

	return (ImTextureID)pTextureView;
}

static int imgui_image_destroy(lua_State* L) {
	ID3D11ShaderResourceView* pTextureView = NULL;
	if( pTextureView ) {
		pTextureView->Release();
		pTextureView = NULL;
	}
}

static void image_texture_destroy(ImTextureID tex) {
	ID3D11ShaderResourceView* pTextureView = (ID3D11ShaderResourceView*)tex;
	if (pTextureView)
		pTextureView->Release();
}

static void do_platfrom_uninit() {
	imgui_texture_uninit();
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

static void do_platform_init(lua_State* L) {
	if( g_pd3dDevice )
		return;
	// Create D3DDevice
	UINT createDeviceFlags = 0;
	// createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	HRESULT hRes = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if ( hRes != S_OK)
		luaL_error(L, "D3D11CreateDevice Failed!");
	imgui_texture_init();
}

static int _win32_vk_map[256];

int _win32_vk_convert(WPARAM wParam, LPARAM lParam) {
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

static void _win32_vk_map_init() {
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

static  BOOL CALLBACK on_enum_icons(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName, LONG_PTR lParam) {
	HICON icon = LoadIconW(hModule, lpName);
	if( !icon )
		return TRUE;
	puss_imgui_wc.hIcon = icon;
	return FALSE;
}

BOOL WINAPI DllMain(HANDLE hinstDLL, DWORD dwReason, LPVOID lpvReserved) {
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		// Create application window
		puss_imgui_wc.lpfnWndProc = WndProc;
		puss_imgui_wc.hInstance = (HINSTANCE)hinstDLL;
		// puss_imgui_wc.hIcon = LoadIcon(puss_imgui_wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
		puss_imgui_wc.hIcon = LoadIconW(GetModuleHandle(NULL), L"IDI_PUSS");
		if( !puss_imgui_wc.hIcon )
			EnumResourceNamesW(GetModuleHandle(NULL), RT_GROUP_ICON, on_enum_icons, NULL);
		RegisterClassEx(&puss_imgui_wc);
		_win32_vk_map_init();
		break;
	case DLL_PROCESS_DETACH:
		UnregisterClass(PUSS_IMGUI_WINDOW_CLASS, (HINSTANCE)hinstDLL);
		break;
	}
	return TRUE;
}

#elif defined(PUSS_IMGUI_USE_GLFW)

#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"

#ifdef _MSC_VER
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

static GLFWwindow* g_HideWindow = NULL;
static GLFWwindow* g_Window = NULL;

static void puss_imgui_key_set(ImGuiIO& io, int key, bool st) {
	if( ((unsigned)key) <= PUSS_IMGUI_BASIC_KEY_LAST ) {
		io.KeysDown[key] = st;
	} else {
		switch(key) {
		#define _PUSS_IMGUI_KEY_REG(key)	case GLFW_KEY_ ## key: io.KeysDown[PUSS_IMGUI_KEY_ ## key] = st;	break;
			#include "puss_imgui_keys.inl"
		#undef _PUSS_IMGUI_KEY_REG
		default:
			break;
		}
	}
}

static void ImGui_Puss_WindowCloseCallback(GLFWwindow* window) {
	ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(window);
    if( viewport ) {
        viewport->PlatformRequestClose = true;
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
	g_DropFiles.reserve(4096);
	g_DropFiles.clear();
	for(int i=0; i<count; ++i) {
		for( const char* p=files[i]; *p; ++p ) {
			g_DropFiles.push_back(*p);
		}
		g_DropFiles.push_back('\n');
	}
	glfwFocusWindow(w);

	{
		int wx = 0;
		int wy = 0;
		double mx = -FLT_MAX;
		double my = -FLT_MAX;
		glfwGetWindowPos(w, &wx, &wy);
		glfwGetCursorPos(w, &mx, &my);
		g_DropPos.x = (float)(wx + mx);
		g_DropPos.y = (float)(wy + my);
	}
}

static bool create_window(const char* title, int width, int height) {
    glfwWindowHint(GLFW_VISIBLE, 1);
    glfwWindowHint(GLFW_FOCUSED, 1);
	GLFWwindow* w = glfwCreateWindow(width, height, title, NULL, g_HideWindow);
	if( !w )
		return false;

	glfwMakeContextCurrent(w);
	glfwSwapInterval(0); // Disable vsync

	glfwGetWindowSize(w, &width, &height);
	glViewport(0, 0, width, height);

	// Setup ImGui binding
	do_create_context();

	ImGui_ImplGlfw_InitForOpenGL(w, false);

	glfwSetWindowCloseCallback(w, ImGui_Puss_WindowCloseCallback);
	glfwSetMouseButtonCallback(w, ImGui_ImplGlfw_MouseButtonCallback);
	glfwSetScrollCallback(w, ImGui_ImplGlfw_ScrollCallback);
	glfwSetKeyCallback(w, ImGui_Puss_KeyCallback);
	glfwSetCharCallback(w, ImGui_ImplGlfw_CharCallback);
	glfwSetDropCallback(w, ImGui_Puss_DropCallback);

	#if __APPLE__
		ImGui_ImplOpenGL3_Init("#version 150");
	#else
		ImGui_ImplOpenGL3_Init("#version 130");
	#endif
	g_Window = w;
	return true;
}

static void destroy_window() {
	if( !g_Window )
		return;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(g_Window);
	g_Window = NULL;
}

static bool prepare_newframe() {
	// input
	glfwPollEvents();

	if( !ImGui::GetCurrentContext() )
		return false;

	ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
	return true;
}

static bool prepare_render() {
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

static void render_drawdata() {
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	do_update_and_render_viewports();
    glfwMakeContextCurrent(g_Window);
    glfwSwapBuffers(g_Window);
}

static void error_callback(int e, const char *d) {
	fprintf(stderr, "[GFLW] error %d: %s\n", e, d);
}

static int glfw_inited = 0;

static void do_platfrom_uninit(void) {
	if( glfw_inited ) {
		glfw_inited = 0;
		glfwMakeContextCurrent(g_HideWindow);
		imgui_texture_uninit();
		glfwDestroyWindow(g_HideWindow);
		glfwTerminate();
	}
}

static void do_platform_init(lua_State* L) {
	if( glfw_inited )
		return;

	glfwSetErrorCallback(error_callback);

	glfw_inited = glfwInit();
	if( !glfw_inited ) luaL_error(L, "[GFLW] failed to init!\n");

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
	
    glfwWindowHint(GLFW_VISIBLE, 0);
    glfwWindowHint(GLFW_FOCUSED, 0);
	g_HideWindow = glfwCreateWindow(100, 100, "GlfwHideWindow", NULL, NULL);
	glfwMakeContextCurrent(g_HideWindow);
	gl3wInit();
	imgui_texture_init();
}

static int imgui_wait_events_lua(lua_State* L) {
	double timeout = luaL_optnumber(L, 1, 0.01);
	glfwWaitEventsTimeout(timeout);
	return 0;
}

static ImTextureID image_texture_create(int w, int h, const void* data) {
    // Upload texture to graphics system
	GLuint tex = 0;
    GLint last_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, last_texture);
	return (ImTextureID)(intptr_t)tex;
}

static void image_texture_destroy(ImTextureID tex) {
	GLuint id = (GLuint)(intptr_t)tex;
    glDeleteTextures(1, &id);
}

#endif

static int imgui_created_lua(lua_State* L) {
	lua_pushboolean(L, g_Window ? 1 : 0);
	return 1;
}

static int imgui_destroy_lua(lua_State* L) {
	destroy_window();
	g_DropFiles.clear();
	g_Stack.clear();
	return 0;
}

static int imgui_set_should_close_lua(lua_State* L) {
	int value = lua_toboolean(L, 1);
	if( ImGui::GetCurrentContext() ) {
		ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(g_Window);
		if( viewport ) {
			viewport->PlatformRequestClose = value ? true : false;
		}
	}
	return 0;
}

static int imgui_should_close_lua(lua_State* L) {
	if( ImGui::GetCurrentContext() ) {
		ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(g_Window);
		if( viewport ) {
			lua_pushboolean(L, viewport->PlatformRequestClose ? 1 : 0);
			return 1;
		}
	}
	lua_pushboolean(L, 0);
	return 1;
}

static int imgui_update_lua(lua_State* L) {
	luaL_argcheck(L, lua_type(L, 1)==LUA_TFUNCTION, 1, "need function!");
	if( !prepare_newframe() )
		return 0;

	g_Stack.clear();
	g_StackProtected = 0;

	// run
    ImGui::NewFrame();
	g_UpdateLuaState = L;
	imgui_error_handle_push(L);
	lua_insert(L, 1);
	if( lua_pcall(L, lua_gettop(L)-2, 0, 1) )
		lua_pop(L, 1);
	g_UpdateLuaState = NULL;

	g_DropFiles.clear();
	// check stack
	g_StackProtected = 0;
	while( !g_Stack.empty() ) {
		int tp = g_Stack.back();
		g_Stack.pop_back();
		IMGUI_LUA_WRAP_STACK_POP(tp);
	}

	if( prepare_render() ) {
		ImGui::Render();
		render_drawdata();
		ImGui::EndFrame();
	}
	return 0;
}

static int imgui_create_lua(lua_State* L) {
	const char* title = luaL_checkstring(L, 1);
	int width = (int)luaL_optinteger(L, 2, 1024);
	int height = (int)luaL_optinteger(L, 3, 768);
	const char* ini_filename = luaL_optstring(L, 4, NULL);
	int has_init_cb = lua_isfunction(L, 5);

#if defined(PUSS_IMGUI_USE_DX11)
	lua_pushcfunction(L, _lua_utf8_to_utf16);
	lua_pushvalue(L, 1);
	lua_call(L, 1, 1);
	lua_replace(L, 1);
	if( !create_window((const TCHAR*)lua_tostring(L, 1), width, height) )
		luaL_error(L, "create window failed!");
#elif defined(PUSS_IMGUI_USE_GLFW)
	if( !create_window(title, width, height) )
		luaL_error(L, "create window failed!");
#else
	luaL_error(L, "create window unknown platform!");
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
	if( prepare_newframe() ) {
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

	return 0;
}

void* __scintilla_imgui_os_window(void) {
	#if defined(GLFW_EXPOSE_NATIVE_WIN32)
		return glfwGetWin32Window(g_Window);
	#endif
	return g_Window;
}

static luaL_Reg imgui_plat_lua_apis[] =
	{ {"created", imgui_created_lua}
	, {"create", imgui_create_lua}
	, {"destroy", imgui_destroy_lua}
	, {"update", imgui_update_lua}
	, {"set_should_close", imgui_set_should_close_lua}
	, {"should_close", imgui_should_close_lua}
	, {"wait_events", imgui_wait_events_lua}
	, {"create_image", imgui_image_texture_create_lua}
	, {"destroy_image", imgui_image_texture_destroy_lua}
	, {NULL, NULL}
	};

#define IMGUI_LIB_NAME	"PussImguiLib"

static int puss_imgui_lib_gc(lua_State* L) {
	do_platfrom_uninit();
	return 0;
}

PussInterface* __puss_iface__ = NULL;

PUSS_PLUGIN_EXPORT int __puss_plugin_init__(lua_State* L, PussInterface* puss) {
	__puss_iface__ = puss;
	do_platform_init(L);

	if( lua_getfield(L, LUA_REGISTRYINDEX, IMGUI_LIB_NAME)==LUA_TTABLE )
		return 1;
	lua_pop(L, 1);

	puss_push_consts_table(L);
	lua_register_imgui_consts(L);
	lua_pop(L, 1);

	lua_register_scintilla(L);

	lua_createtable(L, 0, (sizeof(imgui_plat_lua_apis) + sizeof(imgui_lua_apis)) / sizeof(luaL_Reg) - 2);
	luaL_setfuncs(L, imgui_plat_lua_apis, 0);
	luaL_setfuncs(L, imgui_lua_apis, 0);

	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, puss_imgui_lib_gc);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);

	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, IMGUI_LIB_NAME);
	return 1;
}

