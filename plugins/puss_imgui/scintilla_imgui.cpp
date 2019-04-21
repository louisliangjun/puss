// scintilla_imgui.cpp

#include "scintilla_imgui.h"

#if defined(_WIN32)
	#include <windows.h>
	#include <time.h>
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
#include <cmath>
#include <climits>

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

class SurfaceImpl;

#include "Platform.h"

#include "ILoader.h"
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
#include "LineMarker.h"
#include "Style.h"
#include "ViewStyle.h"
#include "CharClassify.h"
#include "Decoration.h"
#include "CaseFolder.h"
#include "Document.h"
#include "CaseConvert.h"
#include "UniConversion.h"
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

#ifdef _WIN32
	static CLIPFORMAT cfColumnSelect = static_cast<CLIPFORMAT>(
		::RegisterClipboardFormat(TEXT("MSDEVColumnSelect")));
	static CLIPFORMAT cfBorlandIDEBlockType = static_cast<CLIPFORMAT>(
		::RegisterClipboardFormat(TEXT("Borland IDE Block Type")));

	// Likewise for line-copy (copies a full line when no text is selected)
	static CLIPFORMAT cfLineSelect = static_cast<CLIPFORMAT>(
		::RegisterClipboardFormat(TEXT("MSDEVLineSelect")));
	static CLIPFORMAT cfVSLineTag = static_cast<CLIPFORMAT>(
		::RegisterClipboardFormat(TEXT("VisualStudioEditorOperationsLineCutCopyClipboardTag")));
#endif

using namespace Scintilla;

// X has a 16 bit coordinate space, so stop drawing here to avoid wrapping
static const int maxCoordinate = 32000;

static const double kPi = 3.14159265358979323846;

static inline XYPOSITION XYPositionMin(XYPOSITION a, XYPOSITION b) {
	return a < b ? a : b;
}

class FontIM {
public:
	FontIM() : size(7.0f) {}
public:
	float	size;
};

Font::Font() : fid(0) {}

Font::~Font() {}

void Font::Create(const FontParameters &fp) {
	FontIM* ft;
	Release();
	ft = new FontIM();
	ft->size = fp.size;
	fid = ft;
}

void Font::Release() {
	if( fid ) {
		fid = 0;
	}
}

static FontIM null_font;

inline FontIM& im_font_cast(Font& font) {
	FontIM* ft = (FontIM*)(font.GetID());
	return ft ? *ft : null_font;
}

inline ImFont* im_font_current() {
	// NOTICE imgui maybe free, but SciFont unknown this: return (ImFont*)font.GetID();
	return ImGui::GetCurrentContext() ? ImGui::GetFont() : NULL;
}

class WindowIM {
public:
	WindowIM() : win(0), modified(false), font_scale(1.0f) {
	}
public:
	ImGuiWindow*	win;
	bool			modified;
	float			font_scale;
	ImVec2			pos;
	ImVec2			size;
};

class SurfaceImpl : public Surface {
	bool inited;
	ImVec2 offset;
	ImU32 pen_color;
	ImU32 bg_color;
	float line_thickness;
	ImVec2 move_to;
	float font_scale;
	int clip_count;
private:
	void DoClear() {
		for( ; clip_count > 0; --clip_count )
			ImGui::PopClipRect();
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
		, font_scale(1.0f)
		, clip_count(0)
	{
	}
	~SurfaceImpl() override {
		DoClear();
	}
	void Init(WindowID wid) override {
		WindowIM* w = (WindowIM*)wid;
		PLATFORM_ASSERT(w);
		offset = w->pos;
		inited = true;
		if( w->win ) {
			font_scale = w->font_scale;
		}
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
		ImVec2 a(offset.x + move_to.x, offset.y + move_to.y);
		ImVec2 b(offset.x + x_, offset.y + y_);
		move_to.x = (float)x_;
		move_to.y = (float)y_;
		ImGui::GetWindowDrawList()->AddLine(a, b, pen_color, line_thickness);
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
		bg_color = IM_COL32(back.GetRed(),  back.GetGreen(), back.GetBlue(), alpha);
#if 1
		canvas->AddRectFilled(a, b, bg_color);
#else
		canvas->PathRect(a, b);
		canvas->PathStroke(IM_COL32(255,  0, 0, 32), true, line_thickness);
#endif
	}
	void FillRectangle(PRectangle rc, Surface &surfacePattern) override {
		SurfaceImpl &surfi = static_cast<SurfaceImpl &>(surfacePattern);
		FillRectangle(rc, surfi.bg_color);
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
		float r = ((w < h) ? w : h) * 0.5f;
		canvas->PathArcTo(ImVec2(cx, cy), r, 0.0f, (float)(2*kPi));
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
	void DrawTextBase(PRectangle rc, Font &font, XYPOSITION ybase, const char *s, int len, ColourDesired fore) {
		ImDrawList* canvas = ImGui::GetWindowDrawList();
		ImVec2 a(offset.x + rc.left, offset.y + rc.top);
		ImVec2 b(offset.x + rc.right, offset.y + rc.bottom);
		;
		float font_size = im_font_cast(font).size * font_scale;
		if( font_size <= 5.0f ) {
			float s = 0.5f;
			canvas->PathLineTo(ImVec2(a.x+s, a.y+s));
			canvas->PathLineTo(ImVec2(b.x-s, a.y+s));
			canvas->PathLineTo(ImVec2(b.x-s, b.y-s));
			canvas->PathLineTo(ImVec2(a.x+s, b.y-s));
			canvas->PathFillConvex(SetPenColour(fore));
		} else {
			ImFont* imfont = im_font_current();
			if( imfont ) {
				ImVec4 rect(a.x, a.y, b.x, b.y);
				canvas->AddText(imfont, font_size, a, SetPenColour(fore), s, s+len, 0.0f, &rect);
			}
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
	XYPOSITION DoMeasureWidths(Font &font, const char *s, int len, XYPOSITION *positions) {
		ImFont* imfont = im_font_current();
		XYPOSITION w = 0.0f;
		unsigned int wch = 0;
		const char *e = s + len;
		while( s < e ) {
			int glyph_len = ImTextCharFromUtf8(&wch, s, e);
			if( glyph_len==0 )
				break;
			if( !wch )
				break;
			if( imfont ) {
				const ImFontGlyph* g = imfont->FindGlyph((ImWchar)wch);
				w += (g->AdvanceX * (im_font_cast(font).size / imfont->FontSize * font_scale));
			} else {
				w += im_font_cast(font).size * font_scale;
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
	XYPOSITION Ascent(Font &font) override {
		ImFont* imfont = im_font_current();
		return imfont ? (imfont->Ascent * (im_font_cast(font).size / imfont->FontSize * font_scale)): 1.0f;
	}
	XYPOSITION Descent(Font &font) override {
		ImFont* imfont = im_font_current();
		return imfont ? -(imfont->Descent * (im_font_cast(font).size / imfont->FontSize * font_scale)): 1.0f;
	}
	XYPOSITION InternalLeading(Font &font_) override {
		return 0.0f;
	}
	XYPOSITION Height(Font &font) override {
		return im_font_cast(font).size * font_scale;
	}
	XYPOSITION AverageCharWidth(Font &font_) override {
		return WidthChar(font_, 'n');
	}

public:
	void SetClip(PRectangle rc) override {
		for( ; clip_count > 0; --clip_count )
			ImGui::PopClipRect();
		++clip_count;
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
	if( w ) {
		rc.left = w->pos.x;
		rc.right = rc.left + w->size.x;
		rc.top = w->pos.y;
		rc.bottom = rc.top + w->size.y;
	}
	return rc;
}

void Window::SetPosition(PRectangle rc) {
	WindowIM* w = (WindowIM*)wid;
	if( w )	{
		w->pos.x = rc.left;
		w->pos.y = rc.top;
		w->size.x = rc.Width();
		w->size.y = rc.Height();
		if( w->win ) {
			w->win->Pos = w->pos;
			w->win->Size = w->size;
		} else {
			w->modified = true;
		}
	}
}

void Window::SetPositionRelative(PRectangle rc, Window relativeTo) {
	WindowIM* w = (WindowIM*)wid;
	if( w ) {
		WindowIM* wndRelativeTo = (WindowIM*)(relativeTo.GetID());
		XYPOSITION ox = 0;
		XYPOSITION oy = 0;
		PRectangle rcMonitor(0, 0, 1920, 1080);
		if( wndRelativeTo ) {
			ox = wndRelativeTo->pos.x;
			oy = wndRelativeTo->pos.y;
			rcMonitor.left = ox;
			rcMonitor.top = oy;
			rcMonitor.right = ox + wndRelativeTo->size.x;
			rcMonitor.bottom = oy + wndRelativeTo->size.y;
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

		w->pos.x = ox;
		w->pos.y = oy;
		w->size.x = ox + sizex;
		w->size.y = oy + sizey;
		if( w->win ) {
			w->win->Pos = w->pos;
			w->win->Size = w->size;
		} else {
			w->modified = true;
		}
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

const char* MENU_TITLE = "PussIMSciMenu";

class MenuIM {
public:
	struct Item {
		const char*	name;
		int			cmd;
		bool		enabled;
	};
public:
	Point			point;
	ImVector<Item>	items;
};

Menu::Menu() : mid(0) {
}

void Menu::CreatePopUp() {
	MenuIM* m = new MenuIM;
	mid = (MenuID)m;
}

void Menu::Destroy() {
	delete (MenuIM*)mid;
	mid = 0;
}

void Menu::Show(Point pt, Window &w) {
	MenuIM* m = (MenuIM*)mid;
	if( !m )
		return;
	m->point = pt;
	ImGui::OpenPopup(MENU_TITLE);
}

ColourDesired Platform::Chrome() {
	return ColourDesired(0xe0, 0xe0, 0xe0);
}

ColourDesired Platform::ChromeHighlight() {
	return ColourDesired(0xff, 0xff, 0xff);
}

const char *Platform::DefaultFont() {
#ifdef _WIN32
	return "Lucida Console";
#else
	return "!Sans";
#endif
}

int Platform::DefaultFontSize() {
#ifdef _WIN32
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
		, scrollDirty(false)
		, scrollActive(false)
		, scrollThumbnailWidth(0.0f)
		, scrollOffsetY(0.0f)
		, scrollPageTop(0.0f)
		, scrollPageBottom(0.0f)
		, rectangularSelectionModifier(SCMOD_ALT)
		, notifyCallback(NULL)
		, notifyCallbackUD(0)
	{
		view.bufferedDraw = false;
		wMain = (WindowID)&mainWindow;
		mainWindow.size.x = 800;
		mainWindow.size.y = 600;
		for(int i=0; i<tickPlatform; ++i) {
			timerIntervals[i] = 0;
			timers[i] = 0.0f;
		}
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
				if( mainWindow.win ) {
					ImGui::SetWindowFocus();
					ImGui::FocusWindow(mainWindow.win);
				}
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
		ImGuiWindow* window = mainWindow.win;
		const bool focus_requested = ImGui::FocusableItemRegister(window, id);
		// const bool focus_requested_by_code = focus_requested && (window->FocusIdxAllCounter == window->FocusIdxAllRequestCurrent);
		// const bool focus_requested_by_tab = focus_requested && !focus_requested_by_code;
		const bool user_clicked = hovered && io.MouseClicked[0];
		const bool user_scrolled = hovered && (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f);
	    if( focus_requested || user_clicked || user_scrolled ) {
			// fprintf(stderr, "grab focus!\n");
			ImGui::SetActiveID(id, window);
			ImGui::SetFocusID(id, window);
			ImGui::FocusWindow(window);
		} else if( ImGui::GetActiveID()==id ) {
			ImGui::ClearActiveID();
		}

		// mouse click handler
		if( hovered ) {
			if( io.MousePos.x < (wRect.right - scrollThumbnailWidth) ) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
				if( ImGui::IsMouseClicked(0) ) {
					Point click_pos(io.MousePos.x - wRect.left, io.MousePos.y - wRect.top);
					ButtonDownWithModifiers(click_pos, now, modifiers);
					// fprintf(stderr, "mouse down\n");
				}
			} else {
				if( ImGui::IsMouseClicked(0) ) {
					scrollActive = true;
					float my = io.MousePos.y - wRect.top;
					//fprintf(stderr, "%.0f %.0f %.0f\n", my, scrollPageTop, scrollPageBottom);
					if( my>=scrollPageTop && my<=scrollPageBottom ) {
						scrollOffsetY = (scrollPageTop + scrollPageBottom)/2 - my;
					} else {
						scrollOffsetY = 0.0f;
					}
				}
			}
		}
		if( ImGui::IsMouseReleased(0) ) {
			// fprintf(stderr, "mouse up\n");
			Point pt(io.MousePos.x - wRect.left, io.MousePos.y - wRect.top);
			ButtonUpWithModifiers(pt, now, modifiers);
		}
		if( hovered && ImGui::IsMouseClicked(1) && (io.MousePos.x < (wRect.right - scrollThumbnailWidth)) ) {
			Point pt(io.MousePos.x - wRect.left, io.MousePos.y - wRect.top);
			if(!PointInSelection(pt))
				SetEmptySelection(PositionFromLocation(pt));
			if(ShouldDisplayPopup(pt)) {
				ContextMenu(Point(pt.x, pt.y));
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
		if( !io.InputQueueCharacters.empty() ) {
            // Process text input (before we check for Return because using some IME will effectively send a Return?)
            // We ignore CTRL inputs, but need to allow ALT+CTRL as some keyboards (e.g. German) use AltGR (which _is_ Alt+Ctrl) to input certain characters.
            if( !(io.KeyCtrl && !io.KeyAlt) && (!pdoc->IsReadOnly()) ) {
				char utf8[6];
				ClearSelection();
				for( int i=0; i<io.InputQueueCharacters.size(); ++i ) {
					const ImWchar* wstr = &io.InputQueueCharacters[i];
					int len = ImTextStrToUtf8(utf8, 6, wstr, wstr+1);
					AddCharUTF(utf8, len);
				}
			}
			io.ClearInputCharacters();
		}

        // Handle key-presses
        const bool is_shortcut_key_only = (io.ConfigMacOSXBehaviors ? (io.KeySuper && !io.KeyCtrl) : (io.KeyCtrl && !io.KeySuper)) && !io.KeyAlt && !io.KeyShift; // OS X style: Shortcuts using Cmd/Super instead of Ctrl
        const bool is_ctrl_key_only = io.KeyCtrl && !io.KeyShift && !io.KeyAlt && !io.KeySuper;
        const bool is_shift_key_only = io.KeyShift && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper;

		TryKeyDownWithModifiers(ImGuiKey_LeftArrow, SCK_LEFT, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_RightArrow, SCK_RIGHT, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_UpArrow, SCK_UP, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_DownArrow, SCK_DOWN, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_Home, SCK_HOME, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_End, SCK_END, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_PageUp, SCK_PRIOR, modifiers);
		TryKeyDownWithModifiers(ImGuiKey_PageDown, SCK_NEXT, modifiers);
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
	void HandleInputEvents(ImGuiWindow* window) {
		ImGuiIO& io = ImGui::GetIO();
		unsigned int now = GetCurrentTime();
		int modifiers = ModifierFlags(io.KeyShift, io.KeyCtrl, io.KeyAlt, io.KeySuper);
		PRectangle wRect = wMain.GetPosition();
		bool hovered = ImGui::ItemHoverable(window->ClipRect, window->ID);
		if( hovered || HaveMouseCapture() ) {
			HandleMouseEvents(io, window->ID, hovered, wRect, now, modifiers);
		}
		bool focused = ImGui::IsWindowFocused();
		if( focused != hasFocus ) {
			SetFocusState(focused);
			scrollActive = false;
		}
		if( hasFocus ) {
			HandleKeyboardEvents(io, now, modifiers);
		}
		if( scrollActive ) {
			if( ImGui::IsMouseReleased(0) ) {
				scrollActive = false;
			} else {
				float totalHeight = (float)((pdoc->LinesTotal() + 1) * vs.lineHeight);
				float y = (io.MousePos.y + scrollOffsetY - window->Pos.y);
				float h = window->Size.y;
				if( window->ScrollbarX )
					h -= ImGui::GetStyle().ScrollbarSize;
				ImGui::SetScrollY((totalHeight*y/h) - (h*0.5f));
			}
		}
	}
	void UpdateWindow(ImGuiWindow* window) {
		float totalHeight = (float)((pdoc->LinesTotal() + 1) * vs.lineHeight);
		float totalWidth = (float)(scrollWidth * vs.aveCharWidth);
		ImVec2 total_sz(window->ClipRect.GetSize());
		total_sz.x -= scrollThumbnailWidth;

		horizontalScrollBarVisible = (totalWidth > total_sz.x);
		verticalScrollBarVisible = (totalHeight > total_sz.y);

		// scrollbar
		if( horizontalScrollBarVisible )
			total_sz.x = totalWidth;
		if( verticalScrollBarVisible )
			total_sz.y = totalHeight;
		if( verticalScrollBarVisible || horizontalScrollBarVisible )
			ImGui::Dummy(total_sz);

		if( scrollDirty ) {
			ImGui::SetScrollX(scroll.x);
			ImGui::SetScrollY(scroll.y);
		} else {
			scroll = window->Scroll;
		}
		SetXYScroll(XYScrollPosition(scroll.x / vs.aveCharWidth, scroll.y / vs.lineHeight));
		scrollDirty = false;
		// fprintf(stderr, "scroll pos: %d, %d\n", xOffset, topLine);
		mainWindow.pos = window->Pos;
		mainWindow.size = window->Size;

		// ime
		if( hasFocus ) {
			ImGuiContext* ctx = ImGui::GetCurrentContext();
			const Point pos = PointMainCaret();
			ctx->PlatformImePos.x = window->Pos.x + pos.x - 1;
			ctx->PlatformImePos.y = window->Pos.y + pos.y;
			ctx->PlatformImePosViewport = window->Viewport;
		}

		// timers
		ImGuiIO& io = ImGui::GetIO();
		for( int i=0; i<tickPlatform; ++i ) {
			if( timerIntervals[i] ) {
				timers[i] -= io.DeltaTime;
				if( timers[i] <= 0.0f ) {
					timers[i] = ((float)timerIntervals[i]) * 0.001f;
					TickFor((TickReason)i);
				}
			}
		}

		// idle
		idler.state = idler.state && Idle();
	}
	void RenderThumbnail(Surface* surface, const PRectangle& rcArea) {
		view.posCache.Clear();
		ViewStyle style(vs);
		style.technology = SC_TECHNOLOGY_DEFAULT;
		style.ms.clear();
		style.fixedColumnWidth = 0;
		style.zoomLevel = -15;
		style.viewIndentationGuides = ivNone;
		style.selColours.back.isSet = false;
		style.selColours.fore.isSet = false;
		style.selAlpha = SC_ALPHA_NOALPHA;
		style.selAdditionalAlpha = SC_ALPHA_NOALPHA;
		style.whitespaceColours.back.isSet = false;
		style.whitespaceColours.fore.isSet = false;
		style.showCaretLineBackground = false;
		style.alwaysShowCaretLineBackground = false;
		style.braceHighlightIndicatorSet = false;
		style.braceBadLightIndicatorSet = false;

		Sci::Position epos = pdoc->Length();
		Sci::Line line_num = pdoc->LineFromPosition(epos) + 1;
		float lineHeight = rcArea.Height() / line_num;
		PRectangle rc = rcArea;
		surface->SetClip(rc);
		surface->FillRectangle(rc, ColourDesired(0xe8, 0xe8, 0xe0));

		rc.top = (mainWindow.win->Scroll.y) / vs.lineHeight * lineHeight;
		rc.bottom = (mainWindow.win->Scroll.y + mainWindow.size.y) / vs.lineHeight * lineHeight;
		surface->RoundedRectangle(rc, ColourDesired(0, 0, 0), ColourDesired(0xff, 0xff, 0xff));
		scrollPageTop = rc.top;
		scrollPageBottom = rc.bottom;

		Sci::Line current_line = pdoc->LineFromPosition(CurrentPosition());
		rc.top = lineHeight * current_line;
		rc.bottom = rc.top + style.lineHeight + 1.0f;
		surface->RoundedRectangle(rc, ColourDesired(0, 0, 0xff), ColourDesired(0xe8, 0xe8, 0xe0));

		style.leftMarginWidth = 0;
		style.rightMarginWidth = 0;
		style.Refresh(*surface, pdoc->tabInChars);

		pdoc->EnsureStyledTo(epos);

		float xoffset = rcArea.left + 8.0f;
		int width = (int)rcArea.Width();
		float lastDrawPos = -99999.0f;
		ColourDesired background(0, 0, 0);
		for( Sci::Line line = 0; line<line_num; ++line ) {
			const Sci::Position posLineStart = static_cast<Sci::Position>(pdoc->LineStart(line));
			const Sci::Position posLineEnd = static_cast<Sci::Position>(pdoc->LineEnd(line));
			rc.top = rcArea.top + lineHeight * line;
			if( (rc.top - lastDrawPos) >= 2.0f ) {
				lastDrawPos = rc.top;
				rc.bottom = rc.top + style.lineHeight;
				LineLayout ll(static_cast<int>(pdoc->LineStart(line + 1) - posLineStart + 1));
				view.LayoutLine(*this, line, surface, style, &ll, width);
				view.DrawForeground(surface, *this, style, &ll, line, rc, ll.SubLineRange(0), posLineStart, xoffset, line, background);
			}

			for (const Decoration *deco : pdoc->decorations.View()) {
				Sci::Position startPos = posLineStart;
				if (!deco->rs.ValueAt(startPos)) {
					startPos = deco->rs.EndRun(startPos);
				}
				if ((startPos < posLineEnd) && (deco->rs.ValueAt(startPos))) {
					PRectangle rcMarker(rc.left+1.0f, rc.top-2.0f, rc.left+6.0f, rc.top+2.0f);
					surface->FillRectangle(rcMarker, ColourDesired(0x00, 0x80, 0x00));
				}
			}
		}
		view.posCache.Clear();
	}
	void RenderPopup() {
		MenuIM* m = (MenuIM*)popup.GetID();
		if( m && ImGui::BeginPopupContextItem(MENU_TITLE) ) {
			for( int i=0; i<m->items.size(); ++i ) {
				const char* name = m->items[i].name;
				bool* selected = NULL;
				bool enabled = m->items[i].enabled;
				if( !name[0] ) {
					ImGui::Separator();
				}else if( ImGui::MenuItem(name, NULL, selected, enabled) ) {
					ImGui::CloseCurrentPopup();
					Command(m->items[i].cmd);
				}
			}
			ImGui::EndPopup();
		}
	}
	void Update(int draw, ScintillaIMCallback cb, void* ud) {
		ImGuiContext* context = ImGui::GetCurrentContext();
		ImGuiWindow* window = context ? ImGui::GetCurrentWindow() : NULL;
		if( !window )
			return;
		notifyCallback = cb;
		notifyCallbackUD = ud;
		float scale = (context->FontSize / context->Font->FontSize);
		bool thumbnail = (draw > 1) && (mainWindow.size.x > 256.0f);
		if( mainWindow.font_scale != scale ) {
			mainWindow.font_scale = scale;
			InvalidateStyleData();
		}
		mainWindow.win = window;
		scrollThumbnailWidth = thumbnail ? 72.0f : 0.0f;
		HandleInputEvents(window);
		UpdateWindow(window);
		ImGuiStyle& style = ImGui::GetStyle();
		AutoSurface surface(this);
		PRectangle rc(0.0f, 0.0f, mainWindow.size.x, mainWindow.size.y);
		if( window->ScrollbarY )	rc.right -= style.ScrollbarSize;
		if( window->ScrollbarX )	rc.bottom -= style.ScrollbarSize;
		if( thumbnail ) {
			rc.right -= scrollThumbnailWidth;
			Paint(surface, rc);
			rc.left = rc.right;
			rc.right += scrollThumbnailWidth;
			RenderThumbnail(surface, rc);
		} else {
			Paint(surface, rc);
		}
		RenderPopup();
		mainWindow.win = NULL;
		notifyCallback = NULL;
		notifyCallbackUD = NULL;
	}
	void DirtyScroll() {
		scrollDirty = true;
	}
private:
	sptr_t DefWndProc(unsigned int iMessage, uptr_t wParam, sptr_t lParam) override {
		return 0;
	}
	bool FineTickerRunning(TickReason reason) override {
		return timerIntervals[reason] > 0;
	}
	void FineTickerStart(TickReason reason, int millis, int tolerance) override {
		timerIntervals[reason] = millis;
		timers[reason] = ((float)millis) * 0.001f;
	}
	void FineTickerCancel(TickReason reason) override {
		timerIntervals[reason] = 0;
	}
	bool SetIdle(bool on) override {
		idler.state = on;
		return on;
	}
	void SetMouseCapture(bool on) override {
		captureMouse = on;
	}
	bool HaveMouseCapture() override {
		return captureMouse;
	}
	PRectangle GetClientRectangle() const override {
		PRectangle rc(0.0f, 0.0f, mainWindow.size.x, mainWindow.size.y);
		if( ImGui::GetCurrentContext() ) {
			ImGuiStyle& style = ImGui::GetStyle();
			if( scrollThumbnailWidth > 0.1 ) {
				rc.right -= scrollThumbnailWidth;
			} else if (verticalScrollBarVisible) {
				rc.right -= style.ScrollbarSize;
			}
			if (horizontalScrollBarVisible && !Wrapping()) {
				rc.bottom -= style.ScrollbarSize;
			}
		}
		return rc;
	}
	void SetVerticalScrollPos() override {
		scroll.y = topLine * vs.lineHeight;
		scrollDirty = true;
	}
	void SetHorizontalScrollPos() override {
		scroll.x = xOffset * vs.aveCharWidth;
		scrollDirty = true;
	}
	bool ModifyScrollBars(Sci::Line nMax, Sci::Line nPage) override {
		scroll.y = topLine * vs.lineHeight;
		scroll.x = xOffset * vs.aveCharWidth;
		scrollDirty = true;
		return false;
	}
	void NotifyChange() override {
	}
	void NotifyParent(SCNotification scn) override {
		if( notifyCallback ) {
			notifyCallback(this, &scn, notifyCallbackUD);
		}
	}
	
	void Copy() override {
		SelectionText selectedText;
		CopySelectionRange(&selectedText, true);
		CopyToClipboard(selectedText);
	}

#ifdef _WIN32
	bool CanPaste() override {
		if (!Editor::CanPaste())
			return false;
		if (::IsClipboardFormatAvailable(CF_TEXT))
			return true;
		if (IsUnicodeMode())
			return ::IsClipboardFormatAvailable(CF_UNICODETEXT) != 0;
		return false;
	}

	class GlobalMemory {
		HGLOBAL hand;
	public:
		void *ptr;
		GlobalMemory() : hand(0), ptr(0) {
		}
		explicit GlobalMemory(HGLOBAL hand_) : hand(hand_), ptr(0) {
			if (hand) {
				ptr = ::GlobalLock(hand);
			}
		}
		~GlobalMemory() {
			PLATFORM_ASSERT(!ptr);
			assert(!hand);
		}
		void Allocate(size_t bytes) {
			assert(!hand);
			hand = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
			if (hand) {
				ptr = ::GlobalLock(hand);
			}
		}
		HGLOBAL Unlock() {
			PLATFORM_ASSERT(ptr);
			HGLOBAL handCopy = hand;
			::GlobalUnlock(hand);
			ptr = 0;
			hand = 0;
			return handCopy;
		}
		void SetClip(UINT uFormat) {
			::SetClipboardData(uFormat, Unlock());
		}
		operator bool() const {
			return ptr != 0;
		}
		SIZE_T Size() {
			return ::GlobalSize(hand);
		}
	};

	// OpenClipboard may fail if another application has opened the clipboard.
	// Try up to 8 times, with an initial delay of 1 ms and an exponential back off
	// for a maximum total delay of 127 ms (1+2+4+8+16+32+64).
	static bool OpenClipboardRetry(HWND hwnd) {
		for (int attempt=0; attempt<8; attempt++) {
			if (attempt > 0) {
				::Sleep(1 << (attempt-1));
			}
			if (::OpenClipboard(hwnd)) {
				return true;
			}
		}
		return false;
	}

	void Paste() override {
		HWND hWnd = (HWND)__scintilla_imgui_os_window();
		if (!OpenClipboardRetry(hWnd)) {
			return;
		}
		UndoGroup ug(pdoc);
		const bool isLine = SelectionEmpty() &&
			(::IsClipboardFormatAvailable(cfLineSelect) || ::IsClipboardFormatAvailable(cfVSLineTag));
		ClearSelection(multiPasteMode == SC_MULTIPASTE_EACH);
		bool isRectangular = (::IsClipboardFormatAvailable(cfColumnSelect) != 0);

		if (!isRectangular) {
			// Evaluate "Borland IDE Block Type" explicitly
			GlobalMemory memBorlandSelection(::GetClipboardData(cfBorlandIDEBlockType));
			if (memBorlandSelection) {
				isRectangular = (memBorlandSelection.Size() == 1) && (static_cast<BYTE *>(memBorlandSelection.ptr)[0] == 0x02);
				memBorlandSelection.Unlock();
			}
		}
		const PasteShape pasteShape = isRectangular ? pasteRectangular : (isLine ? pasteLine : pasteStream);

		// Always use CF_UNICODETEXT if available
		GlobalMemory memUSelection(::GetClipboardData(CF_UNICODETEXT));
		if (memUSelection) {
			wchar_t *uptr = static_cast<wchar_t *>(memUSelection.ptr);
			if (uptr) {
				size_t len;
				std::vector<char> putf;
				// Default Scintilla behaviour in Unicode mode
				if (IsUnicodeMode()) {
					const size_t bytes = memUSelection.Size();
					len = UTF8Length(uptr, bytes / 2);
					putf.resize(len + 1);
					UTF8FromUTF16(uptr, bytes / 2, &putf[0], len);
				} else {
					// CF_UNICODETEXT available, but not in Unicode mode
					// Convert from Unicode to current Scintilla code page
					len = ::WideCharToMultiByte(CP_UTF8, 0, uptr, -1,
												NULL, 0, NULL, NULL) - 1; // subtract 0 terminator
					putf.resize(len + 1);
					::WideCharToMultiByte(CP_UTF8, 0, uptr, -1,
											  &putf[0], static_cast<int>(len) + 1, NULL, NULL);
				}

				InsertPasteShape(&putf[0], static_cast<int>(len), pasteShape);
			}
			memUSelection.Unlock();
		} else {
			// CF_UNICODETEXT not available, paste ANSI text
			GlobalMemory memSelection(::GetClipboardData(CF_TEXT));
			if (memSelection) {
				char *ptr = static_cast<char *>(memSelection.ptr);
				if (ptr) {
					const size_t bytes = memSelection.Size();
					size_t len = bytes;
					for (size_t i = 0; i < bytes; i++) {
						if ((len == bytes) && (0 == ptr[i]))
							len = i;
					}
					const int ilen = static_cast<int>(len);

					// In Unicode mode, convert clipboard text to UTF-8
					if (IsUnicodeMode()) {
						std::vector<wchar_t> uptr(len+1);

						const size_t ulen = ::MultiByteToWideChar(CP_ACP, 0,
											ptr, ilen, &uptr[0], ilen +1);

						const size_t mlen = UTF8Length(&uptr[0], ulen);
						std::vector<char> putf(mlen+1);
						UTF8FromUTF16(&uptr[0], ulen, &putf[0], mlen);

						InsertPasteShape(&putf[0], static_cast<int>(mlen), pasteShape);
					} else {
						InsertPasteShape(ptr, ilen, pasteShape);
					}
				}
				memSelection.Unlock();
			}
		}
		::CloseClipboard();
		EnsureCaretVisible();
		Redraw();
	}

	void CopyToClipboard(const SelectionText &selectedText) override {
		HWND hWnd = (HWND)__scintilla_imgui_os_window();
		if (!OpenClipboardRetry(hWnd)) {
			return;
		}
		::EmptyClipboard();

		GlobalMemory uniText;

		// Default Scintilla behaviour in Unicode mode
		if (IsUnicodeMode()) {
			size_t uchars = UTF16Length(selectedText.Data(),
				selectedText.LengthWithTerminator());
			uniText.Allocate(2 * uchars);
			if (uniText) {
				UTF16FromUTF8(selectedText.Data(), selectedText.LengthWithTerminator(),
					static_cast<wchar_t *>(uniText.ptr), uchars);
			}
		} else {
			// Not Unicode mode
			// Convert to Unicode using the current Scintilla code page
			int uLen = ::MultiByteToWideChar(CP_UTF8, 0, selectedText.Data(),
				static_cast<int>(selectedText.LengthWithTerminator()), 0, 0);
			uniText.Allocate(2 * uLen);
			if (uniText) {
				::MultiByteToWideChar(CP_UTF8, 0, selectedText.Data(),
					static_cast<int>(selectedText.LengthWithTerminator()),
					static_cast<wchar_t *>(uniText.ptr), uLen);
			}
		}

		if (uniText) {
			uniText.SetClip(CF_UNICODETEXT);
		} else {
			// There was a failure - try to copy at least ANSI text
			GlobalMemory ansiText;
			ansiText.Allocate(selectedText.LengthWithTerminator());
			if (ansiText) {
				memcpy(static_cast<char *>(ansiText.ptr), selectedText.Data(), selectedText.LengthWithTerminator());
				ansiText.SetClip(CF_TEXT);
			}
		}

		if (selectedText.rectangular) {
			::SetClipboardData(cfColumnSelect, 0);

			GlobalMemory borlandSelection;
			borlandSelection.Allocate(1);
			if (borlandSelection) {
				static_cast<BYTE *>(borlandSelection.ptr)[0] = 0x02;
				borlandSelection.SetClip(cfBorlandIDEBlockType);
			}
		}

		if (selectedText.lineCopy) {
			::SetClipboardData(cfLineSelect, 0);
			::SetClipboardData(cfVSLineTag, 0);
		}

		::CloseClipboard();
	}

#else
	void CopyToClipboard(const SelectionText &selectedText) override {
		ImGui::SetClipboardText(selectedText.Data());
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
#endif
	void CreateCallTipWindow(PRectangle rc) override {
	}
	void AddToPopUp(const char *label, int cmd = 0, bool enabled = true) override {
		MenuIM* m = (MenuIM*)popup.GetID();
		if( !m ) return;
		MenuIM::Item item = { label, cmd, enabled };
		m->items.push_back(item);
	}
	void ClaimSelection() override {
	}
private:
	bool captureMouse;
	bool scrollDirty;
	bool scrollActive;
	float scrollThumbnailWidth;
	float scrollOffsetY;
	float scrollPageTop;
	float scrollPageBottom;
	ImVec2 scroll;
	int rectangularSelectionModifier;
	WindowIM mainWindow;
	ScintillaIMCallback notifyCallback;
	void* notifyCallbackUD;
	int timerIntervals[tickPlatform];
	float timers[tickPlatform];
};

ScintillaIM* scintilla_imgui_create() {
	return new ScintillaIM();
}

void scintilla_imgui_destroy(ScintillaIM* sci) {
	delete sci;
}

void scintilla_imgui_update(ScintillaIM* sci, int draw, ScintillaIMCallback cb, void* ud) {
	if( sci ) {
		if( draw ) {
			sci->Update(draw, cb, ud);
		} else if( cb ) {
			cb(sci, NULL, ud);
		}
	}
}

sptr_t scintilla_imgui_send(ScintillaIM* sci, unsigned int iMessage, uptr_t wParam, sptr_t lParam) {
	return sci ? sci->WndProc(iMessage, wParam, lParam) : NULL;
}

void scintilla_imgui_dirty_scroll(ScintillaIM* sci) {
	if( sci ) { sci->DirtyScroll(); }
}
