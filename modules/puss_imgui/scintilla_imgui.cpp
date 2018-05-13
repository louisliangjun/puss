// scintilla_imgui.cpp

#include "scintilla_imgui.h"

#if defined(__WIN32__) || defined(_MSC_VER)
	#include <windows.h>
#else
	#include <sys/time.h>
	unsigned int GetCurrentTime(void) {
		struct timeval tv;
		uint64_t ret;
		gettimeofday(&tv, 0);
		ret = tv.tv_sec;
		ret *= 1000;
		ret += (tv.tv_usec/1000);
		return (unsigned int)ret;
	}
#endif

#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <cmath>

#include <stdexcept>
#include <new>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <memory>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "Platform.h"

#include "ILexer.h"
#include "Scintilla.h"
#ifdef SCI_LEXER
#include "SciLexer.h"
#endif
#include "StringCopy.h"
#ifdef SCI_LEXER
#include "LexerModule.h"
#endif
#include "Position.h"
#include "UniqueString.h"
#include "SplitVector.h"
#include "Partitioning.h"
#include "RunStyles.h"
#include "ContractionState.h"
#include "CellBuffer.h"
#include "CallTip.h"
#include "KeyMap.h"
#include "Indicator.h"
#include "XPM.h"
#include "LineMarker.h"
#include "Style.h"
#include "ViewStyle.h"
#include "CharClassify.h"
#include "Decoration.h"
#include "CaseFolder.h"
#include "Document.h"
#include "CaseConvert.h"
#include "UniConversion.h"
#include "UnicodeFromUTF8.h"
#include "Selection.h"
#include "PositionCache.h"
#include "EditModel.h"
#include "MarginView.h"
#include "EditView.h"
#include "Editor.h"
#include "AutoComplete.h"
#include "ScintillaBase.h"

#ifdef SCI_LEXER
#include "ExternalLexer.h"
#endif

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

// X has a 16 bit coordinate space, so stop drawing here to avoid wrapping
static const int maxCoordinate = 32000;

static const double kPi = 3.14159265358979323846;

static const XYPOSITION kDefaultFontSize = 7.0f;

static inline XYPOSITION XYPositionMin(XYPOSITION a, XYPOSITION b) {
	return a < b ? a : b;
}

Font::Font() : fid(0) {}

Font::~Font() {}

void Font::Create(const FontParameters &fp) {
	Release();
	fid = ImGui::GetFont();
}

void Font::Release() {
	if( fid ) {
		fid = 0;
	}
}

class WindowIM {
public:
	WindowIM() : win(0) {
	}
public:
	ImGuiWindow*	win;
};

class SurfaceImpl : public Surface {
	bool inited;
	ImVec2 offset;
	ImU32 pen_color;
	ImU32 bg_color;
	float line_thickness;
	ImVec2 move_to;
private:
	void DoClear() {
		inited = false;
		offset = ImVec2(0.0f, 0.0f);
		pen_color = IM_COL32_BLACK;
		bg_color = IM_COL32_WHITE;
		line_thickness = 1.0f;
		move_to = ImVec2(0.0f, 0.0f);
	}
public:
	SurfaceImpl()
		: inited(false)
		, pen_color(IM_COL32_BLACK)
		, bg_color(IM_COL32_WHITE)
		, line_thickness(1.0f)
		, move_to(0.0f, 0.0f)
	{
	}
	~SurfaceImpl() override {
		DoClear();
	}
	void Init(WindowID wid) override {
		WindowIM* w = (WindowIM*)wid;
		PLATFORM_ASSERT(w);
		if( w->win ) {
			offset = w->win->Pos;
		}
		inited = true;
	}
	void Init(SurfaceID sid, WindowID wid) override {
		Init(wid);
	}
	void InitPixMap(int width_, int height_, Surface *surface_, WindowID wid) override {
		PLATFORM_ASSERT(surface_);
		Release();
		//SurfaceImpl *surfImpl = static_cast<SurfaceImpl *>(surface_);
		PLATFORM_ASSERT(wid);
		// TODO : image or glfw3 render target ??
		Init(0, wid);
	}
	void Release() override {
		DoClear();
	}
	bool Initialised() override {
		return inited;
	}
	inline ImU32 SetPenColour(ColourDesired rgb, ImU32 a=255) {
		pen_color = IM_COL32(rgb.GetRed(), rgb.GetGreen(), rgb.GetBlue(), a);
		return pen_color;
	}
	void PenColour(ColourDesired fore) override {
		pen_color = IM_COL32(fore.GetRed(), fore.GetGreen(), fore.GetBlue(), 255);
	}
	int LogPixelsY() override {
		return 72;
	}
	int DeviceHeightFont(int points) override {
		int logPix = LogPixelsY();
		return (points * logPix + logPix / 2) / 72;
	}
	void MoveTo(int x_, int y_) override {
		move_to.x = (float)x_;
		move_to.y = (float)y_;
	}
	void LineTo(int x_, int y_) override {
		ImVec2 pos = move_to;
		move_to.x = (float)x_;
		move_to.y = (float)y_;
		ImGui::GetWindowDrawList()->AddLine(pos, move_to, pen_color, line_thickness);
	}
	void Polygon(Point *pts, int npts, ColourDesired fore, ColourDesired back) override {
		ImDrawList* canvas = ImGui::GetWindowDrawList();
		canvas->PathClear();
		for( int i=0; i<npts; ++i ) {
			canvas->PathLineTo(ImVec2(offset.x + pts[i].x, offset.y + pts[i].y));
		}
		canvas->AddConvexPolyFilled(canvas->_Path.Data, canvas->_Path.Size, SetPenColour(back));
		canvas->PathStroke(SetPenColour(fore), true, line_thickness);
	}
	void RectangleDraw(PRectangle rc, ColourDesired fore, ColourDesired back) override {
		ImDrawList* canvas = ImGui::GetWindowDrawList();
		ImVec2 a(offset.x + rc.left + 0.5f, offset.y + rc.top + 0.5f);
		ImVec2 b(offset.x + rc.right - 1.0f, offset.y + rc.bottom - 1.0f);
		canvas->AddRectFilled(a, b, IM_COL32(back.GetRed(),  back.GetGreen(), back.GetBlue(), 255));
		pen_color = IM_COL32(fore.GetRed(),  fore.GetGreen(), fore.GetBlue(), 255);
		canvas->PathRect(a, b);
		canvas->PathStroke(pen_color, true, line_thickness);
	}
	void FillRectangle(PRectangle rc, ColourDesired back) override {
		ImDrawList* canvas = ImGui::GetWindowDrawList();
		ImVec2 a(offset.x + rc.left, offset.y + rc.top);
		ImVec2 b(offset.x + rc.right, offset.y + rc.bottom);
		ImU32 alpha = 255;	// 32;	// for debug
#if 1
		canvas->AddRectFilled(a, b, IM_COL32(back.GetRed(),  back.GetGreen(), back.GetBlue(), alpha));
#else
		canvas->PathRect(a, b);
		canvas->PathStroke(IM_COL32(255,  0, 0, 32), true, line_thickness);
#endif
	}
	void FillRectangle(PRectangle rc, Surface &surfacePattern) override {
		// SurfaceImpl &surfi = static_cast<SurfaceImpl &>(surfacePattern);
		bool canDraw = false; // TODO : surfi.psurf != NULL;
		if( canDraw ) {
			ImDrawList* canvas = ImGui::GetWindowDrawList();
			// Tile pattern over rectangle
			// Currently assumes 8x8 pattern
			int widthPat = 8;
			int heightPat = 8;
			int left = (int)rc.left;
			int right = (int)rc.right;
			int top = (int)rc.top;
			int bottom = (int)rc.bottom;
			for (int xTile = left; xTile < right; xTile += widthPat) {
				int widthx = (xTile + widthPat > right) ? right - xTile : widthPat;
				for (int yTile = top; yTile < bottom; yTile += heightPat) {
					int heighty = (yTile + heightPat > bottom) ? bottom - yTile : heightPat;
					// cairo_set_source_surface(context, surfi.psurf, xTile, yTile);
					ImVec2 a(offset.x + xTile, offset.y + yTile);
					ImVec2 b(offset.x + (float)widthx, offset.y + (float)heighty);
					canvas->AddRectFilled(a, b, pen_color);
				}
			}
		} else {
			// Something is wrong so try to show anyway
			// Shows up black because colour not allocated
			FillRectangle(rc, 0);
		}
	}
	void RoundedRectangle(PRectangle rc, ColourDesired fore, ColourDesired back) override {
		if (((rc.right - rc.left) > 4) && ((rc.bottom - rc.top) > 4)) {
			// Approximate a round rect with some cut off corners
			Point pts[] = {
							  Point(rc.left + 2, rc.top),
							  Point(rc.right - 2, rc.top),
							  Point(rc.right, rc.top + 2),
							  Point(rc.right, rc.bottom - 2),
							  Point(rc.right - 2, rc.bottom),
							  Point(rc.left + 2, rc.bottom),
							  Point(rc.left, rc.bottom - 2),
							  Point(rc.left, rc.top + 2),
						  };
			Polygon(pts, ELEMENTS(pts), fore, back);
		} else {
			RectangleDraw(rc, fore, back);
		}
	}
	void AlphaRectangle(PRectangle rc, int cornerSize, ColourDesired fill, int alphaFill,
		ColourDesired outline, int alphaOutline, int flags) override
	{
		if (rc.Width() > 0) {
			ImDrawList* canvas = ImGui::GetWindowDrawList();
			ImVec2 a(offset.x + rc.left + 1.0f, offset.y + rc.top + 1.0f);
			ImVec2 b(offset.x + rc.right - 2.0f, offset.y + rc.bottom - 2.0f);
			pen_color = IM_COL32(fill.GetRed(),  fill.GetGreen(), fill.GetBlue(), alphaFill);
			canvas->AddRectFilled(a, b, pen_color, (float)cornerSize, flags);
			
			pen_color = IM_COL32(outline.GetRed(),  outline.GetGreen(), outline.GetBlue(), alphaOutline);
			a.x -= 0.5f;	a.y -= 0.5f;
			b.x += 1.0f;	b.y += 1.0f;
			canvas->PathRect(a, b, (float)cornerSize, flags);
			canvas->PathStroke(pen_color, true, line_thickness);
			canvas->PathStroke(pen_color, true, line_thickness);
		}
	}
	void DrawRGBAImage(PRectangle rc, int width, int height, const unsigned char *pixelsImage) override {
		// TODO : nk_draw_image();
	}
	void Ellipse(PRectangle rc, ColourDesired fore, ColourDesired back) override {
		ImDrawList* canvas = ImGui::GetWindowDrawList();
		float cx = offset.x + (rc.left + rc.right) / 2;
		float cy = offset.y + (rc.top + rc.bottom) / 2;
		float w = rc.Width();
		float h = rc.Height();
		float r = (w < h) ? w : h;
		canvas->PathFillConvex(SetPenColour(back));
		canvas->PathArcTo(ImVec2(cx, cy), r, 0.0f, (float)(2*kPi));
		canvas->PathStroke(SetPenColour(fore), true, line_thickness);
	}
	void Copy(PRectangle rc, Point from, Surface &surfaceSource) override {
		//// SurfaceImpl &surfi = static_cast<SurfaceImpl &>(surfaceSource);
		//bool canDraw = false; // TODO : surfi.psurf != NULL;
		//if( canDraw ) {
		//	struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
		//	struct nk_rect rect = __nk_rect(rc.left, rc.top, rc.Width(), rc.Height());
		//	struct nk_color c = { 255, 255, 0, 255 };
		//	nk_fill_rect(out, rect, 0.0f, c);
		//	// cairo_set_source_surface(context, surfi.psurf, rc.left - from.x, rc.top - from.y);
		//	// cairo_rectangle(context, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top);
		//	// cairo_fill(context);
		//}
	}

public:
	void DrawTextBase(PRectangle rc, Font &font_, XYPOSITION ybase, const char *s, int len, ColourDesired fore) {
		ImDrawList* canvas = ImGui::GetWindowDrawList();
		ImVec2 pos(offset.x + rc.left, offset.y + rc.top);
		ImVec4 rect(pos.x, pos.y, offset.x + rc.right, offset.y + rc.bottom);
		ImFont* font = (ImFont*)(font_.fid);
		if( font ) {
			canvas->AddText(font, font->FontSize, pos, SetPenColour(fore), s, s+len, 0.0f, &rect);
		}
#if 0
		canvas->PathRect(ImVec2(rect.x, rect.y), ImVec2(rect.z, rect.w));
		canvas->PathStroke(IM_COL32(0, 255, 0, 32), true, line_thickness);
#endif
	}
	void DrawTextNoClip(PRectangle rc, Font &font_, XYPOSITION ybase, const char *s, int len, ColourDesired fore, ColourDesired back) override {
		FillRectangle(rc, back);
		DrawTextBase(rc, font_, ybase, s, len, fore);
	}
	void DrawTextClipped(PRectangle rc, Font &font_, XYPOSITION ybase, const char *s, int len, ColourDesired fore, ColourDesired back) override {
		FillRectangle(rc, back);
		DrawTextBase(rc, font_, ybase, s, len, fore);
	}
	void DrawTextTransparent(PRectangle rc, Font &font_, XYPOSITION ybase, const char *s, int len, ColourDesired fore) override {
		// Avoid drawing spaces in transparent mode
		for (int i=0; i<len; i++) {
			if (s[i] != ' ') {
				DrawTextBase(rc, font_, ybase, s, len, fore);
				return;
			}
		}
	}
	static XYPOSITION DoMeasureWidths(Font &font_, const char *s, int len, XYPOSITION *positions) {
		ImFont* font = (ImFont*)(font_.fid);
		XYPOSITION w = 0.0f;
		unsigned int wch = 0;
		float scale = font ? font->Scale : 1.0f;
		const char *e = s + len;
		while( s < e ) {
			int glyph_len = ImTextCharFromUtf8(&wch, s, e);
			if( glyph_len==0 )
				break;
			if( !wch )
				break;
			if( font ) {
				const ImFontGlyph* g = font->FindGlyph((ImWchar)wch);
				w += g->AdvanceX * scale;
			} else {
				w += kDefaultFontSize;
			}
			if( positions ) {
				for(int i=0; i<glyph_len; ++i) {
					*positions++ = w;
				}
			}
			s += glyph_len;
		}
		return w;
	}
	void MeasureWidths(Font &font_, const char *s, int len, XYPOSITION *positions) override {
		DoMeasureWidths(font_, s, len, positions);
	}
	XYPOSITION WidthText(Font &font_, const char *s, int len) override {
		return DoMeasureWidths(font_, s, len, NULL);
	}
	XYPOSITION WidthChar(Font &font_, char ch) override {
		return DoMeasureWidths(font_, &ch, 1, NULL);
	}
	XYPOSITION Ascent(Font &font_) override {
		ImFont* font = (ImFont*)(font_.fid);
		return font ? font->Ascent : 1.0f;
	}
	XYPOSITION Descent(Font &font_) override {
		ImFont* font = (ImFont*)(font_.fid);
		return font ? -(font->Descent) : 1.0f;
	}
	XYPOSITION InternalLeading(Font &font_) override {
		return 0.0f;
	}
	XYPOSITION Height(Font &font_) override {
		ImFont* font = (ImFont*)(font_.fid);
		return font ? font->FontSize : 18.0f;
	}
	XYPOSITION AverageCharWidth(Font &font_) override {
		return WidthChar(font_, 'n');
	}

public:
	void SetClip(PRectangle rc) override {
		ImVec2 vmin(offset.x + rc.left, offset.y + rc.top);
		ImVec2 vmax(offset.x + rc.right, offset.y + rc.bottom);
		ImGui::PushClipRect(vmin, vmax, true);
	}
	void FlushCachedState() override {
		// TODO :
	}
	void SetUnicodeMode(bool unicodeMode_) override {
		// only utf-8 support
	}
	void SetDBCSMode(int codePage) override {
		// only utf-8 support
	}
};

Surface *Surface::Allocate(int) {
	return new SurfaceImpl();
}

Window::~Window() {
}

void Window::Destroy() {
}

PRectangle Window::GetPosition() {
	WindowIM* w = (WindowIM*)wid;
	PRectangle rc;
	if( w && w->win ) {
		rc.left = w->win->Pos.x;
		rc.right = rc.left + w->win->Size.x;
		rc.top = w->win->Pos.y;
		rc.bottom = rc.top + w->win->Size.y;
	}
	return rc;
}

void Window::SetPosition(PRectangle rc) {
	WindowIM* w = (WindowIM*)wid;
	if( w && w->win ) {
		w->win->Pos.x = rc.left;
		w->win->Pos.y = rc.top;
		w->win->Size.x = rc.Width();
		w->win->Size.y = rc.Height();
	}
}

void Window::SetPositionRelative(PRectangle rc, Window relativeTo) {
	WindowIM* w = (WindowIM*)wid;
	if( w && w->win ) {
		WindowIM* wndRelativeTo = (WindowIM*)(relativeTo.GetID());
		XYPOSITION ox = 0;
		XYPOSITION oy = 0;
		PRectangle rcMonitor(0, 0, 1920, 1080);
		if( wndRelativeTo && wndRelativeTo->win ) {
			ox = wndRelativeTo->win->Pos.x;
			oy = wndRelativeTo->win->Pos.y;
			rcMonitor.left = ox;
			rcMonitor.top = oy;
			rcMonitor.right = ox + wndRelativeTo->win->Size.x;
			rcMonitor.bottom = oy + wndRelativeTo->win->Size.y;
		}
		ox += rc.left;
		oy += rc.top;

		// do some corrections to fit into screen
		XYPOSITION sizex = rc.Width();
		XYPOSITION sizey = rc.Height();
		if (sizex > rcMonitor.Width() || ox < rcMonitor.left)
			ox = rcMonitor.left; /* the best we can do */
		else if (ox + sizex > rcMonitor.left + rcMonitor.Width())
			ox = rcMonitor.left + rcMonitor.Width() - sizex;
		if (sizey > rcMonitor.Height() || oy < rcMonitor.top)
			oy = rcMonitor.top;
		else if (oy + sizey > rcMonitor.top + rcMonitor.Height())
			oy = rcMonitor.top + rcMonitor.Height() - sizey;

		w->win->Pos.x = ox;
		w->win->Pos.y = oy;
		w->win->Size.x = ox + sizex;
		w->win->Size.y = oy + sizey;
	}
}

PRectangle Window::GetClientPosition() {
	return GetPosition();
}

void Window::Show(bool show) {
}

void Window::InvalidateAll() {
}

void Window::InvalidateRectangle(PRectangle rc) {
}

void Window::SetFont(Font &) {
}

void Window::SetCursor(Cursor curs) {
}

PRectangle Window::GetMonitorRect(Point pt) {
	PRectangle rcMonitor(0, 0, 1920, 1080);
	return rcMonitor;
}

ListBox::ListBox() {
}

ListBox::~ListBox() {
}

class ListBoxX : public ListBox {
public:
	ListBoxX() {
	}
	~ListBoxX() override {
	}
	void SetFont(Font &font) override {
	}
	void Create(Window &parent, int ctrlID, Point location_, int lineHeight_, bool unicodeMode_, int technology_) override {
	}
	void SetAverageCharWidth(int width) override {
	}
	void SetVisibleRows(int rows) override {
	}
	int GetVisibleRows() const override {
		return 0;
	}
	PRectangle GetDesiredRect() override {
		PRectangle rc(0,0,100,100);
		return rc;
	}
	int CaretFromEdge() override {
		return 0;
	}
	void Clear() override {
	}
	void Append(char *s, int type = -1) override {
	}
	int Length() override {
		return 0;
	}
	void Select(int n) override {
	}
	int GetSelection() override {
		return -1;
	}
	int Find(const char *prefix) override {
		return -1;
	}
	void GetValue(int n, char *value, int len) override {
	};
	void RegisterImage(int type, const char *xpm_data) override {
	}
	void RegisterRGBAImage(int type, int width, int height, const unsigned char *pixelsImage) override {
	}
	void ClearRegisteredImages() override {
	}
	void SetDelegate(IListBoxDelegate *lbDelegate) override {
	}
	void SetList(const char *listText, char separator, char typesep) override {
	}
};

ListBox *ListBox::Allocate() {
	ListBoxX *lb = new ListBoxX();
	return lb;
}

Menu::Menu() : mid(0) {
}

void Menu::CreatePopUp() {
	Destroy();
}

void Menu::Destroy() {
	mid = 0;
}

void Menu::Show(Point pt, Window &w) {
}

ColourDesired Platform::Chrome() {
	return ColourDesired(0xe0, 0xe0, 0xe0);
}

ColourDesired Platform::ChromeHighlight() {
	return ColourDesired(0xff, 0xff, 0xff);
}

const char *Platform::DefaultFont() {
#ifdef G_OS_WIN32
	return "Lucida Console";
#else
	return "!Sans";
#endif
}

int Platform::DefaultFontSize() {
#ifdef G_OS_WIN32
	return 10;
#else
	return 12;
#endif
}

unsigned int Platform::DoubleClickTime() {
	return 500; 	// Half a second
}

void Platform::DebugDisplay(const char *s) {
	fprintf(stderr, "%s", s);
}

DynamicLibrary* DynamicLibrary::Load(const char* modulePath) {
	return NULL;
}

bool DynamicLibrary::IsValid() {
	return false;
}

#ifdef _WIN32

static bool initialisedET = false;
static bool usePerformanceCounter = false;
static LARGE_INTEGER frequency;

ElapsedTime::ElapsedTime() {
	if (!initialisedET) {
		usePerformanceCounter = ::QueryPerformanceFrequency(&frequency) != 0;
		initialisedET = true;
	}
	if (usePerformanceCounter) {
		LARGE_INTEGER timeVal;
		::QueryPerformanceCounter(&timeVal);
		bigBit = timeVal.HighPart;
		littleBit = timeVal.LowPart;
	} else {
		bigBit = clock();
		littleBit = 0;
	}
}

double ElapsedTime::Duration(bool reset) {
	double result;
	long endBigBit;
	long endLittleBit;

	if (usePerformanceCounter) {
		LARGE_INTEGER lEnd;
		::QueryPerformanceCounter(&lEnd);
		endBigBit = lEnd.HighPart;
		endLittleBit = lEnd.LowPart;
		LARGE_INTEGER lBegin;
		lBegin.HighPart = bigBit;
		lBegin.LowPart = littleBit;
		const double elapsed = static_cast<double>(lEnd.QuadPart - lBegin.QuadPart);
		result = elapsed / static_cast<double>(frequency.QuadPart);
	} else {
		endBigBit = clock();
		endLittleBit = 0;
		const double elapsed = endBigBit - bigBit;
		result = elapsed / CLOCKS_PER_SEC;
	}
	if (reset) {
		bigBit = endBigBit;
		littleBit = endLittleBit;
	}
	return result;
}

#else

ElapsedTime::ElapsedTime() {
	struct timeval curTime;
	gettimeofday(&curTime, 0);
	bigBit = curTime.tv_sec;
	littleBit = curTime.tv_usec;
}

double ElapsedTime::Duration(bool reset) {
	struct timeval curTime;
	gettimeofday(&curTime, 0);
	long endBigBit = curTime.tv_sec;
	long endLittleBit = curTime.tv_usec;
	double result = 1000000.0 * (endBigBit - bigBit);
	result += endLittleBit - littleBit;
	result /= 1000000.0;
	if (reset) {
		bigBit = endBigBit;
		littleBit = endLittleBit;
	}
	return result;
}

#endif

//#define TRACE

#ifdef TRACE
void Platform::DebugPrintf(const char *format, ...) {
	char buffer[2000];
	va_list pArguments;
	va_start(pArguments, format);
	vsprintf(buffer, format, pArguments);
	va_end(pArguments);
	Platform::DebugDisplay(buffer);
}
#else
void Platform::DebugPrintf(const char *, ...) {
}
#endif

bool Platform::ShowAssertionPopUps(bool assertionPopUps_) {
	return true;
}

void Platform::Assert(const char *c, const char *file, int line) {
	fprintf(stderr, "Assertion [%s] failed at %s %d\r\n", c, file, line);
	abort();
}

class ScintillaIM : public ScintillaBase {
	ScintillaIM(const ScintillaIM &) = delete;
	ScintillaIM &operator=(const ScintillaIM &) = delete;
public:
	ScintillaIM()
		: captureMouse(false)
		, rectangularSelectionModifier(SCMOD_ALT)
	{
		view.bufferedDraw = false;
		wMain = (WindowID)&mainWindow;
	}
	virtual ~ScintillaIM() {
	}
	void DisplayCursor(Window::Cursor c) override {
	}
	bool DragThreshold(Point ptStart, Point ptNow) override {
		return false;
	}
	void StartDrag() override {
	}
	bool ValidCodePage(int codePage) const override {
		return true;
	}
private:
	static sptr_t DirectFunction(sptr_t ptr, unsigned int iMessage, uptr_t wParam, sptr_t lParam) {
		return reinterpret_cast<ScintillaIM*>(ptr)->WndProc(iMessage, wParam, lParam);
	}
public: 	// Public for scintilla_send_message
	sptr_t WndProc(unsigned int iMessage, uptr_t wParam, sptr_t lParam) override {
		try {
			switch (iMessage) {
			case SCI_GRABFOCUS:
				SetFocusState(true);
				break;
			case SCI_GETDIRECTFUNCTION:
				return reinterpret_cast<sptr_t>(DirectFunction);
			case SCI_GETDIRECTPOINTER:
				return reinterpret_cast<sptr_t>(this);
	#ifdef SCI_LEXER
			case SCI_LOADLEXERLIBRARY:
				LexerManager::GetInstance()->Load(reinterpret_cast<const char*>(lParam));
				break;
	#endif
			case SCI_TARGETASUTF8: {
					// only support utf8
					char *text = reinterpret_cast<char*>(lParam);
					int targetLength = targetEnd - targetStart;
					if( text ) {
						pdoc->GetCharRange(text, targetStart, targetLength);
					}
					return targetLength;
				}
			case SCI_SETRECTANGULARSELECTIONMODIFIER:
				rectangularSelectionModifier = (int)wParam;
				break;

			case SCI_GETRECTANGULARSELECTIONMODIFIER:
				return rectangularSelectionModifier;

			default:
				return ScintillaBase::WndProc(iMessage, wParam, lParam);
			}
		} catch (std::bad_alloc&) {
			errorStatus = SC_STATUS_BADALLOC;
		} catch (...) {
			errorStatus = SC_STATUS_FAILURE;
		}
		return 0;
	}
	void HandleMouseEvents(ImGuiIO& io, ImGuiID id, bool hovered, const PRectangle& wRect, unsigned int now, int modifiers) {
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		const bool focus_requested = ImGui::FocusableItemRegister(window, id);
		// const bool focus_requested_by_code = focus_requested && (window->FocusIdxAllCounter == window->FocusIdxAllRequestCurrent);
		// const bool focus_requested_by_tab = focus_requested && !focus_requested_by_code;
		const bool user_clicked = hovered && io.MouseClicked[0];
		const bool user_scrolled = hovered && (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f);

	    if( focus_requested || user_clicked || user_scrolled ) {
			// fprintf(stderr, "grab focus!\n");
			SetFocusState(true);
			ImGui::SetActiveID(id, window);
			ImGui::SetFocusID(id, window);
			ImGui::FocusWindow(window);
		} else if( ImGui::GetActiveID()==id ) {
			ImGui::ClearActiveID();
		}

		/* mouse click handler */
		if( hovered ) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
			if( ImGui::IsMouseDown(0) ) {
				if( ImGui::IsMouseClicked(0) ) {
					Point click_pos(io.MousePos.x - wRect.left, io.MousePos.y - wRect.top);
					ButtonDownWithModifiers(click_pos, now, modifiers);
					// fprintf(stderr, "mouse down\n");
				} else if((io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
					// TODO : drag
				}
			}
		}

		if( ImGui::IsMouseReleased(0) ) {
			// fprintf(stderr, "mouse up\n");
			Point pt(io.MousePos.x - wRect.left, io.MousePos.y - wRect.top);
			ButtonUpWithModifiers(pt, now, modifiers);
		}
		if( hovered && ImGui::IsMouseClicked(1) ) {
			Point pt(io.MousePos.x - wRect.left, io.MousePos.y - wRect.top);
			if(!PointInSelection(pt))
				SetEmptySelection(PositionFromLocation(pt));
			if(ShouldDisplayPopup(pt)) {
				// PopUp menu
				// Convert to screen
				int ox = 0;
				int oy = 0;
				// gdk_window_get_origin(PWindow(wMain), &ox, &oy);
				ContextMenu(Point(pt.x + ox, pt.y + oy));
			} else {
				RightButtonDownWithModifiers(pt, now, modifiers);
			}
		}

		// move
		if((io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
			Point pt(io.MousePos.x - wRect.left, io.MousePos.y - wRect.top);
			ButtonMoveWithModifiers(pt, now, modifiers);
		}
	}
	static bool IsKeyPressedMap(ImGuiKey key, bool repeat = true) {
		const int key_index = GImGui->IO.KeyMap[key];
		return (key_index >= 0) ? ImGui::IsKeyPressed(key_index, repeat) : false;
	}
	void TryKeyDownWithModifiers(ImGuiKey key, int sci_key, int sci_modifiers) {
		if( IsKeyPressedMap(key) ) {
			bool consumed = false;
			KeyDownWithModifiers(sci_key, sci_modifiers, &consumed);
		}
	}
	void HandleKeyboardEvents(ImGuiIO& io, unsigned int now, int modifiers) {
		/* text input */
		if( io.InputCharacters[0] ) {
            // Process text input (before we check for Return because using some IME will effectively send a Return?)
            // We ignore CTRL inputs, but need to allow ALT+CTRL as some keyboards (e.g. German) use AltGR (which _is_ Alt+Ctrl) to input certain characters.
            if( !(io.KeyCtrl && !io.KeyAlt) && (!pdoc->IsReadOnly()) ) {
				char utf8[IM_ARRAYSIZE(io.InputCharacters)*6+8];
				ImWchar* text_end = io.InputCharacters;
                for (int n = 0; n < IM_ARRAYSIZE(io.InputCharacters) && io.InputCharacters[n]; n++) {
					++text_end;
				}
				int len = ImTextStrToUtf8(utf8, IM_ARRAYSIZE(io.InputCharacters)*6, io.InputCharacters, text_end);
				ClearSelection();
				const int inserted = pdoc->InsertString(CurrentPosition(), utf8, len);
				if(inserted > 0) {
					MovePositionTo(CurrentPosition() + inserted);
				}
			}
            // Consume characters
            memset(io.InputCharacters, 0, sizeof(io.InputCharacters));
		}

        // Handle key-presses
        const bool is_shortcut_key_only = (io.OptMacOSXBehaviors ? (io.KeySuper && !io.KeyCtrl) : (io.KeyCtrl && !io.KeySuper)) && !io.KeyAlt && !io.KeyShift; // OS X style: Shortcuts using Cmd/Super instead of Ctrl
        const bool is_ctrl_key_only = io.KeyCtrl && !io.KeyShift && !io.KeyAlt && !io.KeySuper;
        const bool is_shift_key_only = io.KeyShift && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper;

		TryKeyDownWithModifiers(ImGuiKey_LeftArrow, SCK_LEFT, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_RightArrow, SCK_RIGHT, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_UpArrow, SCK_UP, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_DownArrow, SCK_DOWN, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_Home, SCK_HOME, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_End, SCK_END, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_Delete, SCK_DELETE, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_Backspace, SCK_BACK, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_Tab, SCK_TAB, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_Enter, SCK_RETURN, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_Escape, SCK_ESCAPE, modifiers);

		if( (is_shortcut_key_only && IsKeyPressedMap(ImGuiKey_A)) ) { SelectAll(); }
		if( (is_shortcut_key_only && IsKeyPressedMap(ImGuiKey_Z)) ) { Undo(); }
		if( (is_shortcut_key_only && IsKeyPressedMap(ImGuiKey_Y)) || (io.KeyCtrl && io.KeyShift && !io.KeyAlt && !io.KeySuper && IsKeyPressedMap(ImGuiKey_Z)) ) { Redo(); }
		if( (is_shortcut_key_only && IsKeyPressedMap(ImGuiKey_X)) || (is_shift_key_only && IsKeyPressedMap(ImGuiKey_Delete)) ) { Cut(); }
        if( (is_shortcut_key_only && IsKeyPressedMap(ImGuiKey_C)) || (is_ctrl_key_only  && IsKeyPressedMap(ImGuiKey_Insert)) ) { Copy(); }
        if( (is_shortcut_key_only && IsKeyPressedMap(ImGuiKey_V)) || (is_shift_key_only && IsKeyPressedMap(ImGuiKey_Insert)) ) { Paste(); }
    }
	void HandleInputEvents(ImGuiID id, const ImRect& bb) {
		ImGuiIO& io = ImGui::GetIO();
		unsigned int now = GetCurrentTime();
		int modifiers = ModifierFlags(io.KeyShift, io.KeyCtrl, io.KeyAlt, io.KeySuper);
		PRectangle wRect = wMain.GetPosition();
		bool hovered = ImGui::ItemHoverable(bb, id);
		if( hovered || HaveMouseCapture() ) {
			HandleMouseEvents(io, id, hovered, wRect, now, modifiers);
		} else if( hasFocus && ImGui::IsAnyMouseDown() ) {
			SetFocusState(false);
		}
		if( hasFocus ) {
			HandleKeyboardEvents(io, now, modifiers);
		}
	}
	void Update() {
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImVec2 total_sz = window->Pos + window->Size - window->DC.CursorPos - (window->WindowPadding * 2.0f);
		ImRect frame_bb(window->DC.CursorPos, (window->DC.CursorPos + total_sz));
		float totalHeight = (float)((pdoc->LinesTotal() + 1) * vs.lineHeight);
		float totalWidth = (float)(scrollWidth * vs.aveCharWidth);
		ImVec2 frame_sz = total_sz;
		horizontalScrollBarVisible = (totalWidth > frame_sz.x);
		verticalScrollBarVisible = (totalHeight > frame_sz.y);
		ImGuiID id = window->GetID(this);
		if( ImGui::BeginChildFrame(id, frame_sz, ImGuiWindowFlags_HorizontalScrollbar)) {
			window = ImGui::GetCurrentWindow();
			mainWindow.win = window;

			// input
			HandleInputEvents(id, frame_bb);

			// scrollbar
			if( horizontalScrollBarVisible ) {
				total_sz.x = totalWidth;
				total_sz.y -= (window->ScrollbarSizes.y + (window->WindowPadding.y * 2.0f));
			}
			if( verticalScrollBarVisible ) {
				total_sz.x -= (window->ScrollbarSizes.x + (window->WindowPadding.x * 2.0f));
				total_sz.y = totalHeight;
			}
			SetXYScroll(XYScrollPosition(window->Scroll.x / vs.aveCharWidth, window->Scroll.y / vs.lineHeight));
			// fprintf(stderr, "scroll pos: %d, %d\n", xOffset, topLine);
			if( verticalScrollBarVisible || horizontalScrollBarVisible ) {
				ImGui::Dummy(total_sz);
			}

			// render
			std::unique_ptr<Surface> surfaceWindow(Surface::Allocate(SC_TECHNOLOGY_DEFAULT));
			surfaceWindow->Init(0, wMain.GetID());

			PRectangle rc(0.0f, 0.0f, frame_sz.x, frame_sz.y);
			Paint(surfaceWindow.get(), rc);
			surfaceWindow->Release();

			mainWindow.win = NULL;
		}

		ImGui::EndChildFrame();
	}
private:
	sptr_t DefWndProc(unsigned int iMessage, uptr_t wParam, sptr_t lParam) override {
		return 0;
	}
	bool FineTickerRunning(TickReason reason) override {
		return false;
	}
	void FineTickerStart(TickReason reason, int millis, int tolerance) override {
	}
	void FineTickerCancel(TickReason reason) override {
	}
	bool SetIdle(bool on) override {
		return true;
	}
	void SetMouseCapture(bool on) override {
		captureMouse = on;
	}
	bool HaveMouseCapture() override {
		return captureMouse;
	}
	PRectangle GetClientRectangle() const override {
		Window win = wMain;
		PRectangle rc = win.GetClientPosition();
		ImGuiStyle& style = ImGui::GetStyle();
		if (verticalScrollBarVisible)
			rc.right -= style.ScrollbarSize;
		if (horizontalScrollBarVisible && !Wrapping())
			rc.bottom -= style.ScrollbarSize;
		// Move to origin
		rc.right -= rc.left;
		rc.bottom -= rc.top;
		rc.left = 0;
		rc.top = 0;
		if (rc.bottom < 0)
			rc.bottom = 0;
		if (rc.right < 0)
			rc.right = 0;
		return rc;
	}
	void SetVerticalScrollPos() override {
		if( mainWindow.win ) {
			mainWindow.win->Scroll.y = topLine * vs.lineHeight;
		}
	}
	void SetHorizontalScrollPos() override {
		if( mainWindow.win ) {
			mainWindow.win->Scroll.x = xOffset * vs.aveCharWidth;
		}
	}
	bool ModifyScrollBars(Sci::Line nMax, Sci::Line nPage) override {
		if( mainWindow.win ) {
			mainWindow.win->Scroll.y = topLine * vs.lineHeight;
			mainWindow.win->Scroll.x = xOffset * vs.aveCharWidth;
		}
		return false;
	}
	void NotifyChange() override {
	}
	void NotifyParent(SCNotification scn) override {
	}
	void CopyToClipboard(const SelectionText &selectedText) override {
		ImGui::SetClipboardText(selectedText.Data());
	}
	void Copy() override {
		if (!sel.Empty()) {
			SelectionText selectedText;
			CopySelectionRange(&selectedText);
			CopyToClipboard(selectedText);
		}
	}
	void Paste() override {
		const char* text = ImGui::GetClipboardText();
		if( text ) {
			ClearSelection();
			const int inserted = pdoc->InsertString(CurrentPosition(), text, (Sci::Position)strlen(text));
			if( inserted > 0 ) {
				MovePositionTo(CurrentPosition() + inserted);
			}
		}
	}
	void CreateCallTipWindow(PRectangle rc) override {
	}
	void AddToPopUp(const char *label, int cmd = 0, bool enabled = true) override {
	}
	void ClaimSelection() override {
	}
private:
	bool captureMouse;
	int rectangularSelectionModifier;
	WindowIM mainWindow;
};

ScintillaIM* scintilla_imgui_new() {
	return new ScintillaIM();
}

void scintilla_imgui_free(ScintillaIM* sci) {
	delete sci;
}

void scintilla_imgui_update(ScintillaIM* sci) {
	if( sci ) { sci->Update(); }
}

sptr_t scintilla_imgui_send(ScintillaIM* sci, unsigned int iMessage, uptr_t wParam, sptr_t lParam) {
	return sci ? sci->WndProc(iMessage, wParam, lParam) : NULL;
}
