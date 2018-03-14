// puss_module_nuklear_scintilla.c

#include "scintilla_nk.h"

#if defined(__WIN32__) || defined(_MSC_VER)
#include <windows.h>
#else
#include <sys/time.h>
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

#include "puss_module_nuklear.h"

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

enum encodingType { singleByte, UTF8, dbcs};

// X has a 16 bit coordinate space, so stop drawing here to avoid wrapping
static const int maxCoordinate = 32000;

static const double kPi = 3.14159265358979323846;

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

static struct nk_color g_color_black = {0,0,0,255};
static struct nk_color g_color_white = {255,255,255,255};

// SurfaceID is a cairo_t*
class SurfaceImpl : public Surface {
	bool inited;
	struct nk_window* window;
	struct nk_context* context;
	int width;
	int height;
	struct nk_color pen_color;
	struct nk_color bg_color;
	float line_thickness;
	int move_to_x;
	int move_to_y;
private:
	struct nk_command_buffer* FetchCurrentCommandBuffer() {
		return (context && context->current) ? &(context->current->buffer) : NULL;
	}
	void DoClear() {
		inited = false;
		window = NULL;
		context = NULL;
		pen_color = g_color_black;
		bg_color = g_color_white;
		line_thickness = 1.0f;
		move_to_x = 0;
		move_to_y = 0;
	}
public:
	SurfaceImpl()
		: inited(false)
		, window(NULL)
		, context(NULL)
		, width(100)
		, height(100)
		, pen_color(g_color_black)
		, bg_color(g_color_white)
		, line_thickness(1.0f)
		, move_to_x(0)
		, move_to_y(0)
	{
	}
	~SurfaceImpl() override {
		DoClear();
	}
	struct nk_color& BackgroundColor() {
		return bg_color;
	}
	void Init(WindowID wid) override {
		window = (struct nk_window*)wid;
		inited = true;
	}
	void Init(SurfaceID sid, WindowID wid) override {
		context = (struct nk_context*)sid;
		Init(wid);
	}
	void InitPixMap(int width_, int height_, Surface *surface_, WindowID wid) override {
		width = width_;
		height = height_;
		Init(surface_, wid);
	}
	void Release() override {
		DoClear();
	}
	bool Initialised() override {
		return inited;
	}
	void PenColour(ColourDesired fore) override {
		ColourDesired cdFore(fore.AsLong());
		struct nk_color c =
			{ (nk_byte)cdFore.GetRed()
			, (nk_byte)cdFore.GetGreen()
			, (nk_byte)cdFore.GetBlue()
			, 255
			};
		pen_color = c;
	}
	int LogPixelsY() override {
		return 72;
	}
	int DeviceHeightFont(int points) override {
		int logPix = LogPixelsY();
		return (points * logPix + logPix / 2) / 72;
	}
	void MoveTo(int x_, int y_) override {
		move_to_x = x_;
		move_to_y = y_;
	}
	void LineTo(int x_, int y_) override {
		struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
		nk_stroke_line(out
			, (float)(move_to_x + 0.5f), (float)(move_to_y + 0.5f)
			, (float)(x_ + 0.5f), (float)(y_ + 0.5f)
			, line_thickness
			, pen_color
			);
		move_to_x = x_;
		move_to_y = y_;
	}
	void Polygon(Point *pts, int npts, ColourDesired fore, ColourDesired back) override {
		struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
		float* points = (float*)pts;
		assert( sizeof(Point)==(sizeof(float)*2) );
		PenColour(back);
		nk_fill_polygon(out, points, npts, pen_color);
		PenColour(fore);
		nk_stroke_polygon(out, points, npts, line_thickness, pen_color);
	}
	void RectangleDraw(PRectangle rc, ColourDesired fore, ColourDesired back) override {
		struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
		struct nk_rect rect = __nk_rect(rc.left + 0.5, rc.top + 0.5, rc.Width() - 1.0f, rc.Height() - 1.0f);
		PenColour(back);
		nk_fill_rect(out, rect, 0.0f, pen_color);
		PenColour(fore);
		nk_stroke_rect(out, rect, 0.0f, line_thickness, pen_color);
	}
	void FillRectangle(PRectangle rc, ColourDesired back) override {
		PenColour(back);
		struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
		struct nk_rect rect = __nk_rect(rc.left, rc.top, rc.Width(), rc.Height());
		nk_fill_rect(out, rect, 0.0f, pen_color);
	}
	void FillRectangle(PRectangle rc, Surface &surfacePattern) override {
		// SurfaceImpl &surfi = static_cast<SurfaceImpl &>(surfacePattern);
		bool canDraw = false; // TODO : surfi.psurf != NULL;	
		if( canDraw ) {
			struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
			// Tile pattern over rectangle
			// Currently assumes 8x8 pattern
			int widthPat = 8;
			int heightPat = 8;
			for (int xTile = rc.left; xTile < rc.right; xTile += widthPat) {
				int widthx = (xTile + widthPat > rc.right) ? rc.right - xTile : widthPat;
				for (int yTile = rc.top; yTile < rc.bottom; yTile += heightPat) {
					int heighty = (yTile + heightPat > rc.bottom) ? rc.bottom - yTile : heightPat;
					// cairo_set_source_surface(context, surfi.psurf, xTile, yTile);
					struct nk_rect rect = __nk_recti(xTile, yTile, widthx, heighty);
					nk_fill_rect(out, rect, 0.0f, pen_color);
				}
			}
		} else {
			// Something is wrong so try to show anyway
			// Shows up black because colour not allocated
			FillRectangle(rc, ColourDesired(0));
		}
	}
	void RoundedRectangle(PRectangle rc, ColourDesired fore, ColourDesired back) override {
		struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
		struct nk_rect rect = __nk_rect(rc.left + 0.5, rc.top + 0.5, rc.Width() - 1.0f, rc.Height() - 1.0f);
		PenColour(back);
		nk_fill_rect(out, rect, 2.0f, pen_color);
		PenColour(fore);
		nk_stroke_rect(out, rect, 2.0f, line_thickness, pen_color);
	}
	void AlphaRectangle(PRectangle rc, int cornerSize, ColourDesired fill, int alphaFill,
		ColourDesired outline, int alphaOutline, int flags) override
	{
		struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
		struct nk_rect rect = __nk_rect(rc.left + 0.5, rc.top + 0.5, rc.Width() - 1.0f, rc.Height() - 1.0f);
		PenColour(fill);
		pen_color.a = (nk_byte)alphaFill;
		nk_fill_rect(out, rect, 0.0f, pen_color);
		PenColour(outline);
		pen_color.a = (nk_byte)alphaFill;
		nk_stroke_rect(out, rect, 0.0f, line_thickness, pen_color);
	}
	void DrawRGBAImage(PRectangle rc, int width, int height, const unsigned char *pixelsImage) override {
		struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
		struct nk_rect rect = __nk_rect(rc.left, rc.top, rc.Width(), rc.Height());
		struct nk_color c = { 255, 0, 0, 255 };
		// TODO : nk_draw_image();
		nk_fill_rect(out, rect, 0.0f, c);
	}
	void Ellipse(PRectangle rc, ColourDesired fore, ColourDesired back) override {
		struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
		PenColour(back);
		nk_fill_arc(out, (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2, std::min(rc.Width(), rc.Height()) / 2, 0, 2*kPi, pen_color);
		PenColour(fore);
		nk_stroke_arc(out, (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2, std::min(rc.Width(), rc.Height()) / 2, 0, 2*kPi, line_thickness, pen_color);
	}
	void Copy(PRectangle rc, Point from, Surface &surfaceSource) override {
		// SurfaceImpl &surfi = static_cast<SurfaceImpl &>(surfaceSource);
		bool canDraw = false; // TODO : surfi.psurf != NULL;	
		if( canDraw ) {
			struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
			struct nk_rect rect = __nk_rect(rc.left, rc.top, rc.Width(), rc.Height());
			struct nk_color c = { 255, 255, 0, 255 };
			nk_fill_rect(out, rect, 0.0f, c);
			// cairo_set_source_surface(context, surfi.psurf, rc.left - from.x, rc.top - from.y);
			// cairo_rectangle(context, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top);
			// cairo_fill(context);
		}
	}

public:
	void DrawTextBase(PRectangle rc, Font &font_, XYPOSITION ybase, const char *s, int len, ColourDesired fore) {
		PenColour(fore);
		struct nk_command_buffer* out = FetchCurrentCommandBuffer(); if( !out ) return;
		struct nk_rect rect = __nk_rect(rc.left, rc.top, rc.Width(), rc.Height());
		// XYPOSITION xText = rc.left;
		// TODO : use font
		nk_draw_text(out, rect, s, len, context->style.font, pen_color, bg_color);
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
	void MeasureWidths(Font &font_, const char *s, int len, XYPOSITION *positions) override {
		// TODO : now simple
		XYPOSITION pos = 0;
		for( int i=0; i<len; ++len ) {
			positions[i] = pos;
			pos += 16;
		}
	}
	XYPOSITION WidthText(Font &font_, const char *s, int len) override {
		// TODO : now simple
		return 16 * len;
	}
	XYPOSITION WidthChar(Font &font_, char ch) override {
		return 16;
	}
	XYPOSITION Ascent(Font &font_) override {
		return 0;		
	}
	XYPOSITION Descent(Font &font_) override {
		return 24;
	}
	XYPOSITION InternalLeading(Font &font_) override {
		return 0;
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
	struct nk_window* w = (struct nk_window*)wid;
	PRectangle rc(0, 0, 1000, 1000);
	if( w ) {
		rc.left = w->bounds.x;
		rc.top = w->bounds.y;
		rc.right = w->bounds.x + w->bounds.w;
		rc.bottom = w->bounds.y + w->bounds.h;
	}
	return rc;
}

void Window::SetPosition(PRectangle rc) {
	struct nk_window* w = (struct nk_window*)wid;
	if( w ) {
		w->bounds.x = rc.left;
		w->bounds.y = rc.top;
		w->bounds.w = rc.Width();
		w->bounds.h = rc.Height();
	}
}

void Window::SetPositionRelative(PRectangle rc, Window relativeTo) {
	struct nk_window* w = (struct nk_window*)wid;
	if( w ) {
		struct nk_window* wndRelativeTo = (struct nk_window*)(relativeTo.wid);
		int ox = wndRelativeTo ? wndRelativeTo->bounds.x : 0;
		int oy = wndRelativeTo ? wndRelativeTo->bounds.y : 0;
		PRectangle rcMonitor(0, 0, 1000, 1000);
		ox += rc.left;
		oy += rc.top;
		if( wndRelativeTo ) {
			rcMonitor.left = wndRelativeTo->bounds.x;
			rcMonitor.top = wndRelativeTo->bounds.y;
			rcMonitor.right = wndRelativeTo->bounds.x + wndRelativeTo->bounds.w;
			rcMonitor.bottom = wndRelativeTo->bounds.y + wndRelativeTo->bounds.h;
		}

		/* do some corrections to fit into screen */
		int sizex = rc.right - rc.left;
		int sizey = rc.bottom - rc.top;
		if (sizex > rcMonitor.Width() || ox < rcMonitor.left)
			ox = rcMonitor.left; /* the best we can do */
		else if (ox + sizex > rcMonitor.left + rcMonitor.Width())
			ox = rcMonitor.left + rcMonitor.Width() - sizex;
		if (sizey > rcMonitor.Height() || oy < rcMonitor.top)
			oy = rcMonitor.top;
		else if (oy + sizey > rcMonitor.top + rcMonitor.Height())
			oy = rcMonitor.top + rcMonitor.Height() - sizey;

		w->bounds.x = ox;
		w->bounds.y = oy;
		w->bounds.w = sizex;
		w->bounds.h = sizey;
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
	PRectangle rcMonitor(0, 0, 1000, 1000);
	struct nk_window* w = (struct nk_window*)wid;
	if( w ) {
		rcMonitor.left = w->bounds.x;
		rcMonitor.top = w->bounds.y;
		rcMonitor.right = w->bounds.x + w->bounds.w;
		rcMonitor.bottom = w->bounds.y + w->bounds.h;
	}
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

class ScintillaNK : public ScintillaBase {
	ScintillaNK(const ScintillaNK &) = delete;
	ScintillaNK &operator=(const ScintillaNK &) = delete;
public:
	ScintillaNK() {
	}
	virtual ~ScintillaNK() {
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
public: 	// Public for scintilla_send_message
	sptr_t WndProc(unsigned int iMessage, uptr_t wParam, sptr_t lParam) override {
		return 0;
	}
	void RenderNK(struct nk_context* ctx) {
		struct nk_rect bounds;
		nk_widget(&bounds, ctx);
		PRectangle rc;
		rc.left = bounds.x;
		rc.top = bounds.y;
		rc.right = bounds.x + bounds.w;
		rc.bottom = bounds.y + bounds.y;

		AutoSurface surfaceWindow(ctx, this);
		if( surfaceWindow ) {
			nk_fill_rect(&(ctx->current->buffer), bounds, 0.0f, g_color_white);
			Paint(surfaceWindow, rc);
			surfaceWindow->Release();
		} else {
			nk_fill_rect(&(ctx->current->buffer), bounds, 0.0f, g_color_black);
		}
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
	}
	bool HaveMouseCapture() override {
		return false;
	}
	bool PaintContains(PRectangle rc) override {
		return false;
	}
	PRectangle GetClientRectangle() const override {
		PRectangle rect;
		return rect;
	}
	void ScrollText(Sci::Line linesToMove) override {
	}
	void SetVerticalScrollPos() override {
	}
	void SetHorizontalScrollPos() override {
	}
	bool ModifyScrollBars(Sci::Line nMax, Sci::Line nPage) override {
		return false;
	}
	void ReconfigureScrollBars() override {
	}
	void NotifyChange() override {
	}
	void NotifyFocus(bool focus) override {
	}
	void NotifyParent(SCNotification scn) override {
	}
	void NotifyKey(int key, int modifiers) {
	}
	void NotifyURIDropped(const char *list) {
	}
	CaseFolder *CaseFolderForEncoding() override {
		return 0;
	}
	std::string CaseMapString(const std::string &s, int caseMapping) override {
		return "";
	}
	int KeyDefault(int key, int modifiers) override {
		return 0;
	}
	void CopyToClipboard(const SelectionText &selectedText) override {
	}
	void Copy() override {
	}
	void Paste() override {
	}
	void CreateCallTipWindow(PRectangle rc) override {
	}
	void AddToPopUp(const char *label, int cmd = 0, bool enabled = true) override {
	}
	bool OwnPrimarySelection() {
		return false;
	}
	void ClaimSelection() override {
	}
};

extern "C" ScintillaNK* scintilla_nk_new(void) {
	return new ScintillaNK;
}

extern "C" void scintilla_nk_free(ScintillaNK* sci) {
	delete sci;
}

extern "C" void scintilla_nk_update(ScintillaNK* sci, struct nk_context* ctx) {
	if( !(sci && ctx && ctx->current) )	return;

	// TODO : input events

	// paint
	nk_layout_row_dynamic(ctx, 200, 1);
	sci->RenderNK(ctx);
}

extern "C" sptr_t scintilla_nk_send(ScintillaNK* sci, unsigned int iMessage, uptr_t wParam, sptr_t lParam) {
	return sci ? sci->WndProc(iMessage, wParam, lParam) : NULL;
}

