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

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

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
	fid = 0;	// nk_font CreateNewFont(fp);
}

void Font::Release() {
	if( fid ) {
		fid = 0;
		// delete static_cast<FontHandle *>(fid);
	}
}

#define SCI_PAINT_DEBUG

class WindowIM {
public:
	Window*		win;
	PRectangle	rc;
};

class SurfaceImpl : public Surface {
	bool inited;
	ImguiEnv* env;
	PRectangle bounds;
	ImU32 pen_color;
	ImU32 bg_color;
	float line_thickness;
	ImVec2 move_to;
private:
	void DoClear() {
		inited = false;
		env = NULL;
		bounds = PRectangle(0.0f, 0.0f, 100.0f, 100.0f);
		pen_color = IM_COL32_BLACK;
		bg_color = IM_COL32_WHITE;
		line_thickness = 1.0f;
		move_to = ImVec2(0.0f, 0.0f);
	}
public:
	SurfaceImpl()
		: inited(false)
		, env(NULL)
		, bounds(0.0f, 0.0f, 100.0f, 100.0f)
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
		bounds = w->rc;
		inited = true;
	}
	void Init(SurfaceID sid, WindowID wid) override {
		env = (ImguiEnv*)sid;
		Init(wid);
	}
	void InitPixMap(int width_, int height_, Surface *surface_, WindowID wid) override {
		ImguiEnv* env_ = env;
		PLATFORM_ASSERT(surface_);
		Release();
		SurfaceImpl *surfImpl = static_cast<SurfaceImpl *>(surface_);
		PLATFORM_ASSERT(wid);
		// TODO : image or glfw3 render target ??
		bounds.left = 0.0f;
		bounds.top = 0.0f;
		bounds.right = (XYPOSITION)width_;
		bounds.bottom = (XYPOSITION)height_;
		Init(env_, wid);
	}
	void Release() override {
		DoClear();
	}
	bool Initialised() override {
		return inited;
	}
	void PenColour(ColourDesired fore) override {
		pen_color = IM_COL32(fore.GetRed(),  fore.GetGreen(), fore.GetBlue(), 255);
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
			canvas->PathLineTo(ImVec2(bounds.left + pts[i].x, bounds.top + pts[i].y));
		}
		PenColour(back);
		canvas->AddConvexPolyFilled(canvas->_Path.Data, canvas->_Path.Size,pen_color);
		PenColour(fore);
		canvas->PathStroke(pen_color, true, line_thickness);
	}
	void RectangleDraw(PRectangle rc, ColourDesired fore, ColourDesired back) override {
		ImDrawList* canvas = ImGui::GetWindowDrawList();
		ImVec2 a(bounds.left + rc.left + 0.5f, bounds.top + rc.top + 0.5f);
		ImVec2 b(bounds.left + rc.Width() - 1.0f, bounds.top + rc.Height() - 1.0f);
		PenColour(back);
		canvas->AddRectFilled(a, b, pen_color);
		PenColour(fore);
		canvas->PathRect(a, b);
		canvas->PathStroke(pen_color, true, line_thickness);
	}
	void FillRectangle(PRectangle rc, ColourDesired back) override {
		ImDrawList* canvas = ImGui::GetWindowDrawList();
		ImVec2 a(bounds.left + rc.left + 0.5f, bounds.top + rc.top + 0.5f);
		ImVec2 b(bounds.left + rc.Width() - 1.0f, bounds.top + rc.Height() - 1.0f);
		PenColour(back);
		canvas->AddRectFilled(a, b, pen_color);
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
					ImVec2 a(bounds.left + xTile, bounds.top + yTile);
					ImVec2 b(bounds.left + (float)widthx, bounds.top + (float)heighty);
					canvas->AddRectFilled(a, b, pen_color);
				}
			}
		} else {
			// Something is wrong so try to show anyway
			// Shows up black because colour not allocated
			FillRectangle(rc, ColourDesired(0));
		}
	}
	void RoundedRectangle(PRectangle rc, ColourDesired fore, ColourDesired back) override {
		ImDrawList* canvas = ImGui::GetWindowDrawList();
		float rounding = 2.0f;
		ImVec2 a(bounds.left + rc.left + 0.5f, bounds.top + rc.top + 0.5f);
		ImVec2 b(bounds.left + rc.Width() - 1.0f, bounds.top + rc.Height() - 1.0f);
		PenColour(back);
		canvas->AddRectFilled(a, b, pen_color, rounding);
		PenColour(fore);
		canvas->PathRect(a, b, rounding);
		canvas->PathStroke(pen_color, true, line_thickness);
	}
	void AlphaRectangle(PRectangle rc, int cornerSize, ColourDesired fill, int alphaFill,
		ColourDesired outline, int alphaOutline, int flags) override
	{
		ImDrawList* canvas = ImGui::GetWindowDrawList();
		ImVec2 a(bounds.left + rc.left + 0.5f, bounds.top + rc.top + 0.5f);
		ImVec2 b(bounds.left + rc.Width() - 1.0f, bounds.top + rc.Height() - 1.0f);
		pen_color = IM_COL32(fill.GetRed(),  fill.GetGreen(), fill.GetBlue(), alphaFill);
		canvas->AddRectFilled(a, b, pen_color);
		pen_color = IM_COL32(outline.GetRed(),  outline.GetGreen(), outline.GetBlue(), alphaFill);
		canvas->PathRect(a, b);
		canvas->PathStroke(pen_color, true, line_thickness);
	}
	void DrawRGBAImage(PRectangle rc, int width, int height, const unsigned char *pixelsImage) override {
		// TODO : nk_draw_image();
	}
	void Ellipse(PRectangle rc, ColourDesired fore, ColourDesired back) override {
		//float cx = bounds.x + (rc.left + rc.right) / 2;
		//float cy = bounds.y + (rc.top + rc.bottom) / 2;
		//float r = XYPositionMin(rc.Width(), rc.Height()) / 2;
		//
		//PenColour(back);
		//nk_fill_arc(out, cx, cy, r, 0.0f, (float)(2*kPi), pen_color);
		//PenColour(fore);
		//nk_stroke_arc(out, cx, cy, r, 0.0f, (float)(2*kPi), line_thickness, pen_color);
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
		ImVec2 pos(bounds.left + rc.left, bounds.top + rc.top);
		PenColour(fore);
		// TODO : use font & clip
		canvas->AddText(pos, pen_color, s, s+len);
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
	// TODO : need implements Font, now simple 
	void MeasureWidths(Font &font_, const char *s, int len, XYPOSITION *positions) override {
		// const struct nk_user_font* ufont = context ? context->style.font : NULL;
		// struct nk_font *font = ufont ? (struct nk_font *)(ufont->userdata.ptr) : NULL;
		if( false ) {
			/*
			nk_rune unicode = NK_UTF_INVALID;
			const struct nk_font_glyph *g;
			float w = 0.0f;
			float scale = 1.0f;
			while( len > 0 ) {
				int glyph_len = nk_utf_decode(s, &unicode, len);
				if( glyph_len==0 )
					break;
				if (unicode == NK_UTF_INVALID)
					break;
				g = nk_font_find_glyph(font, unicode);
				w += g->xadvance * scale;
				for(int i=0; i<glyph_len; ++i)
					*positions++ = w;
				s += glyph_len;
				len -= glyph_len;
			}
			*/
		} else {
			XYPOSITION pos = kDefaultFontSize;
			for( int i=0; i<len; ++i ) {
				positions[i] = pos;
				pos += kDefaultFontSize;
			}
		}
	}
	XYPOSITION WidthText(Font &font_, const char *s, int len) override {
		// const struct nk_user_font* font = context ? context->style.font : NULL;
		// return font ? font->width(font->userdata, font->height, s, len) : (kDefaultFontSize * len);
		return kDefaultFontSize * len;
	}
	XYPOSITION WidthChar(Font &font_, char ch) override {
		return WidthText(font_, &ch, 1);
	}
	XYPOSITION Ascent(Font &font_) override {
		return 0.0f;
	}
	XYPOSITION Descent(Font &font_) override {
		return 16.0f;
	}
	XYPOSITION InternalLeading(Font &font_) override {
		return 0.0f;
	}
	XYPOSITION Height(Font &font_) override {
		return Ascent(font_) + Descent(font_);
	}
	XYPOSITION AverageCharWidth(Font &font_) override {
		return WidthChar(font_, 'n');
	}

public:
	void SetClip(PRectangle rc) override {
		// TODO : nk_start_popup or nk_command_buffer_init
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
	return w->rc;
}

void Window::SetPosition(PRectangle rc) {
	WindowIM* w = (WindowIM*)wid;
	w->rc = rc;
}

void Window::SetPositionRelative(PRectangle rc, Window relativeTo) {
	WindowIM* w = (WindowIM*)wid;
	if( w && w->win ) {
		WindowIM* wndRelativeTo = (WindowIM*)(relativeTo.GetID());
		XYPOSITION ox = wndRelativeTo ? wndRelativeTo->rc.left : 0;
		XYPOSITION oy = wndRelativeTo ? wndRelativeTo->rc.right : 0;
		PRectangle rcMonitor(0, 0, 1920, 1080);
		ox += rc.left;
		oy += rc.top;
		if( wndRelativeTo ) {
			rcMonitor = wndRelativeTo->rc;
		}

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

		w->rc = PRectangle(ox, oy, ox+sizex, oy+sizey);
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
		: imguiEnv(NULL)
		, capturedMouse(false)
		, rectangularSelectionModifier(SCMOD_ALT)
	{
		mainWindow.win = &wMain;
		mainWindow.rc.left = 16;
		mainWindow.rc.top = 0;
		mainWindow.rc.right = 400;
		mainWindow.rc.bottom = 300;
		wMain = (WindowID)&mainWindow;

		marginWindow.win = &wMargin;
		mainWindow.rc.left = 0;
		mainWindow.rc.top = 0;
		mainWindow.rc.right = 16;
		mainWindow.rc.bottom = 300;
		wMargin = (WindowID)&marginWindow;

		view.bufferedDraw = false;
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
	//void HandleMouseEvents(struct nk_input* in, struct nk_rect bounds, unsigned int now, int modifiers) {
	//	struct nk_rect area = bounds;

	//	PRectangle wRect = wMain.GetPosition();
	//	struct nk_mouse_button* btn;

 //       /* mouse click handler */
 //       char is_hovered = (char)nk_input_is_mouse_hovering_rect(in, area);
	//	if( is_hovered ) {
	//		btn = &(in->mouse.buttons[NK_BUTTON_LEFT]);
	//		if( btn->down ) {
	//			SetFocusState(true);
	//			if( btn->clicked ) {
	//				Point click_pos(btn->clicked_pos.x - wRect.left, btn->clicked_pos.y - wRect.top);
	//				ButtonDownWithModifiers(click_pos, now, modifiers);
	//			} else if ((in->mouse.delta.x != 0.0f || in->mouse.delta.y != 0.0f)) {
	//				// TODO : drag
	//			}
	//		} else if( btn->clicked ) {
	//			SetFocusState(true);
	//			if (!HaveMouseCapture())
	//				return;
	//			Point pt(in->mouse.pos.x - wRect.left, in->mouse.pos.y - wRect.top);
	//			//	sciThis,event->window,event->time, pt.x, pt.y);
	//			// if (event->window != PWindow(sciThis->wMain))
	//					// If mouse released on scroll bar then the position is relative to the
	//					// scrollbar, not the drawing window so just repeat the most recent point.
	//			//		pt = sciThis->ptMouseLast;
	//			ButtonUpWithModifiers(pt, now, modifiers);
	//		}
	//	}

	//	if( is_hovered ) {
	//		btn = &(in->mouse.buttons[NK_BUTTON_RIGHT]);
	//		if( btn->down ) {
	//			if( btn->clicked ) {
	//				Point pt(in->mouse.pos.x - wRect.left, in->mouse.pos.y - wRect.top);
	//				if (!PointInSelection(pt))
	//					SetEmptySelection(PositionFromLocation(pt));
	//				if (ShouldDisplayPopup(pt)) {
	//					// PopUp menu
	//					// Convert to screen
	//					int ox = 0;
	//					int oy = 0;
	//					// gdk_window_get_origin(PWindow(wMain), &ox, &oy);
	//					ContextMenu(Point(pt.x + ox, pt.y + oy));
	//				} else {
	//					RightButtonDownWithModifiers(pt, now, modifiers);
	//				}
	//			}
	//		}
	//	}

	//	// move
	//	if( is_hovered || capturedMouse ) {
	//		if ((in->mouse.delta.x != 0.0f || in->mouse.delta.y != 0.0f)) {
	//			Point pt(in->mouse.pos.x - wRect.left, in->mouse.pos.y - wRect.top);
	//			ButtonMoveWithModifiers(pt, now, modifiers);
	//		}
	//	}
	//}
	//void DoKeyPressed(int k, int modifiers) {
	//	int key = 0;
	//	switch(k) {
	//	case NK_KEY_DEL:				key=SCK_DELETE;	break;
	//	case NK_KEY_TAB:				key=SCK_TAB;	break;
	//	case NK_KEY_BACKSPACE:			key=SCK_BACK;	break;
	//	case NK_KEY_UP:					key=SCK_UP;		break;
	//	case NK_KEY_DOWN:				key=SCK_DOWN;	break;
	//	case NK_KEY_LEFT:				key=SCK_LEFT;	break;
	//	case NK_KEY_RIGHT:				key=SCK_RIGHT;	break;
	//	case NK_KEY_TEXT_START:			key=SCK_HOME;	break;
	//	case NK_KEY_TEXT_END:			key=SCK_END;	break;
	//	case NK_KEY_TEXT_INSERT_MODE:	key=SCK_INSERT;	break;
	//	case NK_KEY_TEXT_REPLACE_MODE:	key=SCK_INSERT;	break;
	//	case NK_KEY_TEXT_RESET_MODE:	key=SCK_INSERT;	break;
	//	case NK_KEY_SCROLL_START:		key=SCK_HOME;	break;
	//	case NK_KEY_SCROLL_END:			key=SCK_END;	break;
	//	case NK_KEY_SCROLL_DOWN:		key=SCK_PRIOR;	break;
	//	case NK_KEY_SCROLL_UP:			key=SCK_NEXT;	break;
	//	default:	break;
	//	}
	//	if( key ) {
	//		bool consumed = false;
	//		KeyDownWithModifiers(key, modifiers, &consumed);
	//		//fprintf(stderr, "SK-key: %d %x %x\n",event->keyval, event->state, consumed);
	//	}
	//}
	//void HandleKeyboardEvents(struct nk_input* in, struct nk_rect bounds, unsigned int now, int modifiers) {
 //       /* keyboard input */
 //       {
 //       	for( int k=0; k<NK_KEY_MAX; ++k ) {
 //       		if( nk_input_is_key_pressed(in, (nk_keys)k) ) {
	//        		DoKeyPressed(k, modifiers);
 //       		}
 //       	}
 //       }

 //       /* text input */
	//	if( in->keyboard.text_len ) {
	//		ClearSelection();
	//		const int inserted = pdoc->InsertString(CurrentPosition(), in->keyboard.text, in->keyboard.text_len);
	//		if( inserted > 0 ) {
	//			MovePositionTo(CurrentPosition() + inserted);
	//		}
 //           in->keyboard.text_len = 0;
 //       }

 //       /* enter key handler */
 //       if (nk_input_is_key_pressed(in, NK_KEY_ENTER)) {
	//		bool consumed = false;
	//		bool added = KeyDownWithModifiers('\n', modifiers, &consumed) != 0;
	//		if( !consumed )
	//			consumed = added;
	//		if( !consumed ) {
	//			ClearSelection();
	//			const int inserted = pdoc->InsertString(CurrentPosition(), "\n", 1);
	//			if( inserted > 0 ) {
	//				MovePositionTo(CurrentPosition() + inserted);
	//			}
	//		}
 //       }

 //       /* cut & copy handler */
	//	if( nk_input_is_key_pressed(in, NK_KEY_COPY) ) {
	//		Copy();
	//	}
	//	if( nk_input_is_key_pressed(in, NK_KEY_CUT) ) {
	//		Cut();
	//	}

 //       /* paste handler */
	//	if( nk_input_is_key_pressed(in, NK_KEY_PASTE) ) {
	//		Paste();
	//	}
	//}
	//void HandleInputEvents() {
	//	struct nk_input* in = &(ctx->input);
	//	unsigned int now = GetCurrentTime();
 //       int shift = in->keyboard.keys[NK_KEY_SHIFT].down;
 //       int ctrl = in->keyboard.keys[NK_KEY_CTRL].down;
	//	int alt = in->keyboard.keys[NK_KEY_ALT].down;
	//	int modifiers = ModifierFlags(shift, ctrl, alt);
	//	HandleMouseEvents(in, bounds, now, modifiers);
	//	HandleKeyboardEvents(in, bounds, now, modifiers);
	//}
	void Update(ImguiEnv* env) {
		if( !env ) return;
		imguiEnv = env;

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImRect bb(window->DC.CursorPos, window->DC.CursorPos);
		bb.Max.x += 400.0f;
		bb.Max.y += 300.0f;
        if( !ImGui::ItemAdd(bb, 0) )
			return;
		if( !ImGui::BeginChildFrame(ImGui::GetID("Sci"), bb.GetSize())) {
            ImGui::EndChildFrame();
            return;
        }
        // size.x -= window->ScrollbarSizes.x;
		// const bool hovered = ItemHoverable(frame_bb, id);
		// if (hovered)
		// 	g.MouseCursor = ImGuiMouseCursor_TextInput;

		mainWindow.rc.left = bb.Min.x;
		mainWindow.rc.top = bb.Min.y;
		mainWindow.rc.right = bb.Max.x;
		mainWindow.rc.bottom = bb.Max.y;
		PRectangle rc(0.0f, 0.0f, bb.GetWidth(), bb.GetHeight());

		// HandleInputEvents();

		// render
		std::unique_ptr<Surface> surfaceWindow(Surface::Allocate(SC_TECHNOLOGY_DEFAULT));
		surfaceWindow->Init(imguiEnv, wMain.GetID());
		Paint(surfaceWindow.get(), rc);
		surfaceWindow->Release();
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
		capturedMouse = on;
	}
	bool HaveMouseCapture() override {
		return capturedMouse;
	}
	PRectangle GetClientRectangle() const override {
		Window win = wMain;
		PRectangle rc = win.GetClientPosition();
		// if (verticalScrollBarVisible)
		// 	rc.right -= verticalScrollBarWidth;
		// rc.right -= 24.0f;
		// if (horizontalScrollBarVisible && !Wrapping())
		// 	rc.bottom -= horizontalScrollBarHeight;
		// rc.bottom -= 24.0f;
		// Move to origin
		rc.right -= rc.left;
		rc.bottom -= rc.top;
		if (rc.bottom < 0)
			rc.bottom = 0;
		if (rc.right < 0)
			rc.right = 0;
		rc.left = 0;
		rc.top = 0;
		return rc;
	}
	void SetVerticalScrollPos() override {
	}
	void SetHorizontalScrollPos() override {
	}
	bool ModifyScrollBars(Sci::Line nMax, Sci::Line nPage) override {
		return false;
	}
	void NotifyChange() override {
	}
	void NotifyParent(SCNotification scn) override {
	}
	void CopyToClipboard(const SelectionText &selectedText) override {
		// __puss_nuklear_iface__->clipbard_set_string(currentNKContext, selectedText.Data());
	}
	void Copy() override {
		if (!sel.Empty()) {
			SelectionText selectedText;
			CopySelectionRange(&selectedText);
			CopyToClipboard(selectedText);
		}
	}
	void Paste() override {
		const char* text = NULL; // __puss_nuklear_iface__->clipbard_get_string(currentNKContext);
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
	ImguiEnv* imguiEnv;
	WindowIM mainWindow;
	WindowIM marginWindow;
	bool capturedMouse;
	int rectangularSelectionModifier;
};

ScintillaIM* scintilla_imgui_new() {
	return new ScintillaIM();
}

void scintilla_imgui_free(ScintillaIM* sci) {
	delete sci;
}

void scintilla_imgui_update(ScintillaIM* sci, ImguiEnv* env) {
	if( sci ) {
		sci->Update(env);
	}
}

sptr_t scintilla_imgui_send(ScintillaIM* sci, unsigned int iMessage, uptr_t wParam, sptr_t lParam) {
	return sci ? sci->WndProc(iMessage, wParam, lParam) : NULL;
}
