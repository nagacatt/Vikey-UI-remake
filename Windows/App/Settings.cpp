// Vikey windows on Windows (Reze Edition): full-bleed Reze artwork canvas with optical frosted glass cards.
#define _HAS_STD_BYTE 0
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <objidl.h>
#include <d2d1.h>
#include <dwrite.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <functional>
#define GDIPVER 0x0110
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include "Settings.h"
#include "Core.h"
#include "Prefs.h"
#include "KeyMap.h"
#include "Emoji.h"
#include "resource.h"
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")
using namespace Gdiplus;

namespace vikey { namespace ui {

struct Palette {
    COLORREF bg, panelSolid, field, fieldHover, border, sep, text, secondary, tertiary, accent, accentHover, accentInk, emerald, amber;
    BYTE glassAlphaTop, glassAlphaBot;
    COLORREF glassTintTop, glassTintBot, glassRim, glassHighlight;
};

static const Palette DARK_PAL = {
    RGB(11, 16, 27),     // bg: #0B101B
    RGB(17, 25, 40),     // panelSolid: #111928
    RGB(24, 36, 56),     // field: #182438
    RGB(34, 50, 76),     // fieldHover: #22324C
    RGB(94, 118, 153),   // border: #5E7699 (3.59:1 vs glass)
    RGB(42, 58, 84),     // sep
    RGB(244, 247, 251),  // text: #F4F7FB (15.52:1 vs glass)
    RGB(176, 192, 216),  // secondary: #B0C0D8 (9.03:1 vs glass)
    RGB(143, 164, 194),  // tertiary
    RGB(88, 196, 246),   // accent: #58C4F6 (Bioluminescent Sky Cyan)
    RGB(135, 214, 250),  // accentHover
    RGB(11, 16, 27),     // accentInk: #0B101B (9.64:1 vs accent)
    RGB(52, 211, 153),   // emerald: #34D399
    RGB(251, 191, 36),   // amber: #FBBF24
    194, 212,
    RGB(16, 24, 40), RGB(11, 17, 29),
    RGB(130, 185, 240), RGB(255, 255, 255)
};

static const Palette LIGHT_PAL = {
    RGB(232, 241, 252),  // bg: #E8F1FC
    RGB(247, 250, 255),  // panelSolid: #F7FAFF
    RGB(224, 235, 248),  // field: #E0EBF8
    RGB(208, 224, 244),  // fieldHover: #D0E0F4
    RGB(91, 112, 139),   // border: #5B708B (4.09:1 vs glass)
    RGB(196, 210, 228),  // sep
    RGB(15, 23, 42),     // text: #0F172A (14.39:1 vs glass)
    RGB(51, 65, 85),     // secondary: #334155 (8.35:1 vs glass)
    RGB(82, 98, 122),    // tertiary
    RGB(37, 99, 235),    // accent: #2563EB (Royal Azure)
    RGB(29, 78, 216),    // accentHover
    RGB(255, 255, 255),  // accentInk: #FFFFFF (5.17:1 vs accent)
    RGB(5, 150, 105),    // emerald: #059669
    RGB(180, 83, 9),     // amber: #B45309
    218, 208,
    RGB(255, 255, 255), RGB(240, 247, 255),
    RGB(140, 175, 215), RGB(255, 255, 255)
};

bool isDarkTheme() { return prefs::getInt(L"RezeTheme", 0) == 0; }
static const Palette& pal() { return isDarkTheme() ? DARK_PAL : LIGHT_PAL; }

static HFONT fBody, fBodyBold, fSmall, fSmallBold, fWordmark, fTitle, fMono, fTitle2, fCallout, fMonoBold, fTag;
static const int W = 480, MARGIN = 18, ROW = 36, GAP = 12, CARD_RADIUS = 16;
static ULONG_PTR gdipToken = 0;
static HINSTANCE _inst;
static Bitmap* _arts[4] = {nullptr, nullptr, nullptr, nullptr};
static const wchar_t* ART_NAMES[4] = {L"Đêm mưa", L"Sứa biển", L"Hoa thủy tiên", L"Lam thanh"};
static HBRUSH _editBrushDark = nullptr, _editBrushLight = nullptr;
static std::map<HWND, HBITMAP> _winCanvas;

static int currentArtIndex() {
    if (prefs::has(L"RezeArtIndex")) return prefs::getInt(L"RezeArtIndex", 0) & 3;
    return isDarkTheme() ? 0 : 2;
}

static Bitmap* loadArt(int resId, const wchar_t* diskName) {
    std::wstring diskPath = std::wstring(L"c:\\ViKey-UI-Remake\\Tài nguyên\\") + diskName;
    if (GetFileAttributesW(diskPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        Bitmap* b = Bitmap::FromFile(diskPath.c_str());
        if (b && b->GetLastStatus() == Ok) return b;
        delete b;
    }
    HRSRC r = FindResourceW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(resId), RT_RCDATA);
    HGLOBAL g = r ? LoadResource(GetModuleHandleW(nullptr), r) : nullptr;
    const void* data = g ? LockResource(g) : nullptr;
    DWORD sz = r ? SizeofResource(GetModuleHandleW(nullptr), r) : 0;
    if (!data || !sz) return nullptr;
    IStream* s = SHCreateMemStream((const BYTE*)data, sz);
    if (!s) return nullptr;
    Bitmap* b = Bitmap::FromStream(s);
    s->Release();
    if (b && b->GetLastStatus() == Ok) return b;
    delete b;
    return nullptr;
}

static void initLook() {
    if (fBody) return;
    GdiplusStartupInput in; GdiplusStartup(&gdipToken, &in, nullptr);
    auto mk = [](int px, int weight, const wchar_t* face = L"Segoe UI") {
        return CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, face);
    };
    fBody = mk(14, FW_NORMAL);
    fBodyBold = mk(14, FW_SEMIBOLD);
    fSmall = mk(12, FW_NORMAL);
    fSmallBold = mk(12, FW_SEMIBOLD);
    fWordmark = mk(25, FW_BOLD);
    fTitle = mk(16, FW_BOLD);
    fMono = mk(13, FW_NORMAL, L"Consolas");
    fTitle2 = mk(19, FW_BOLD);
    fCallout = mk(13, FW_NORMAL);
    fMonoBold = mk(13, FW_BOLD, L"Consolas");
    fTag = mk(11, FW_SEMIBOLD);
    _editBrushDark = CreateSolidBrush(DARK_PAL.field);
    _editBrushLight = CreateSolidBrush(LIGHT_PAL.field);
    _arts[0] = loadArt(IDR_REZE_NIGHT, L"Reze - Chainsawman.jpg");
    _arts[1] = loadArt(IDR_REZE_ABYSS, L"Reze _ Chainsaw Man.jpg");
    _arts[2] = loadArt(IDR_REZE_FLOWER, L"Reze chainsaw man.jpg");
    _arts[3] = loadArt(IDR_REZE_GRAPHIC, L"Reze _ Chainsaw Man (1).jpg");
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_LINK_CLASS | ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);
}

static Color gp(COLORREF c, BYTE a = 255) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }

static void applyWindowChrome(HWND h) {
    if (!h) return;
    BOOL dark = isDarkTheme() ? TRUE : FALSE;
    DwmSetWindowAttribute(h, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    DWORD corner = 2 /* DWMWCP_ROUND */;
    DwmSetWindowAttribute(h, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner, sizeof(corner));
    const Palette& p = pal();
    COLORREF caption = isDarkTheme() ? RGB(13, 19, 32) : RGB(236, 244, 255);
    DwmSetWindowAttribute(h, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));
    DwmSetWindowAttribute(h, 36 /* DWMWA_TEXT_COLOR */, &p.text, sizeof(p.text));
}

static void makeRoundPath(GraphicsPath& p, RectF r, float radius) {
    if (radius <= 0.5f) { p.AddRectangle(r); return; }
    float d = std::min({radius * 2.0f, r.Width, r.Height});
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
}

static void roundBox(Graphics& g, RectF r, float radius, Color fill, Color stroke, float strokeW = 1.0f) {
    GraphicsPath p;
    makeRoundPath(p, r, radius);
    SolidBrush b(fill);
    g.FillPath(&b, &p);
    if (stroke.GetA()) {
        Pen pen(stroke, strokeW);
        g.DrawPath(&pen, &p);
    }
}

static void drawArtCover(Graphics& g, Bitmap* bmp, RectF dest, float focusX = 0.5f, float focusY = 0.25f) {
    if (!bmp || bmp->GetWidth() == 0 || bmp->GetHeight() == 0) return;
    float bw = (float)bmp->GetWidth(), bh = (float)bmp->GetHeight();
    float scale = std::max(dest.Width / bw, dest.Height / bh);
    float sw = dest.Width / scale, sh = dest.Height / scale;
    float sx = (bw - sw) * focusX, sy = (bh - sh) * focusY;
    GraphicsState st = g.Save();
    g.SetClip(dest, CombineModeIntersect);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.DrawImage(bmp, dest, sx, sy, sw, sh, UnitPixel);
    g.Restore(st);
}

/// Generates a creamy 3-pass separable Gaussian-like blurred Bitmap for optical frosted glass rendering.
static Bitmap* createBlurredWallpaper(Bitmap* art, int w, int h, float focusX = 0.5f, float focusY = 0.28f) {
    int sw = std::max(1, w / 2), sh = std::max(1, h / 2);
    Bitmap* smallBmp = new Bitmap(sw, sh, PixelFormat32bppARGB);
    {
        Graphics g(smallBmp);
        if (art) drawArtCover(g, art, RectF(0, 0, (float)sw, (float)sh), focusX, focusY);
        else { SolidBrush b(gp(pal().bg)); g.FillRectangle(&b, 0, 0, sw, sh); }
    }
    BitmapData bd;
    Rect rect(0, 0, sw, sh);
    if (smallBmp->LockBits(&rect, ImageLockModeRead | ImageLockModeWrite, PixelFormat32bppARGB, &bd) == Ok) {
        std::vector<DWORD> tmp(sw * sh);
        DWORD* px = (DWORD*)bd.Scan0;
        int rad = 7;
        for (int pass = 0; pass < 3; pass++) {
            for (int y = 0; y < sh; y++) {
                for (int x = 0; x < sw; x++) {
                    int r = 0, gr = 0, b = 0, cnt = 0;
                    for (int k = -rad; k <= rad; k++) {
                        int nx = std::min(sw - 1, std::max(0, x + k));
                        DWORD c = px[y * sw + nx];
                        r += (c >> 16) & 255; gr += (c >> 8) & 255; b += c & 255; cnt++;
                    }
                    tmp[y * sw + x] = 0xFF000000 | ((r / cnt) << 16) | ((gr / cnt) << 8) | (b / cnt);
                }
            }
            for (int y = 0; y < sh; y++) {
                for (int x = 0; x < sw; x++) {
                    int r = 0, gr = 0, b = 0, cnt = 0;
                    for (int k = -rad; k <= rad; k++) {
                        int ny = std::min(sh - 1, std::max(0, y + k));
                        DWORD c = tmp[ny * sw + x];
                        r += (c >> 16) & 255; gr += (c >> 8) & 255; b += c & 255; cnt++;
                    }
                    px[y * sw + x] = 0xFF000000 | ((r / cnt) << 16) | ((gr / cnt) << 8) | (b / cnt);
                }
            }
        }
        smallBmp->UnlockBits(&bd);
    }
    Bitmap* fullBmp = new Bitmap(w, h, PixelFormat32bppARGB);
    {
        Graphics g(fullBmp);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.DrawImage(smallBmp, RectF(0, 0, (float)w, (float)h), 0, 0, (float)sw, (float)sh, UnitPixel);
    }
    delete smallBmp;
    return fullBmp;
}

/// Draws a frosted glass card over the blurred wallpaper with a translucent gradient scrim and 1px specular rim.
static void drawGlassCard(Graphics& g, Bitmap* blurredBg, RectF r, float radius) {
    const Palette& p = pal();
    GraphicsPath path;
    makeRoundPath(path, r, radius);
    GraphicsState st = g.Save();
    g.SetClip(&path);
    if (blurredBg) g.DrawImage(blurredBg, r, r.X, r.Y, r.Width, r.Height, UnitPixel);
    LinearGradientBrush tint(r, gp(p.glassTintTop, p.glassAlphaTop), gp(p.glassTintBot, p.glassAlphaBot), LinearGradientModeForwardDiagonal);
    g.FillPath(&tint, &path);
    g.Restore(st);

    // Specular inner-top rim highlight
    GraphicsPath innerPath;
    makeRoundPath(innerPath, RectF(r.X + 1.0f, r.Y + 1.0f, r.Width - 2.0f, r.Height - 2.0f), radius - 1.0f);
    Pen hiPen(gp(p.glassHighlight, isDarkTheme() ? 38 : 170), 1.0f);
    g.DrawPath(&hiPen, &innerPath);

    // Outer glass edge
    Pen rimPen(gp(p.glassRim, isDarkTheme() ? 78 : 145), 1.1f);
    g.DrawPath(&rimPen, &path);
}

static void saveWindowCanvas(HWND h, HDC srcDc, int w, int hgt) {
    if (_winCanvas.count(h) && _winCanvas[h]) DeleteObject(_winCanvas[h]);
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP copy = CreateCompatibleBitmap(screen, w, hgt);
    HGDIOBJ old = SelectObject(mem, copy);
    BitBlt(mem, 0, 0, w, hgt, srcDc, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    _winCanvas[h] = copy;
}

static void dropWindowCanvas(HWND h) {
    auto it = _winCanvas.find(h);
    if (it != _winCanvas.end()) {
        if (it->second) DeleteObject(it->second);
        _winCanvas.erase(it);
    }
}

/// Copies the parent window's composed glass/artwork canvas into a child control's DC for zero-seam transparency.
static void blitParentCanvas(HWND childHwnd, HDC childMemDc, const RECT& childRc) {
    HWND parent = GetParent(childHwnd);
    auto it = _winCanvas.find(parent);
    if (it != _winCanvas.end() && it->second) {
        POINT pt = {0, 0};
        MapWindowPoints(childHwnd, parent, &pt, 1);
        HDC pMem = CreateCompatibleDC(childMemDc);
        HGDIOBJ old = SelectObject(pMem, it->second);
        BitBlt(childMemDc, 0, 0, childRc.right, childRc.bottom, pMem, pt.x, pt.y, SRCCOPY);
        SelectObject(pMem, old);
        DeleteDC(pMem);
    } else {
        HBRUSH bg = CreateSolidBrush(pal().panelSolid);
        FillRect(childMemDc, &childRc, bg);
        DeleteObject(bg);
    }
}

static std::wstring hotkeyDisplay(int hk) {
    std::wstring s;
    if (HAS_CONTROL(hk)) s += L"Ctrl+"; if (HAS_OPTION(hk)) s += L"Alt+"; if (HAS_SHIFT(hk)) s += L"Shift+"; if (HAS_COMMAND(hk)) s += L"Win+";
    int k = GET_SWITCH_KEY(hk);
    if (k != 0xFE) {
        Uint16 ch = keyCodeToCharacter(k | CAPS_MASK);
        if (k == KEY_SPACE) s += L"Space"; else if (k == KEY_RETURN) s += L"Enter"; else if (k == KEY_TAB) s += L"Tab"; else if (ch) s += (wchar_t)ch; else s += L"?";
    } else if (!s.empty()) s.pop_back();
    return s.empty() ? L"Chưa đặt" : s;
}
static int currentHotkey() { int hk = prefs::getInt(L"SwitchKeyStatus", 0); return hk ? hk : 0x7A000206; }
static int textWidth(HDC dc, HFONT f, const std::wstring& s) { SelectObject(dc, f); SIZE sz = {}; GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz); return sz.cx; }
static int textHeight(HFONT f, const std::wstring& s, int width) {
    HDC dc = GetDC(nullptr); SelectObject(dc, f); RECT r = {0, 0, width, 0}; DrawTextW(dc, s.c_str(), -1, &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX); ReleaseDC(nullptr, dc); return r.bottom;
}

static void drawWordmark(HDC dc, HFONT f, int x, int y) {
    const Palette& p = pal();
    SelectObject(dc, f); SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, p.text); TextOutW(dc, x, y, L"Vi", 2);
    int viW = textWidth(dc, f, L"Vi");
    SetTextColor(dc, p.accent); TextOutW(dc, x + viW, y, L"key", 3);
}

static void colorText(HDC dc, const std::wstring& s, RECT r, float px, COLORREF c) {
    static ID2D1Factory* d2d = nullptr; static IDWriteFactory* dw = nullptr; static ID2D1DCRenderTarget* rt = nullptr;
    if (!d2d) D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d);
    if (!dw) DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dw);
    if (!rt && d2d) { D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96, 96); d2d->CreateDCRenderTarget(&props, &rt); }
    if (!rt || !dw) { DrawTextW(dc, s.c_str(), -1, &r, DT_WORDBREAK | DT_NOPREFIX); return; }
    IDWriteTextFormat* f = nullptr; dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, px, L"vi", &f);
    if (!f) return;
    rt->BindDC(dc, &r); rt->BeginDraw(); rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    ID2D1SolidColorBrush* b = nullptr; rt->CreateSolidColorBrush(D2D1::ColorF(GetRValue(c) / 255.0f, GetGValue(c) / 255.0f, GetBValue(c) / 255.0f), &b);
    if (b) { rt->DrawTextW(s.c_str(), (UINT32)s.size(), f, D2D1::RectF(0, 0, (float)(r.right - r.left), (float)(r.bottom - r.top)), b, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT); b->Release(); }
    if (rt->EndDraw() == D2DERR_RECREATE_TARGET) { rt->Release(); rt = nullptr; }
    f->Release();
}

static void centerOnMouseScreen(HWND h) {
    POINT p; GetCursorPos(&p); MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST), &mi);
    RECT r; GetWindowRect(h, &r); int w = r.right - r.left, hh = r.bottom - r.top; RECT a = mi.rcWork;
    int x = (a.left + a.right - w) / 2, y = (a.top + a.bottom - hh) / 2;
    if (y < a.top) y = a.top; if (x < a.left) x = a.left;
    SetWindowPos(h, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
static void bringToFront(HWND h) { ShowWindow(h, SW_SHOW); SetForegroundWindow(h); }

struct Seg { std::wstring text; std::wstring url; HFONT font; COLORREF color; };
struct Link { RECT r; std::wstring url; };
static int drawSegs(HDC dc, const std::vector<Seg>& segs, int x, int y, int width, bool center, std::vector<Link>& links) {
    int total = 0, h = 0;
    for (auto& s : segs) { total += textWidth(dc, s.font, s.text); TEXTMETRICW tm; GetTextMetricsW(dc, &tm); h = std::max(h, (int)tm.tmHeight); }
    int cx = center ? x + (width - total) / 2 : x;
    SetBkMode(dc, TRANSPARENT);
    const Palette& p = pal();
    for (auto& s : segs) {
        int w = textWidth(dc, s.font, s.text);
        SetTextColor(dc, s.url.empty() ? s.color : p.accent);
        TextOutW(dc, cx, y, s.text.c_str(), (int)s.text.size());
        if (!s.url.empty()) {
            links.push_back({{cx, y, cx + w, y + h}, s.url});
        }
        cx += w;
    }
    return h;
}
static const Link* linkAt(const std::vector<Link>& links, LPARAM lp) { POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; for (auto& l : links) if (PtInRect(&l.r, p)) return &l; return nullptr; }

static void trackMouseHover(HWND h) {
    if (!GetPropW(h, L"vkhover")) {
        SetPropW(h, L"vkhover", (HANDLE)1);
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, h, 0};
        TrackMouseEvent(&tme);
        InvalidateRect(h, nullptr, FALSE);
    }
}

/// Smooth Glass Capsule Toggle (`44x24`, `r=12`) with size-differentiated circular thumb and subtle choker-ring detail.
static LRESULT CALLBACK toggleProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        blitParentCanvas(h, mem, rc);
        const Palette& p = pal();
        Graphics g(mem); g.SetSmoothingMode(SmoothingModeAntiAlias);
        bool on = GetWindowLongPtrW(h, GWLP_USERDATA) != 0, enabled = IsWindowEnabled(h) != 0;
        bool hover = GetPropW(h, L"vkhover") != nullptr, focused = GetFocus() == h;
        Color trackFill = on ? gp(hover ? p.accentHover : p.accent, enabled ? 245 : 105)
                             : gp(hover ? p.fieldHover : p.field, enabled ? 185 : 95);
        Color stroke = focused ? gp(p.emerald) : (on ? gp(p.accent, enabled ? 255 : 110) : gp(p.border, enabled ? 200 : 100));
        roundBox(g, RectF(0.5f, 0.5f, (float)rc.right - 1.0f, (float)rc.bottom - 1.0f), 11.5f, trackFill, stroke, focused ? 2.0f : 1.1f);
        if (on) {
            float d = 18.0f, kx = (float)rc.right - d - 3.0f, ky = ((float)rc.bottom - d) / 2.0f;
            SolidBrush shB(Color(45, 0, 0, 0)); g.FillEllipse(&shB, kx, ky + 1.0f, d, d);
            SolidBrush kb(Color(255, 255, 255, 255)); g.FillEllipse(&kb, kx, ky, d, d);
            Pen ringPen(gp(p.accent, 190), 1.5f); g.DrawEllipse(&ringPen, kx + 5.5f, ky + 5.5f, 7.0f, 7.0f);
        } else {
            float d = 14.0f, kx = 5.0f, ky = ((float)rc.bottom - d) / 2.0f;
            SolidBrush kb(gp(enabled ? p.secondary : p.tertiary, 230)); g.FillEllipse(&kb, kx, ky, d, d);
        }
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: trackMouseHover(h); return 0;
    case WM_MOUSELEAVE: RemovePropW(h, L"vkhover"); InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_SETFOCUS: case WM_KILLFOCUS: InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_GETDLGCODE: return DLGC_BUTTON | DLGC_WANTCHARS;
    case WM_LBUTTONUP: case WM_KEYDOWN:
        if (m == WM_KEYDOWN && w != VK_SPACE && w != VK_RETURN) break;
        if (!IsWindowEnabled(h)) return 0;
        SetFocus(h);
        SetWindowLongPtrW(h, GWLP_USERDATA, !GetWindowLongPtrW(h, GWLP_USERDATA)); InvalidateRect(h, nullptr, FALSE);
        SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_ENABLE: InvalidateRect(h, nullptr, FALSE); break;
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, IsWindowEnabled(h) ? IDC_HAND : IDC_NO)); return TRUE;
    case WM_NCDESTROY: RemovePropW(h, L"vkhover"); break;
    }
    return DefWindowProcW(h, m, w, l);
}
static bool toggleOn(HWND h) { return h && GetWindowLongPtrW(h, GWLP_USERDATA) != 0; }
static void setToggle(HWND h, bool on) { if (h) { SetWindowLongPtrW(h, GWLP_USERDATA, on); InvalidateRect(h, nullptr, FALSE); } }

/// Frosted Glass Segmented Bar (`Tiếng Việt` | `English`) with smooth rounded pill indicator.
static LRESULT CALLBACK segProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    static const wchar_t* labels[2] = {L"Tiếng Việt", L"English"};
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        blitParentCanvas(h, mem, rc);
        const Palette& p = pal();
        Graphics g(mem); g.SetSmoothingMode(SmoothingModeAntiAlias);
        bool focused = GetFocus() == h;
        roundBox(g, RectF(0.5f, 0.5f, (float)rc.right - 1.0f, (float)rc.bottom - 1.0f), 11.0f,
                 gp(p.panelSolid, isDarkTheme() ? 185 : 210), focused ? gp(p.emerald) : gp(p.glassRim, 110), focused ? 2.0f : 1.0f);
        int sel = (int)GetWindowLongPtrW(h, GWLP_USERDATA); float half = rc.right / 2.0f;
        roundBox(g, RectF(sel * half + 3.0f, 3.0f, half - 6.0f, (float)rc.bottom - 6.0f), 8.5f, gp(p.accent), gp(p.glassHighlight, 70), 1.0f);
        SelectObject(mem, fBodyBold); SetBkMode(mem, TRANSPARENT);
        for (int i = 0; i < 2; i++) {
            RECT r = {(LONG)(i * half), 0, (LONG)((i + 1) * half), rc.bottom};
            SetTextColor(mem, i == sel ? p.accentInk : p.text);
            DrawTextW(mem, labels[i], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case WM_LBUTTONUP: {
        SetFocus(h);
        RECT rc; GetClientRect(h, &rc); int i = GET_X_LPARAM(l) < rc.right / 2 ? 0 : 1;
        if (i != (int)GetWindowLongPtrW(h, GWLP_USERDATA)) { SetWindowLongPtrW(h, GWLP_USERDATA, i); InvalidateRect(h, nullptr, FALSE); SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h); }
        return 0; }
    case WM_KEYDOWN:
        if (w == VK_LEFT || w == VK_RIGHT || w == VK_SPACE || w == VK_RETURN) {
            int cur = (int)GetWindowLongPtrW(h, GWLP_USERDATA);
            int next = (w == VK_LEFT) ? 0 : (w == VK_RIGHT) ? 1 : (1 - cur);
            if (next != cur) { SetWindowLongPtrW(h, GWLP_USERDATA, next); InvalidateRect(h, nullptr, FALSE); SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h); }
            return 0;
        }
        break;
    case WM_SETFOCUS: case WM_KILLFOCUS: InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_GETDLGCODE: return DLGC_WANTARROWS | DLGC_BUTTON;
    case WM_ERASEBKGND: return 1;
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE;
    }
    return DefWindowProcW(h, m, w, l);
}

/// Rounded Frosted Glass Button (`r = 9`): prominent (luminous accent pill) or translucent glass surface.
static LRESULT CALLBACK buttonProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        blitParentCanvas(h, mem, rc);
        const Palette& p = pal();
        bool prominent = GetWindowLongPtrW(h, GWLP_USERDATA) != 0, enabled = IsWindowEnabled(h) != 0;
        bool down = GetPropW(h, L"vkdown") != nullptr, hover = GetPropW(h, L"vkhover") != nullptr, focused = GetFocus() == h;
        Color fill = prominent ? gp(hover || down ? p.accentHover : p.accent, enabled ? 245 : 95)
                               : gp(down ? p.sep : (hover ? p.fieldHover : p.panelSolid), isDarkTheme() ? (hover ? 215 : 175) : (hover ? 235 : 205));
        Color stroke = focused ? gp(p.emerald) : (prominent ? gp(p.glassHighlight, 90) : gp(hover ? p.accent : p.glassRim, isDarkTheme() ? 110 : 155));
        Graphics g(mem); g.SetSmoothingMode(SmoothingModeAntiAlias);
        roundBox(g, RectF(0.5f, 0.5f, (float)rc.right - 1.0f, (float)rc.bottom - 1.0f), 9.0f, fill, stroke, focused ? 2.0f : 1.0f);
        wchar_t t[64]; GetWindowTextW(h, t, 64);
        SelectObject(mem, prominent ? fBodyBold : fSmallBold); SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, prominent ? p.accentInk : (enabled ? p.text : p.tertiary));
        DrawTextW(mem, t, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: trackMouseHover(h); return 0;
    case WM_MOUSELEAVE: RemovePropW(h, L"vkhover"); InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_SETFOCUS: case WM_KILLFOCUS: InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_GETDLGCODE: return DLGC_BUTTON | DLGC_WANTCHARS;
    case WM_KEYDOWN:
        if ((w == VK_SPACE || w == VK_RETURN) && IsWindowEnabled(h)) {
            SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN: if (IsWindowEnabled(h)) { SetFocus(h); SetPropW(h, L"vkdown", (HANDLE)1); SetCapture(h); InvalidateRect(h, nullptr, FALSE); } return 0;
    case WM_LBUTTONUP: {
        bool was = GetPropW(h, L"vkdown") != nullptr; RemovePropW(h, L"vkdown"); ReleaseCapture(); InvalidateRect(h, nullptr, FALSE);
        RECT rc; GetClientRect(h, &rc); POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        if (was && PtInRect(&rc, p) && IsWindowEnabled(h)) SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h);
        return 0; }
    case WM_ENABLE: InvalidateRect(h, nullptr, FALSE); break;
    case WM_ERASEBKGND: return 1;
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, IsWindowEnabled(h) ? IDC_HAND : IDC_NO)); return TRUE;
    case WM_NCDESTROY: RemovePropW(h, L"vkdown"); RemovePropW(h, L"vkhover"); break;
    }
    return DefWindowProcW(h, m, w, l);
}

struct SelectData { std::vector<std::wstring> items; int sel = 0; };
static SelectData* selData(HWND h) { return (SelectData*)GetWindowLongPtrW(h, GWLP_USERDATA); }
static LRESULT CALLBACK selectProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    SelectData* d = selData(h);
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        blitParentCanvas(h, mem, rc);
        const Palette& p = pal();
        Graphics g(mem); g.SetSmoothingMode(SmoothingModeAntiAlias);
        bool hover = GetPropW(h, L"vkhover") != nullptr, focused = GetFocus() == h;
        roundBox(g, RectF(0.5f, 0.5f, (float)rc.right - 1.0f, (float)rc.bottom - 1.0f), 9.0f,
                 gp(hover ? p.fieldHover : p.field, 195), focused ? gp(p.emerald) : gp(hover ? p.accent : p.border, 190), focused ? 2.0f : 1.0f);
        SelectObject(mem, fBody); SetBkMode(mem, TRANSPARENT); SetTextColor(mem, p.text);
        std::wstring label = (d && d->sel >= 0 && d->sel < (int)d->items.size()) ? d->items[d->sel] : L"";
        RECT tr = {11, 0, rc.right - 24, rc.bottom};
        DrawTextW(mem, label.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SetTextColor(mem, p.accent);
        RECT ar = {rc.right - 24, 0, rc.right - 4, rc.bottom};
        DrawTextW(mem, L"▾", -1, &ar, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case CB_ADDSTRING: if (d && l) d->items.push_back((const wchar_t*)l); InvalidateRect(h, nullptr, FALSE); return 0;
    case CB_SETCURSEL: if (d) { d->sel = std::max(0, std::min((int)w, (int)d->items.size() - 1)); InvalidateRect(h, nullptr, FALSE); } return 0;
    case CB_GETCURSEL: return d ? d->sel : 0;
    case WM_MOUSEMOVE: trackMouseHover(h); return 0;
    case WM_MOUSELEAVE: RemovePropW(h, L"vkhover"); InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_SETFOCUS: case WM_KILLFOCUS: InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_GETDLGCODE: return DLGC_WANTARROWS | DLGC_BUTTON;
    case WM_KEYDOWN:
        if (!d || d->items.empty()) return 0;
        if (w == VK_UP || w == VK_LEFT) {
            d->sel = (d->sel - 1 + (int)d->items.size()) % (int)d->items.size();
            InvalidateRect(h, nullptr, FALSE);
            SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), CBN_SELCHANGE), (LPARAM)h);
            return 0;
        }
        if (w == VK_DOWN || w == VK_RIGHT) {
            d->sel = (d->sel + 1) % (int)d->items.size();
            InvalidateRect(h, nullptr, FALSE);
            SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), CBN_SELCHANGE), (LPARAM)h);
            return 0;
        }
        if (w != VK_SPACE && w != VK_RETURN) break;
        [[fallthrough]];
    case WM_LBUTTONUP: {
        if (!d || d->items.empty()) return 0;
        SetFocus(h);
        HMENU menu = CreatePopupMenu();
        for (int i = 0; i < (int)d->items.size(); i++) AppendMenuW(menu, MF_STRING | (i == d->sel ? MF_CHECKED : 0), i + 1, d->items[i].c_str());
        RECT wr; GetWindowRect(h, &wr);
        int pick = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, wr.left, wr.bottom, 0, h, nullptr);
        DestroyMenu(menu);
        if (pick > 0 && pick - 1 != d->sel) {
            d->sel = pick - 1;
            InvalidateRect(h, nullptr, FALSE);
            SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), CBN_SELCHANGE), (LPARAM)h);
        }
        return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE;
    case WM_NCDESTROY: delete d; SetWindowLongPtrW(h, GWLP_USERDATA, 0); RemovePropW(h, L"vkhover"); break;
    }
    return DefWindowProcW(h, m, w, l);
}

struct Recorder { const wchar_t* pref; bool modsOnly; bool recording; int value; };
static Recorder* rec(HWND h) { return h ? (Recorder*)GetWindowLongPtrW(h, GWLP_USERDATA) : nullptr; }
static void afterHotkeyChange();
static void commitRecorder(HWND h, int value) {
    Recorder* r = rec(h);
    if (!r) return;
    if (!r->modsOnly) {
        if (!(value & 0x700)) return;
        if ((value & ~0x8000) == (currentHotkey() & ~0x8000)) return;
        r->value = value & ~0x8000;
    } else r->value = value | (currentHotkey() & 0x8000);
    r->recording = false; prefs::setInt(r->pref, r->value); core::reloadSettings(); InvalidateRect(h, nullptr, FALSE);
    SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h);
    afterHotkeyChange();
}
static bool isModVk(DWORD vk) { return vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT || vk == VK_LWIN || vk == VK_RWIN || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_LMENU || vk == VK_RMENU || vk == VK_LSHIFT || vk == VK_RSHIFT; }
static LRESULT CALLBACK hotkeyProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Recorder* r = rec(h);
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        blitParentCanvas(h, mem, rc);
        const Palette& p = pal();
        Graphics g(mem); g.SetSmoothingMode(SmoothingModeAntiAlias);
        bool recording = r && r->recording, hover = GetPropW(h, L"vkhover") != nullptr, focused = GetFocus() == h;
        Color stroke = recording ? gp(p.accent) : (focused ? gp(p.emerald) : gp(hover ? p.accent : p.border, 190));
        roundBox(g, RectF(0.5f, 0.5f, (float)rc.right - 1.0f, (float)rc.bottom - 1.0f), 9.0f, gp(hover ? p.fieldHover : p.field, 195), stroke, (recording || focused) ? 2.0f : 1.0f);
        SelectObject(mem, recording ? fSmallBold : fMonoBold); SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, recording ? p.accent : p.text);
        std::wstring t = recording ? L"Bấm phím tắt…" : (r && r->value == 0) ? L"Chưa đặt" : hotkeyDisplay(r ? r->value : 0);
        DrawTextW(mem, t.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: trackMouseHover(h); return 0;
    case WM_MOUSELEAVE: RemovePropW(h, L"vkhover"); InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_SETFOCUS: InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONUP: if (r) { if (r->recording) { r->recording = false; } else { SetFocus(h); r->recording = true; } InvalidateRect(h, nullptr, FALSE); } return 0;
    case WM_KILLFOCUS: if (r) r->recording = false; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_GETDLGCODE: return DLGC_WANTALLKEYS;
    case WM_CHAR: case WM_SYSCHAR: return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN: {
        if (!r) return 0;
        DWORD vk = (DWORD)w;
        if (!r->recording && (vk == VK_SPACE || vk == VK_RETURN)) { r->recording = true; InvalidateRect(h, nullptr, FALSE); return 0; }
        if (!r->recording) return 0;
        bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0, shift = GetKeyState(VK_SHIFT) < 0, win = GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0;
        if (vk == VK_ESCAPE && !ctrl && !alt && !win && !shift) { r->recording = false; InvalidateRect(h, nullptr, FALSE); return 0; }
        if (isModVk(vk)) return 0;
        int key = (int)engineKey(vk); if (key == KEY_EMPTY) return 0;
        if (!(ctrl || alt || win || shift)) return 0;
        commitRecorder(h, key | (ctrl ? 0x100 : 0) | (alt ? 0x200 : 0) | (win ? 0x400 : 0) | (shift ? 0x800 : 0));
        return 0; }
    case WM_KEYUP: case WM_SYSKEYUP: {
        if (!r || !r->recording || !r->modsOnly) return 0;
        DWORD vk = (DWORD)w; if (!isModVk(vk)) return 0;
        int mods = (vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_CONTROL || GetKeyState(VK_CONTROL) < 0 ? 0x100 : 0) | (vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU || GetKeyState(VK_MENU) < 0 ? 0x200 : 0) |
                   (vk == VK_LWIN || vk == VK_RWIN || GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0 ? 0x400 : 0) | (vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT || GetKeyState(VK_SHIFT) < 0 ? 0x800 : 0);
        int n = !!(mods & 0x100) + !!(mods & 0x200) + !!(mods & 0x400) + !!(mods & 0x800);
        if (n < 2) return 0;
        commitRecorder(h, 0xFE | mods); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE;
    case WM_NCDESTROY: delete r; SetWindowLongPtrW(h, GWLP_USERDATA, 0); RemovePropW(h, L"vkhover"); break;
    }
    return DefWindowProcW(h, m, w, l);
}

/// Delicate Circular (?) Help Icon floating seamlessly over the frosted glass card.
static LRESULT CALLBACK tipProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        blitParentCanvas(h, mem, rc);
        const Palette& p = pal();
        Graphics g(mem); g.SetSmoothingMode(SmoothingModeAntiAlias);
        bool hover = GetPropW(h, L"vkhover") != nullptr;
        Pen circlePen(gp(hover ? p.accent : p.secondary, hover ? 230 : 120), 1.2f);
        g.DrawEllipse(&circlePen, 1.0f, 1.0f, 14.0f, 14.0f);
        SelectObject(mem, fTag); SetBkMode(mem, TRANSPARENT); SetTextColor(mem, hover ? p.accent : p.secondary);
        RECT tr = {0, -1, 16, 16};
        DrawTextW(mem, L"?", 1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: trackMouseHover(h); return 0;
    case WM_MOUSELEAVE: RemovePropW(h, L"vkhover"); InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONUP: { wchar_t t[512]; GetWindowTextW(h, t, 512); MessageBoxW(GetParent(h), t, L"Vikey", MB_OK | MB_ICONINFORMATION); return 0; }
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE;
    case WM_ERASEBKGND: return 1;
    case WM_NCDESTROY: RemovePropW(h, L"vkhover"); break;
    }
    return DefWindowProcW(h, m, w, l);
}

static void registerClasses(HINSTANCE inst) {
    static bool done = false; if (done) return; done = true;
    struct { const wchar_t* name; WNDPROC proc; } cls[] = {
        {L"VikeyToggle", toggleProc}, {L"VikeySegmented", segProc}, {L"VikeyHotkey", hotkeyProc},
        {L"VikeyTip", tipProc}, {L"VikeyButton", buttonProc}, {L"VikeySelect", selectProc}
    };
    for (auto& c : cls) { WNDCLASSW wc = {}; wc.lpfnWndProc = c.proc; wc.hInstance = inst; wc.lpszClassName = c.name; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); RegisterClassW(&wc); }
}
static HWND makeChild(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id, HFONT f = nullptr) {
    HWND c = CreateWindowW(cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    if (f) SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE); return c;
}
static HWND makeRecorder(HWND parent, const wchar_t* pref, bool modsOnly, int value, int x, int y, int w, int h, int id) {
    HWND c = makeChild(parent, L"VikeyHotkey", L"", WS_TABSTOP, x, y, w, h, id);
    SetWindowLongPtrW(c, GWLP_USERDATA, (LONG_PTR)new Recorder{pref, modsOnly, false, value});
    return c;
}
static HWND makeSelect(HWND parent, int x, int y, int w, int h, int id) {
    HWND c = makeChild(parent, L"VikeySelect", L"", WS_TABSTOP, x, y, w, h, id);
    SetWindowLongPtrW(c, GWLP_USERDATA, (LONG_PTR)new SelectData());
    return c;
}
static void startRecording(HWND c) { if (Recorder* r = rec(c)) { SetFocus(c); r->recording = true; InvalidateRect(c, nullptr, FALSE); } }
static void destroyChildren(HWND h) { std::vector<HWND> kids; for (HWND c = GetWindow(h, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) kids.push_back(c); for (HWND c : kids) DestroyWindow(c); }

enum {
    C_SEG = 200, C_THEME, C_ART, C_HOTKEY, C_BEEP, C_AUTO, C_PAUSE, C_SMART, C_SITE, C_MARK, C_SPELL, C_RESTORE,
    C_LOGIN, C_UPDATE, C_LOGKEYS, C_LOGENABLE, C_EMOJI, C_EMOJIKEY, C_FEEDBACK, C_STYLE, C_GUIDE, C_LOGWIN, C_ABOUTWIN,
    L_REFRESH = 300, L_OPEN, L_CLEAR, A_LINK = 400, O_BACK = 500, O_NEXT, O_REC, O_BEEP, O_EMOJI, O_EMOJIKEY, O_EDIT = 520
};
static const UINT WM_REBUILD = WM_APP + 20;
static HWND _win, _seg, _logWin, _logList, _logSummary, _logGrid, _logEmpty, _aboutWin, _onbWin, _ackWin, _tooltip;
static std::map<int, HWND> _ctl;
struct Group { int top, bottom; };
static std::vector<Group> _groups;
static std::vector<std::pair<int, std::wstring>> _titles;
static std::vector<int> _seps;
struct RowText { int x, y; std::wstring text; };
static std::vector<RowText> _rowLabels;

static HWND child(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id, HFONT f = nullptr) { return makeChild(_win, cls, text, style, x, y, w, h, id, f); }
static void addTip(HWND tip, const wchar_t* text) {
    if (!_tooltip) { _tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0, _win, nullptr, nullptr, nullptr); SendMessageW(_tooltip, TTM_SETMAXTIPWIDTH, 0, 320); }
    TOOLINFOW ti = {sizeof(ti)}; ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS; ti.hwnd = _win; ti.uId = (UINT_PTR)tip; ti.lpszText = (LPWSTR)text; SendMessageW(_tooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
}

static int _y; static int _groupTop = -1;
static void beginGroup(const wchar_t* title) {
    if (title) { _titles.push_back({_y, title}); _y += 24; }
    _groupTop = _y;
}
static void endGroup() { _groups.push_back({_groupTop, _y}); _y += GAP; }
static void rowLabel(const wchar_t* label, const wchar_t* tip) {
    if (_y != _groupTop) _seps.push_back(_y);
    int ly = _y + (ROW - 20) / 2;
    _rowLabels.push_back({MARGIN + 16, ly, label});
    HDC dc = GetDC(_win); int lw = textWidth(dc, fBody, label); ReleaseDC(_win, dc);
    if (tip) { HWND t = child(L"VikeyTip", tip, 0, MARGIN + 16 + lw + 7, ly + 2, 16, 16, 0); addTip(t, tip); }
}
static HWND toggleRow(const wchar_t* label, const wchar_t* tip, int id, bool on) {
    rowLabel(label, tip);
    HWND c = child(L"VikeyToggle", L"", WS_TABSTOP, W - MARGIN - 16 - 44, _y + (ROW - 24) / 2, 44, 24, id);
    setToggle(c, on);
    _ctl[id] = c; _y += ROW; return c;
}
static HWND recorderRow(const wchar_t* label, const wchar_t* tip, const wchar_t* pref, bool modsOnly, int value, int id) {
    rowLabel(label, tip);
    HWND c = makeRecorder(_win, pref, modsOnly, value, W - MARGIN - 16 - 136, _y + (ROW - 28) / 2, 136, 28, id);
    _ctl[id] = c; _y += ROW; return c;
}
static HWND selectRow(const wchar_t* label, const wchar_t* tip, int id, int cw, const std::vector<const wchar_t*>& opts, int selIdx) {
    rowLabel(label, tip);
    HWND c = makeSelect(_win, W - MARGIN - 16 - cw, _y + (ROW - 28) / 2, cw, 28, id);
    for (auto* o : opts) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)o);
    SendMessageW(c, CB_SETCURSEL, selIdx, 0);
    _ctl[id] = c; _y += ROW; return c;
}

static void refreshEnabled() {
    if (_ctl[C_PAUSE] && _ctl[C_AUTO]) EnableWindow(_ctl[C_PAUSE], toggleOn(_ctl[C_AUTO]));
    if (_ctl[C_SITE] && _ctl[C_SMART]) EnableWindow(_ctl[C_SITE], toggleOn(_ctl[C_SMART]));
    if (_ctl[C_RESTORE] && _ctl[C_SPELL]) EnableWindow(_ctl[C_RESTORE], toggleOn(_ctl[C_SPELL]));
}

/// Composes the full-window Reze artwork + blurred frosted-glass cards into _winCanvas[_win].
static void composeMainCanvas(HWND h, int winW, int winH) {
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, winW, winH);
    HGDIOBJ old = SelectObject(mem, bmp);

    const Palette& p = pal();
    Graphics g(mem); g.SetSmoothingMode(SmoothingModeAntiAlias);

    int artIdx = currentArtIndex();
    Bitmap* activeArt = _arts[artIdx] ? _arts[artIdx] : _arts[0];
    Bitmap* blurred = createBlurredWallpaper(activeArt, winW, winH, 0.55f, 0.25f);

    // 1. Base blurred wallpaper across the entire window
    if (blurred) g.DrawImage(blurred, 0, 0, winW, winH);
    // 2. Top Hero Stage: crisp, vivid Reze artwork fading smoothly into the blurred lower canvas
    float heroH = 148.0f;
    if (activeArt) drawArtCover(g, activeArt, RectF(0, 0, (float)winW, heroH), 0.58f, 0.22f);

    // Ambient veil over the background so glass cards pop while artwork colors breathe through
    LinearGradientBrush heroFade(RectF(0, 0, (float)winW, heroH + 6.0f),
                                 gp(p.bg, isDarkTheme() ? 85 : 95),
                                 gp(p.bg, isDarkTheme() ? 195 : 205),
                                 LinearGradientModeVertical);
    g.FillRectangle(&heroFade, RectF(0, 0, (float)winW, heroH + 6.0f));
    SolidBrush bodyVeil(gp(p.bg, isDarkTheme() ? 120 : 135));
    g.FillRectangle(&bodyVeil, RectF(0, heroH, (float)winW, (float)winH - heroH));

    // Left radial/horizontal scrim in Hero so Wordmark + subtitle have > 12:1 contrast
    LinearGradientBrush leftScrim(RectF(0, 0, (float)winW * 0.72f, heroH),
                                  gp(p.bg, isDarkTheme() ? 185 : 210),
                                  gp(p.bg, 0),
                                  LinearGradientModeHorizontal);
    g.FillRectangle(&leftScrim, RectF(0, 0, (float)winW * 0.72f, heroH));

    // 3. Frosted Glass Setting Cards (`CARD_RADIUS = 16`)
    for (auto& gr : _groups) {
        RectF card((float)MARGIN, (float)gr.top, (float)(W - 2 * MARGIN), (float)(gr.bottom - gr.top));
        drawGlassCard(g, blurred, card, (float)CARD_RADIUS);
    }

    Pen sepPen(gp(p.sep, isDarkTheme() ? 95 : 135), 1.0f);
    for (int y : _seps) g.DrawLine(&sepPen, MARGIN + 16, y, W - MARGIN - 16, y);

    // 4. Hero Header Content: App Icon, Wordmark, hotkey pill
    HICON icon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 42, 42, LR_DEFAULTCOLOR);
    if (icon) { DrawIconEx(mem, MARGIN + 4, 18, icon, 42, 42, 0, nullptr, DI_NORMAL); DestroyIcon(icon); }
    drawWordmark(mem, fWordmark, MARGIN + 56, 12);

    std::wstring subStr = L"Bấm " + hotkeyDisplay(currentHotkey()) + L" để chuyển ngôn ngữ";
    SelectObject(mem, fSmall); SetBkMode(mem, TRANSPARENT); SetTextColor(mem, p.secondary);
    RECT subR = {MARGIN + 56, 42, MARGIN + 275, 62};
    DrawTextW(mem, subStr.c_str(), -1, &subR, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // 5. Section titles & Row labels
    SelectObject(mem, fSmallBold);
    for (auto& t : _titles) {
        SetTextColor(mem, p.secondary);
        RECT r = {MARGIN + 6, t.first + 4, W - MARGIN, t.first + 22};
        DrawTextW(mem, t.second.c_str(), -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    SelectObject(mem, fBody); SetTextColor(mem, p.text);
    for (auto& rl : _rowLabels) {
        RECT r = {rl.x, rl.y, W - MARGIN - 155, rl.y + 20};
        DrawTextW(mem, rl.text.c_str(), -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    delete blurred;
    saveWindowCanvas(h, mem, winW, winH);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
    ReleaseDC(nullptr, screen);
}

static void paintMain(HWND h) {
    PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
    if (!_winCanvas.count(h) || !_winCanvas[h]) composeMainCanvas(h, rc.right, rc.bottom);
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, _winCanvas[h]);
    BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old); DeleteDC(mem);
    EndPaint(h, &ps);
}

static void buildMain() {
    destroyChildren(_win);
    dropWindowCanvas(_win);
    if (_tooltip) { DestroyWindow(_tooltip); _tooltip = nullptr; }
    _groups.clear(); _titles.clear(); _seps.clear(); _ctl.clear(); _rowLabels.clear();
    applyWindowChrome(_win);

    // Top-right floating glass pills: Wallpaper Switcher + Theme Toggle
    std::wstring artLabel = std::wstring(L"Ảnh: ") + ART_NAMES[currentArtIndex()];
    child(L"VikeyButton", artLabel.c_str(), WS_TABSTOP, W - MARGIN - 230, 16, 124, 26, C_ART);
    child(L"VikeyButton", isDarkTheme() ? L"Chế độ: Đêm" : L"Chế độ: Ngày", WS_TABSTOP, W - MARGIN - 100, 16, 100, 26, C_THEME);

    // Frosted Glass Segmented Language Bar in the Hero stage
    _seg = child(L"VikeySegmented", L"", WS_TABSTOP, MARGIN, 74, 240, 34, C_SEG);
    languageChanged(core::language());

    _y = 122;
    int hk = currentHotkey();
    beginGroup(L"Phím tắt & âm báo");
    recorderRow(L"Phím chuyển Anh/Việt", L"Bấm tổ hợp phím này để chuyển giữa tiếng Việt và tiếng Anh. Bấm vào ô để đặt phím khác.", L"SwitchKeyStatus", true, hk, C_HOTKEY);
    toggleRow(L"Âm báo khi chuyển", L"Phát một âm ngắn mỗi lần bạn bấm phím chuyển Anh/Việt: lên giọng khi sang tiếng Việt, xuống giọng khi sang tiếng Anh.", C_BEEP, HAS_BEEP(hk) != 0);
    endGroup();

    beginGroup(L"Tự chuyển song ngữ");
    toggleRow(L"Tự chuyển Anh/Việt", L"Gõ một từ tiếng Anh khi đang ở tiếng Việt (hoặc ngược lại), Vikey tự sửa từ đó và chuyển chế độ. Bấm Xoá ngay sau đó để hoàn tác.", C_AUTO, prefs::getInt(L"AutoSwitchByTyping", 1) != 0);
    toggleRow(L"Tạm dừng 5 giây", L"Khi bạn tự bấm chuyển Anh/Việt, Vikey không tự chuyển trong 5 giây, để giữ đúng lựa chọn của bạn.", C_PAUSE, prefs::getInt(L"PauseAutoAfterManual", 1) != 0);
    toggleRow(L"Nhớ theo ứng dụng", L"Vikey nhớ bạn dùng tiếng Anh hay tiếng Việt ở mỗi ứng dụng và tự chuyển lại khi bạn quay về ứng dụng đó.", C_SMART, prefs::getInt(L"UseSmartSwitchKey", 1) != 0);
    toggleRow(L"Nhớ theo trang web", L"Trong Chrome, Edge, Brave, Vivaldi, Opera, Firefox: nhớ tiếng Anh hay tiếng Việt cho từng trang web. Chỉ đọc địa chỉ tab đang mở, ngay trên máy bạn.", C_SITE, prefs::getInt(L"SmartSwitchPerSite", 1) != 0);
    endGroup();

    beginGroup(L"Kiểu gõ & chính tả");
    selectRow(L"Kiểu gõ", L"Telex: [ ra ơ, ] ra ư, w ra ư. Telex đơn giản 1: [ ] là dấu ngoặc, w đứng riêng vẫn là w. Telex đơn giản 2: [ ] là dấu ngoặc, w vẫn ra ư. Cả ba kiểu đều gõ uw, ow, aw ra ư, ơ, ă.",
              C_STYLE, 154, {L"Telex", L"Telex đơn giản 1", L"Telex đơn giản 2"}, std::min(2, std::max(0, prefs::getInt(L"TelexStyle", 0))));
    selectRow(L"Đặt dấu", L"Vị trí dấu thanh với oa, oe, uy: kiểu mới (hoà, thuý) hoặc kiểu cũ (hòa, thúy).",
              C_MARK, 136, {L"hoà, thuý", L"hòa, thúy"}, prefs::getInt(L"ModernOrthography", 1) ? 0 : 1);
    toggleRow(L"Kiểm tra chính tả", L"Chỉ bỏ dấu khi từ đang gõ đúng quy tắc chính tả tiếng Việt.", C_SPELL, prefs::getInt(L"Spelling", 1) != 0);
    toggleRow(L"Trả lại phím khi sai", L"Khi gõ xong một từ sai chính tả tiếng Việt, Vikey trả lại đúng các phím bạn đã gõ.", C_RESTORE, prefs::getInt(L"RestoreIfInvalidWord", 1) != 0);
    endGroup();

    beginGroup(L"Bảng emoji & hệ thống");
    bool emojiOn = prefs::getInt(L"EmojiPicker", 0) != 0;
    toggleRow(L"Bảng emoji song ngữ", L"Bấm phím tắt để mở bảng emoji ở giữa màn hình. Tìm bằng tiếng Việt hoặc tiếng Anh, gõ Telex vẫn tìm được: \"ngựa\", \"horse\" đều ra 🐴. Enter để chèn, Esc để đóng.", C_EMOJI, emojiOn);
    HWND emojiRec = nullptr; int emojiKey = prefs::getInt(L"EmojiHotkey", 0);
    if (emojiOn) emojiRec = recorderRow(L"Phím mở bảng emoji", L"Một phím kèm Ctrl, Alt hoặc Win, ví dụ Ctrl+Alt+E. Bấm vào ô để đặt phím khác.", L"EmojiHotkey", false, emojiKey, C_EMOJIKEY);
    toggleRow(L"Mở Vikey khi đăng nhập", L"Tự mở Vikey mỗi khi bạn đăng nhập vào Windows.", C_LOGIN, prefs::launchAtLogin());
    toggleRow(L"Tự động cập nhật", L"Mỗi ngày Vikey kiểm tra bản mới trên vikey.app, tự tải về và cài khi bạn thoát Vikey hoặc tắt máy.", C_UPDATE, prefs::getInt(L"AutoUpdate", 1) != 0);
    endGroup();

    int btnW = (W - 2 * MARGIN - 3 * 8) / 4;
    int bx = MARGIN;
    child(L"VikeyButton", L"Hướng dẫn…", WS_TABSTOP, bx, _y + 2, btnW, 30, C_GUIDE); bx += btnW + 8;
    child(L"VikeyButton", L"Nhật ký gõ…", WS_TABSTOP, bx, _y + 2, btnW, 30, C_LOGWIN); bx += btnW + 8;
    child(L"VikeyButton", L"Giới thiệu", WS_TABSTOP, bx, _y + 2, btnW, 30, C_ABOUTWIN); bx += btnW + 8;
    child(L"VikeyButton", L"Gửi góp ý…", WS_TABSTOP, bx, _y + 2, btnW, 30, C_FEEDBACK);
    _y += 46;

    refreshEnabled();
    RECT wr = {0, 0, W, _y}; AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);
    SetWindowPos(_win, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    composeMainCanvas(_win, W, _y);
    RedrawWindow(_win, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    if (emojiRec && emojiKey == 0 && GetForegroundWindow() == _win) startRecording(emojiRec);
}

static LRESULT CALLBACK mainProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: paintMain(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_COMMAND: {
        int id = LOWORD(wp); HWND c = (HWND)lp;
        auto flag = [&](const wchar_t* pref) { prefs::setInt(pref, toggleOn(c)); core::reloadSettings(); refreshEnabled(); };
        switch (id) {
        case C_SEG: core::setLanguageByUser(GetWindowLongPtrW(_seg, GWLP_USERDATA) == 0 ? 1 : 0); break;
        case C_THEME: toggleTheme(); break;
        case C_ART: {
            int nextArt = (currentArtIndex() + 1) & 3;
            prefs::setInt(L"RezeArtIndex", nextArt);
            emoji::themeChanged();
            buildMain();
            if (_onbWin) InvalidateRect(_onbWin, nullptr, TRUE);
            if (_aboutWin) InvalidateRect(_aboutWin, nullptr, TRUE);
            if (_ackWin) InvalidateRect(_ackWin, nullptr, TRUE);
            break; }
        case C_HOTKEY: dropWindowCanvas(h); InvalidateRect(h, nullptr, TRUE); break;
        case C_BEEP: { int hk = (currentHotkey() & ~0x8000) | (toggleOn(c) ? 0x8000 : 0); prefs::setInt(L"SwitchKeyStatus", hk); if (Recorder* r = rec(_ctl[C_HOTKEY])) r->value = hk; core::reloadSettings(); afterHotkeyChange(); break; }
        case C_AUTO: flag(L"AutoSwitchByTyping"); break;
        case C_PAUSE: flag(L"PauseAutoAfterManual"); break;
        case C_SMART: flag(L"UseSmartSwitchKey"); break;
        case C_SITE: flag(L"SmartSwitchPerSite"); break;
        case C_STYLE: if (HIWORD(wp) == CBN_SELCHANGE) { prefs::setInt(L"TelexStyle", (int)SendMessageW(c, CB_GETCURSEL, 0, 0)); core::reloadSettings(); } break;
        case C_MARK: if (HIWORD(wp) == CBN_SELCHANGE) { prefs::setInt(L"ModernOrthography", SendMessageW(c, CB_GETCURSEL, 0, 0) == 0 ? 1 : 0); core::reloadSettings(); } break;
        case C_SPELL: flag(L"Spelling"); break;
        case C_RESTORE: flag(L"RestoreIfInvalidWord"); break;
        case C_EMOJI: prefs::setInt(L"EmojiPicker", toggleOn(c)); core::reloadSettings(); emoji::settingsChanged(); afterHotkeyChange(); break;
        case C_LOGIN: prefs::setLaunchAtLogin(toggleOn(c)); setToggle(c, prefs::launchAtLogin()); break;
        case C_UPDATE: prefs::setInt(L"AutoUpdate", toggleOn(c)); break;
        case C_GUIDE: showOnboarding(_inst); break;
        case C_LOGWIN: showTypingLog(_inst); break;
        case C_ABOUTWIN: showAbout(_inst); break;
        case C_FEEDBACK: ShellExecuteW(nullptr, L"open", L"https://vikey.app/feedback?platform=windows&v=" VIKEY_VERSION_W, nullptr, nullptr, SW_SHOWNORMAL); break;
        }
        return 0; }
    case WM_REBUILD: buildMain(); return 0;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: if (_tooltip) DestroyWindow(_tooltip); dropWindowCanvas(h); _win = nullptr; _seg = nullptr; _tooltip = nullptr; _ctl.clear(); _groups.clear(); _titles.clear(); _seps.clear(); _rowLabels.clear(); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void languageChanged(int lang) {
    if (_seg) { SetWindowLongPtrW(_seg, GWLP_USERDATA, lang ? 0 : 1); InvalidateRect(_seg, nullptr, FALSE); }
    if (_onbWin) InvalidateRect(_onbWin, nullptr, FALSE);
    emoji::languageChanged(lang);
}

static void buildOnboarding();
static void applyLogTheme();

void toggleTheme() {
    prefs::setInt(L"RezeTheme", isDarkTheme() ? 1 : 0);
    emoji::themeChanged();
    if (_win) buildMain();
    if (_onbWin) { applyWindowChrome(_onbWin); buildOnboarding(); }
    if (_aboutWin) { applyWindowChrome(_aboutWin); dropWindowCanvas(_aboutWin); InvalidateRect(_aboutWin, nullptr, TRUE); }
    if (_ackWin) { applyWindowChrome(_ackWin); InvalidateRect(_ackWin, nullptr, TRUE); }
    if (_logWin) { applyWindowChrome(_logWin); applyLogTheme(); InvalidateRect(_logWin, nullptr, TRUE); }
}

void showSettings(HINSTANCE inst, bool firstRun) {
    _inst = inst; initLook(); registerClasses(inst);
    if (firstRun) { showOnboarding(inst); return; }
    if (_win) { bringToFront(_win); return; }
    static bool reg = false;
    if (!reg) { WNDCLASSW wc = {}; wc.lpfnWndProc = mainProc; wc.hInstance = inst; wc.lpszClassName = L"VikeyMain"; wc.hbrBackground = nullptr; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP)); RegisterClassW(&wc); reg = true; }
    _win = CreateWindowExW(0, L"VikeyMain", L"Vikey", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN, 0, 0, W, 620, nullptr, nullptr, inst, nullptr);
    buildMain();
    centerOnMouseScreen(_win);
    bringToFront(_win);
}

enum OnbStep { S_WELCOME, S_PRACTICE, S_HOTKEY, S_EMOJI, S_COUNT };
static int _step = S_WELCOME;
static const int OW = 670, OH = 470, OP = 20, SIDE_W = 196, CONTENT_X = OP + SIDE_W + 22, OCW = OW - CONTENT_X - OP - 12;
static const struct { const wchar_t* keys; const wchar_t* result; } PRACTICE[3] = {
    {L"order traf suwax nhes", L"order trà sữa nhé"},
    {L"meeting lucs ba giowf nhes", L"meeting lúc ba giờ nhé"},
    {L"thanks, camr own nhieeuf", L"thanks, cảm ơn nhiều"}
};
static HWND _practiceEdit[3]; static bool _practiceOk[3];
static int _oy[8];

static std::wstring normalize(const std::wstring& s) {
    std::wstring t = s; while (!t.empty() && t.back() == L' ') t.pop_back(); size_t a = t.find_first_not_of(L' '); t = a == std::wstring::npos ? L"" : t.substr(a);
    if (t.empty()) return t;
    int n = NormalizeString(NormalizationC, t.c_str(), (int)t.size(), nullptr, 0); std::wstring c(n > 0 ? n : 0, 0);
    if (n > 0) { n = NormalizeString(NormalizationC, t.c_str(), (int)t.size(), &c[0], n); c.resize(n > 0 ? n : 0); } else c = t;
    if (!c.empty()) CharLowerBuffW(&c[0], (DWORD)c.size()); return c;
}
static std::wstring practiceText() {
    return core::language() == 1 ? L"Gõ đúng các phím bên dưới, kể cả tiếng Anh. Vikey bỏ dấu cho tiếng Việt và để nguyên tiếng Anh."
                                 : L"Bạn đang ở tiếng Anh: bấm " + hotkeyDisplay(currentHotkey()) + L" để sang tiếng Việt, rồi gõ các phím bên dưới.";
}
static LRESULT CALLBACK practiceEditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR) {
    if (m == WM_KEYDOWN && w == VK_RETURN) { core::resetTyping(); int i = (int)id; SetFocus(_practiceEdit[std::min(i + 1, 2)]); return 0; }
    if (m == WM_CHAR && (w == L'\r' || w == L'\n')) return 0;
    return DefSubclassProc(h, m, w, l);
}
static void onbGo(int s) { _step = s; buildOnboarding(); }
static HWND onbButton(const wchar_t* text, bool prominent, int x, int w, int id) {
    HWND b = makeChild(_onbWin, L"VikeyButton", text, WS_TABSTOP, x, OH - OP - 44, w, 32, id);
    SetWindowLongPtrW(b, GWLP_USERDATA, prominent);
    return b;
}
static void drawCheck(Graphics& g, float x, float y, bool ok) {
    const Palette& p = pal();
    if (ok) {
        SolidBrush b(gp(p.emerald)); g.FillEllipse(&b, x, y, 22.0f, 22.0f);
        Pen pen(Color(255, 255, 255, 255), 2.2f);
        g.DrawLine(&pen, x + 6.0f, y + 11.5f, x + 9.5f, y + 15.0f);
        g.DrawLine(&pen, x + 9.5f, y + 15.0f, x + 16.0f, y + 7.5f);
    } else {
        Pen pen(gp(p.border, 175), 1.6f); g.DrawEllipse(&pen, x + 1.0f, y + 1.0f, 20.0f, 20.0f);
    }
}

static void composeOnboardingCanvas(HWND h) {
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, OW, OH);
    HGDIOBJ old = SelectObject(mem, bmp);

    const Palette& p = pal();
    Graphics g(mem); g.SetSmoothingMode(SmoothingModeAntiAlias);
    SetBkMode(mem, TRANSPARENT);

    Bitmap* bgArt = _arts[1] ? _arts[1] : _arts[0];
    Bitmap* blurred = createBlurredWallpaper(bgArt, OW, OH, 0.5f, 0.3f);
    if (blurred) g.DrawImage(blurred, 0, 0, OW, OH);
    SolidBrush veil(gp(p.bg, isDarkTheme() ? 115 : 135));
    g.FillRectangle(&veil, 0, 0, OW, OH);

    // Left rounded portrait card showcasing Reze & bioluminescent jellyfish
    RectF sideCard((float)OP, (float)OP, (float)SIDE_W, (float)(OH - 2 * OP));
    GraphicsPath sidePath; makeRoundPath(sidePath, sideCard, (float)CARD_RADIUS);
    GraphicsState st = g.Save();
    g.SetClip(&sidePath);
    if (bgArt) drawArtCover(g, bgArt, sideCard, 0.5f, 0.24f);
    LinearGradientBrush botScrim(RectF(sideCard.X, sideCard.Y + sideCard.Height - 84.0f, sideCard.Width, 84.0f),
                                 Color(0, 10, 15, 25), Color(230, 10, 15, 25), LinearGradientModeVertical);
    g.FillRectangle(&botScrim, RectF(sideCard.X, sideCard.Y + sideCard.Height - 84.0f, sideCard.Width, 84.0f));
    g.Restore(st);
    Pen sideRim(gp(p.glassRim, 110), 1.2f);
    g.DrawPath(&sideRim, &sidePath);

    static const wchar_t* STEP_NAMES[4] = {L"Bước 1 / 4 · Bảng Telex", L"Bước 2 / 4 · Gõ thử", L"Bước 3 / 4 · Phím tắt", L"Bước 4 / 4 · Bảng Emoji"};
    SelectObject(mem, fSmallBold); SetTextColor(mem, RGB(244, 247, 251));
    RECT snR = {OP + 14, OH - OP - 50, OP + SIDE_W - 14, OH - OP - 30};
    DrawTextW(mem, STEP_NAMES[_step], -1, &snR, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    roundBox(g, RectF((float)(OP + 14), (float)(OH - OP - 24), (float)(SIDE_W - 28), 6.0f), 3.0f, Color(90, 255, 255, 255), Color(0, 0, 0, 0));
    roundBox(g, RectF((float)(OP + 14), (float)(OH - OP - 24), (float)(SIDE_W - 28) * (_step + 1) / (float)S_COUNT, 6.0f), 3.0f, gp(p.accent), Color(0, 0, 0, 0));

    // Right frosted glass card
    RectF mainCard((float)(CONTENT_X - 12), (float)OP, (float)(OW - CONTENT_X + 12 - OP), (float)(OH - 2 * OP));
    drawGlassCard(g, blurred, mainCard, (float)CARD_RADIUS);

    HICON icon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 40, 40, LR_DEFAULTCOLOR);
    if (icon) { DrawIconEx(mem, CONTENT_X + 2, OP + 14, icon, 40, 40, 0, nullptr, DI_NORMAL); DestroyIcon(icon); }
    drawWordmark(mem, fWordmark, CONTENT_X + 52, OP + 8);
    SelectObject(mem, fSmall); SetTextColor(mem, p.secondary);
    RECT tr = {CONTENT_X + 52, OP + 36, OW - OP - 80, OP + 54};
    DrawTextW(mem, L"Gõ tiếng Việt, tự chuyển Anh/Việt cho bạn.", -1, &tr, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    for (int i = 0; i < S_COUNT; i++) {
        SolidBrush dotB(i == _step ? gp(p.accent) : gp(p.border, 120));
        float w = (i == _step) ? 18.0f : 7.0f;
        float x = (float)(OW - OP - 24 - (S_COUNT - i) * 14) - (i == _step ? 8.0f : 0.0f);
        roundBox(g, RectF(x, (float)(OP + 26), w, 7.0f), 3.5f, i == _step ? gp(p.accent) : gp(p.border, 120), Color(0, 0, 0, 0));
    }
    Pen sepPen(gp(p.sep, 110), 1.0f);
    g.DrawLine(&sepPen, (float)(CONTENT_X + 2), (float)(OP + 64), (float)(OW - OP - 24), (float)(OP + 64));

    auto title = [&](const wchar_t* t, int y) { SelectObject(mem, fTitle2); SetTextColor(mem, p.text); RECT r = {CONTENT_X + 2, y, OW - OP - 20, y + 26}; DrawTextW(mem, t, -1, &r, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX); return textWidth(mem, fTitle2, t); };
    auto para = [&](const std::wstring& t, int y, int hgt) { SelectObject(mem, fBody); SetTextColor(mem, p.secondary); RECT r = {CONTENT_X + 2, y, OW - OP - 20, y + hgt}; DrawTextW(mem, t.c_str(), -1, &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX); };
    auto label = [&](const wchar_t* t, int x, int y, int hgt, COLORREF c, HFONT f = nullptr) { SelectObject(mem, f ? f : fBody); SetTextColor(mem, c); RECT r = {x, y, OW - OP - 20, y + hgt}; DrawTextW(mem, t, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX); };

    switch (_step) {
    case S_WELCOME: {
        title(L"Vikey dùng kiểu gõ Telex", _oy[0]);
        para(L"Vikey chỉ hỗ trợ Telex, kiểu gõ phổ biến nhất. Bạn gõ tiếng Anh bình thường, Vikey tự nhận ra và không bỏ dấu nhầm.", _oy[1], 56);
        static const wchar_t* T[4][4][2] = {{{L"aa", L"â"}, {L"aw", L"ă"}, {L"ee", L"ê"}, {L"oo", L"ô"}}, {{L"ow", L"ơ"}, {L"uw", L"ư"}, {L"dd", L"đ"}, {L"w", L"ư"}},
                                            {{L"s", L"sắc"}, {L"f", L"huyền"}, {L"r", L"hỏi"}, {L"x", L"ngã"}}, {{L"j", L"nặng"}, {L"z", L"xoá dấu"}, {nullptr, nullptr}, {nullptr, nullptr}}};
        int top = _oy[2], rowH = 34, boxH = 12 * 2 + 4 * rowH - 6;
        roundBox(g, RectF((float)(CONTENT_X + 2), (float)top, (float)OCW, (float)boxH), 12.0f, gp(p.field, 145), gp(p.glassRim, 85));
        int colW = (OCW - 24) / 4;
        for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
            if (!T[r][c][0]) continue;
            int x = CONTENT_X + 14 + c * colW, y = top + 12 + r * rowH;
            int kw = textWidth(mem, fMonoBold, T[r][c][0]) + 14;
            roundBox(g, RectF((float)x, (float)y, (float)kw, 24.0f), 6.0f, gp(p.panelSolid, 220), gp(p.accent, 140));
            SelectObject(mem, fMonoBold); SetTextColor(mem, p.accent); RECT kr = {x, y, x + kw, y + 24}; DrawTextW(mem, T[r][c][0], -1, &kr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            label(L"→", x + kw + 5, y, 24, p.tertiary, fSmall);
            label(T[r][c][1], x + kw + 19, y, 24, p.text);
        }
        break; }
    case S_PRACTICE: {
        title(L"Gõ thử song ngữ", _oy[0]);
        para(practiceText(), _oy[1], 40);
        for (int i = 0; i < 3; i++) {
            int y = _oy[2 + i], x = CONTENT_X + 2;
            label(L"Gõ", x, y, 20, p.secondary, fCallout); x += textWidth(mem, fCallout, L"Gõ") + 6;
            label(PRACTICE[i].keys, x, y, 20, p.accent, fMonoBold); x += textWidth(mem, fMonoBold, PRACTICE[i].keys) + 6;
            label(L"→", x, y, 20, p.tertiary, fSmall); x += textWidth(mem, fSmall, L"→") + 6;
            label(PRACTICE[i].result, x, y, 20, p.text, fCallout);
            drawCheck(g, (float)(CONTENT_X + OCW - 24), (float)(y + 25), _practiceOk[i]);
        }
        break; }
    case S_HOTKEY: {
        title(L"Phím chuyển Anh/Việt", _oy[0]);
        para(L"Vikey tự chuyển khi bạn gõ lẫn tiếng Anh. Khi muốn tự chuyển, bấm phím tắt này. Bấm vào ô để đổi phím.", _oy[1], 56);
        label(core::language() == 1 ? L"Đang gõ: Tiếng Việt" : L"Đang gõ: English", CONTENT_X + 150 + 16, _oy[2], 32, p.emerald, fBodyBold);
        label(L"Âm báo khi chuyển", CONTENT_X + 2, _oy[3], 24, p.text);
        break; }
    case S_EMOJI: {
        int tw = title(L"Bảng emoji", _oy[0]);
        label(L"không bắt buộc", CONTENT_X + tw + 12, _oy[0] + 3, 24, p.secondary, fCallout);
        RECT pr = {CONTENT_X + 2, _oy[1], OW - OP - 20, _oy[2] - 12};
        colorText(mem, L"Bấm một phím tắt để mở bảng emoji ở giữa màn hình. Tìm bằng tiếng Việt hay tiếng Anh đều được: \"ngựa\" hay \"horse\" đều ra 🐴.", pr, 14, p.secondary);
        label(L"Bật bảng emoji", CONTENT_X + 2, _oy[2], 24, p.text);
        if (prefs::getInt(L"EmojiPicker", 0)) label(L"Phím mở bảng emoji", CONTENT_X + 2, _oy[3], 32, p.text);
        if (_oy[4] >= 0) label(L"Bấm một phím kèm Ctrl, Alt hoặc Win, ví dụ Ctrl+Alt+E.", CONTENT_X + 2, _oy[4], 20, p.secondary, fCallout);
        break; }
    }

    delete blurred;
    saveWindowCanvas(h, mem, OW, OH);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
    ReleaseDC(nullptr, screen);
}

static void buildOnboarding() {
    destroyChildren(_onbWin);
    dropWindowCanvas(_onbWin);
    for (auto& e : _practiceEdit) e = nullptr;
    int y = 94;
    bool emojiOn = prefs::getInt(L"EmojiPicker", 0) != 0; int emojiKey = prefs::getInt(L"EmojiHotkey", 0);
    switch (_step) {
    case S_WELCOME: {
        _oy[0] = y; y += 28 + 10;
        _oy[1] = y; y += textHeight(fBody, L"Vikey chỉ hỗ trợ Telex, kiểu gõ phổ biến nhất. Bạn gõ tiếng Anh bình thường, Vikey tự nhận ra và không bỏ dấu nhầm.", OCW) + 14;
        _oy[2] = y;
        break; }
    case S_PRACTICE: {
        _oy[0] = y; y += 28 + 10;
        _oy[1] = y; y += 2 * 20 + 10;
        for (int i = 0; i < 3; i++) {
            _oy[2 + i] = y;
            HWND e = makeChild(_onbWin, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, CONTENT_X + 2, y + 22, OCW - 36, 28, O_EDIT + i, fBody);
            SetWindowSubclass(e, practiceEditProc, i, 0);
            SendMessageW(e, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
            _practiceEdit[i] = e; _practiceOk[i] = false;
            y += 22 + 28 + 12;
        }
        SetFocus(_practiceEdit[0]);
        break; }
    case S_HOTKEY: {
        _oy[0] = y; y += 28 + 12;
        std::wstring t = L"Vikey tự chuyển khi bạn gõ lẫn tiếng Anh. Khi muốn tự chuyển, bấm phím tắt này. Bấm vào ô để đổi phím.";
        _oy[1] = y; y += textHeight(fBody, t, OCW) + 14;
        makeRecorder(_onbWin, L"SwitchKeyStatus", true, currentHotkey(), CONTENT_X + 2, y, 148, 32, O_REC);
        _oy[2] = y; y += 32 + 16;
        _oy[3] = y;
        HDC dc = GetDC(_onbWin); int lw = textWidth(dc, fBody, L"Âm báo khi chuyển"); ReleaseDC(_onbWin, dc);
        HWND tg = makeChild(_onbWin, L"VikeyToggle", L"", WS_TABSTOP, CONTENT_X + 2 + lw + 14, y, 44, 24, O_BEEP);
        setToggle(tg, HAS_BEEP(currentHotkey()) != 0);
        break; }
    case S_EMOJI: {
        _oy[0] = y; y += 28 + 12;
        std::wstring t = L"Bấm một phím tắt để mở bảng emoji ở giữa màn hình. Tìm bằng tiếng Việt hay tiếng Anh đều được: \"ngựa\" hay \"horse\" đều ra 🐴.";
        _oy[1] = y; y += textHeight(fBody, t, OCW) + 14;
        _oy[2] = y;
        HDC dc = GetDC(_onbWin); int lw = textWidth(dc, fBody, L"Bật bảng emoji"), lw2 = textWidth(dc, fBody, L"Phím mở bảng emoji"); ReleaseDC(_onbWin, dc);
        HWND tg = makeChild(_onbWin, L"VikeyToggle", L"", WS_TABSTOP, CONTENT_X + 2 + lw + 14, y, 44, 24, O_EMOJI);
        setToggle(tg, emojiOn);
        y += 24 + 16;
        _oy[3] = y; _oy[4] = -1;
        if (emojiOn) {
            HWND r = makeRecorder(_onbWin, L"EmojiHotkey", false, emojiKey, CONTENT_X + 2 + lw2 + 14, y, 148, 32, O_EMOJIKEY);
            y += 32 + 10;
            if (emojiKey == 0) { _oy[4] = y; if (GetForegroundWindow() == _onbWin) startRecording(r); }
        }
        break; }
    }
    if (_step != S_WELCOME) onbButton(L"Quay lại", false, CONTENT_X + 2, 102, O_BACK);
    const wchar_t* next = _step == S_WELCOME ? L"Bắt đầu" : _step == S_EMOJI ? L"Xong" : L"Tiếp tục";
    HWND nb = onbButton(next, true, OW - OP - 128, 112, O_NEXT);
    if (_step == S_EMOJI && emojiOn && emojiKey == 0) EnableWindow(nb, FALSE);
    composeOnboardingCanvas(_onbWin);
    RedrawWindow(_onbWin, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static void paintOnboarding(HWND h) {
    PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
    if (!_winCanvas.count(h) || !_winCanvas[h]) composeOnboardingCanvas(h);
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, _winCanvas[h]);
    BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old); DeleteDC(mem);
    EndPaint(h, &ps);
}

static LRESULT CALLBACK onbProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: paintOnboarding(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_COMMAND: {
        int id = LOWORD(wp); HWND c = (HWND)lp;
        if (id >= O_EDIT && id < O_EDIT + 3 && HIWORD(wp) == EN_CHANGE) {
            int i = id - O_EDIT; wchar_t buf[256]; GetWindowTextW(c, buf, 256);
            bool ok = normalize(buf) == normalize(PRACTICE[i].result);
            if (ok != _practiceOk[i]) {
                _practiceOk[i] = ok;
                composeOnboardingCanvas(h);
                InvalidateRect(h, nullptr, FALSE);
                if (ok && i + 1 < 3) { core::resetTyping(); SetFocus(_practiceEdit[i + 1]); }
            }
            return 0;
        }
        switch (id) {
        case O_BACK: if (_step > 0) onbGo(_step - 1); break;
        case O_NEXT: if (_step == S_EMOJI) { prefs::setInt(L"OnboardingDone", 1); DestroyWindow(h); } else onbGo(_step + 1); break;
        case O_REC: composeOnboardingCanvas(h); InvalidateRect(h, nullptr, FALSE); if (_win) PostMessageW(_win, WM_REBUILD, 0, 0); break;
        case O_BEEP: { int hk = (currentHotkey() & ~0x8000) | (toggleOn(c) ? 0x8000 : 0); prefs::setInt(L"SwitchKeyStatus", hk); core::reloadSettings(); if (_win) PostMessageW(_win, WM_REBUILD, 0, 0); break; }
        case O_EMOJI: prefs::setInt(L"EmojiPicker", toggleOn(c)); core::reloadSettings(); emoji::settingsChanged(); PostMessageW(h, WM_REBUILD, 0, 0); if (_win) PostMessageW(_win, WM_REBUILD, 0, 0); break;
        case O_EMOJIKEY: PostMessageW(h, WM_REBUILD, 0, 0); break;
        }
        return 0; }
    case WM_REBUILD: buildOnboarding(); return 0;
    case WM_CTLCOLOREDIT: {
        const Palette& p = pal();
        SetBkColor((HDC)wp, p.field); SetTextColor((HDC)wp, p.text);
        return (LRESULT)(isDarkTheme() ? _editBrushDark : _editBrushLight); }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: dropWindowCanvas(h); _onbWin = nullptr; return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void showOnboarding(HINSTANCE inst) {
    _inst = inst; initLook(); registerClasses(inst);
    if (_onbWin) { bringToFront(_onbWin); return; }
    static bool reg = false;
    if (!reg) { WNDCLASSW wc = {}; wc.lpfnWndProc = onbProc; wc.hInstance = inst; wc.lpszClassName = L"VikeyOnboarding"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP)); RegisterClassW(&wc); reg = true; }
    RECT wr = {0, 0, OW, OH}; AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
    _onbWin = CreateWindowExW(0, L"VikeyOnboarding", L"Hướng dẫn Vikey", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, 0, 0, wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, inst, nullptr);
    applyWindowChrome(_onbWin);
    int s = prefs::getInt(L"OnboardingStep", -1); _step = s >= 0 && s < S_COUNT ? s : S_WELCOME;
    buildOnboarding();
    centerOnMouseScreen(_onbWin);
    bringToFront(_onbWin);
    prefs::setInt(L"FirstRunDone", 1);
}

static void afterHotkeyChange() {
    if (_win) PostMessageW(_win, WM_REBUILD, 0, 0);
    if (_onbWin && _step != S_PRACTICE) PostMessageW(_onbWin, WM_REBUILD, 0, 0); else if (_onbWin) { composeOnboardingCanvas(_onbWin); InvalidateRect(_onbWin, nullptr, FALSE); }
}

struct Credit { const wchar_t* name; const wchar_t* by; const wchar_t* license; const wchar_t* url; };
static const Credit ACK_CODE[] = {
    {L"OpenKey", L"Tuyen Mai", L"GPL-3.0", L"https://github.com/tuyenvm/OpenKey"},
    {L"TweetNaCl", L"Daniel J. Bernstein et al.", L"public domain", L"https://tweetnacl.cr.yp.to"},
    {L"Inno Setup", L"Jordan Russell, Martijn Laan", L"Inno Setup License", L"https://jrsoftware.org/isinfo.php"},
};
static const Credit ACK_WORDS[] = {
    {L"FrequencyWords (OpenSubtitles 2018)", L"Hermit Dave", L"CC BY-SA 4.0", L"https://github.com/hermitdave/FrequencyWords"},
    {L"FineWeb-2 and FineWeb", L"Hugging Face", L"ODC-By 1.0", L"https://huggingface.co/datasets/HuggingFaceFW/fineweb-2"},
    {L"HPLT v2", L"High Performance Language Technologies", L"CC0", L"https://hplt-project.org"},
    {L"MADLAD-400", L"Google", L"ODC-By 1.0", L"https://huggingface.co/datasets/allenai/MADLAD-400"},
    {L"Reddit comments", L"tensorshield/reddit_dataset_157", L"MIT", L"https://huggingface.co/datasets/tensorshield/reddit_dataset_157"},
    {L"Vietnamese forums and reader comments", L"VnExpress, Tuổi Trẻ, Tinhte, Otofun, Dạy Nhau Học", L"word statistics", L"https://vikey.app/acknowledgements"},
    {L"Emoji names and keywords (CLDR)", L"Unicode, Inc.", L"Unicode License v3", L"https://cldr.unicode.org"},
    {L"Emojibase", L"Miles Johnson", L"MIT", L"https://emojibase.dev"},
};
static const Credit ACK_TEST[] = {
    {L"ViLexNorm", L"Nguyen et al., EACL 2024", L"CC BY-NC-SA 4.0", L"https://github.com/ngxtnhi/ViLexNorm"},
    {L"UIT-ViCTSD", L"UIT NLP Group", L"research use", L"https://huggingface.co/datasets/tarudesu/ViCTSD"},
    {L"VietMix", L"EACL 2026", L"research use", L"https://huggingface.co/datasets/razent/vietmix"},
    {L"Tap typing with touch sensing images", L"Google Research", L"CC BY 4.0", L"https://github.com/google-research-datasets/tap-typing-with-touch-sensing-images"},
    {L"How We Type: Mobile", L"Aalto University", L"research use", L"https://userinterfaces.aalto.fi/how-we-type-mobile"},
    {L"GitHub Typo Corpus", L"Hagiwara & Mita, LREC 2020", L"research use", L"https://github.com/mhagiwara/github-typo-corpus"},
    {L"Birkbeck spelling error corpus", L"Roger Mitton", L"free to use", L"https://www.dcs.bbk.ac.uk/~roger/corpora.html"},
    {L"Lists of common misspellings", L"Wikipedia", L"CC BY-SA 4.0", L"https://en.wikipedia.org/wiki/Wikipedia:Lists_of_common_misspellings"},
};
static const int AW = 540, AH = 600, AP = 20;
static int _ackScroll = 0, _ackContent = 0; static std::vector<Link> _ackLinks;

static int paintAck(HDC dc, int top, bool draw, Bitmap* blurred) {
    int y = top + AP; _ackLinks.clear();
    const Palette& p = pal();
    if (draw) {
        Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
        RectF banner((float)AP, (float)y, (float)(AW - 2 * AP), 72.0f);
        drawGlassCard(g, blurred, banner, 16.0f);
        if (_arts[2]) {
            RectF artR((float)(AP + 10), (float)(y + 10), 52.0f, 52.0f);
            GraphicsPath ap; makeRoundPath(ap, artR, 12.0f);
            GraphicsState st = g.Save(); g.SetClip(&ap);
            drawArtCover(g, _arts[2], artR, 0.5f, 0.35f);
            g.Restore(st);
        }
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, fTitle); SetTextColor(dc, p.text);
        RECT tr = {AP + 74, y + 14, AW - AP - 14, y + 36};
        DrawTextW(dc, L"Ghi nhận mã nguồn & dữ liệu", -1, &tr, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, fSmall); SetTextColor(dc, p.secondary);
        RECT sr = {AP + 74, y + 38, AW - AP - 14, y + 58};
        DrawTextW(dc, L"Các dự án mã nguồn mở và bộ dữ liệu ngôn ngữ tạo nên Vikey.", -1, &sr, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    }
    y += 86;
    struct Sec { const wchar_t* title; const wchar_t* note; const Credit* items; int n; } secs[] = {
        {L"Mã nguồn", L"Vikey dùng mã nguồn của:", ACK_CODE, (int)(sizeof(ACK_CODE) / sizeof(*ACK_CODE))},
        {L"Dữ liệu từ ngữ", L"Vikey tự chuyển Anh/Việt nhờ thống kê từ ngữ đếm từ các nguồn này. Trong ứng dụng chỉ có số liệu thống kê, không có văn bản nào.", ACK_WORDS, (int)(sizeof(ACK_WORDS) / sizeof(*ACK_WORDS))},
        {L"Dữ liệu kiểm thử", L"Chỉ dùng để đo và cải thiện độ chính xác, không có trong ứng dụng.", ACK_TEST, (int)(sizeof(ACK_TEST) / sizeof(*ACK_TEST))}
    };
    SetBkMode(dc, TRANSPARENT);
    for (int si = 0; si < 3; si++) {
        auto& s = secs[si];
        if (draw) { SelectObject(dc, fBodyBold); SetTextColor(dc, p.accent); RECT r = {AP + 4, y, AW - AP, y + 22}; DrawTextW(dc, s.title, -1, &r, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX); }
        y += 22 + 4;
        int nh = textHeight(fCallout, s.note, AW - 2 * AP - 8);
        if (draw) { SelectObject(dc, fCallout); SetTextColor(dc, p.secondary); RECT r = {AP + 4, y, AW - AP - 4, y + nh}; DrawTextW(dc, s.note, -1, &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX); }
        y += nh + 10;
        int cardH = s.n * 44 + 8;
        if (draw) {
            Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
            drawGlassCard(g, blurred, RectF((float)AP, (float)y, (float)(AW - 2 * AP), (float)cardH), 14.0f);
        }
        int ry = y + 4;
        for (int i = 0; i < s.n; i++) {
            auto& c = s.items[i];
            int lw = textWidth(dc, fSmall, c.license);
            if (draw) {
                std::vector<Seg> name = {{c.name, c.url, fBodyBold, p.accent}};
                drawSegs(dc, name, AP + 14, ry + 4, AW - 2 * AP - lw - 28, false, _ackLinks);
                SelectObject(dc, fSmall); SetTextColor(dc, p.tertiary);
                RECT lr = {AW - AP - lw - 14, ry + 5, AW - AP - 14, ry + 23}; DrawTextW(dc, c.license, -1, &lr, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(dc, fSmall); SetTextColor(dc, p.secondary);
                RECT br = {AP + 14, ry + 23, AW - AP - lw - 28, ry + 40}; DrawTextW(dc, c.by, -1, &br, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            }
            ry += 44;
        }
        y += cardH + 16;
    }
    return y - top + AP - 10;
}

static LRESULT CALLBACK ackProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        Bitmap* blurred = createBlurredWallpaper(_arts[currentArtIndex()], rc.right, rc.bottom, 0.5f, 0.3f);
        Graphics g(mem);
        if (blurred) g.DrawImage(blurred, 0, 0, (int)rc.right, (int)rc.bottom);
        SolidBrush veil(gp(pal().bg, isDarkTheme() ? 130 : 145));
        g.FillRectangle(&veil, 0, 0, (int)rc.right, (int)rc.bottom);
        paintAck(mem, -_ackScroll, true, blurred);
        delete blurred;
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEWHEEL: case WM_VSCROLL: {
        RECT rc; GetClientRect(h, &rc); int maxS = std::max(0, _ackContent - (int)rc.bottom), s = _ackScroll;
        if (msg == WM_MOUSEWHEEL) s -= GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * 60;
        else switch (LOWORD(wp)) { case SB_LINEUP: s -= 30; break; case SB_LINEDOWN: s += 30; break; case SB_PAGEUP: s -= rc.bottom; break; case SB_PAGEDOWN: s += rc.bottom; break; case SB_THUMBTRACK: case SB_THUMBPOSITION: s = HIWORD(wp); break; case SB_TOP: s = 0; break; case SB_BOTTOM: s = maxS; break; }
        _ackScroll = std::max(0, std::min(s, maxS)); SetScrollPos(h, SB_VERT, _ackScroll, TRUE); InvalidateRect(h, nullptr, FALSE); return 0; }
    case WM_SETCURSOR: { POINT p; GetCursorPos(&p); ScreenToClient(h, &p); if (LOWORD(lp) == HTCLIENT) { SetCursor(LoadCursor(nullptr, linkAt(_ackLinks, MAKELPARAM(p.x, p.y)) ? IDC_HAND : IDC_ARROW)); return TRUE; } break; }
    case WM_LBUTTONUP: if (const Link* l = linkAt(_ackLinks, lp)) ShellExecuteW(nullptr, L"open", l->url.c_str(), nullptr, nullptr, SW_SHOW); return 0;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: _ackWin = nullptr; return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void showAcknowledgements(HINSTANCE inst) {
    initLook();
    if (_ackWin) { bringToFront(_ackWin); return; }
    static bool reg = false;
    if (!reg) { WNDCLASSW wc = {}; wc.lpfnWndProc = ackProc; wc.hInstance = inst; wc.lpszClassName = L"VikeyAck"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP)); RegisterClassW(&wc); reg = true; }
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VSCROLL;
    RECT wr = {0, 0, AW, AH}; AdjustWindowRectEx(&wr, style, FALSE, 0); wr.right += GetSystemMetrics(SM_CXVSCROLL);
    _ackWin = CreateWindowExW(0, L"VikeyAck", L"Ghi nhận", style, 0, 0, wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, inst, nullptr);
    applyWindowChrome(_ackWin);
    HDC dc = GetDC(_ackWin); _ackContent = paintAck(dc, 0, false, nullptr); ReleaseDC(_ackWin, dc); _ackScroll = 0;
    SCROLLINFO si = {sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS, 0, _ackContent - 1, (UINT)AH, 0}; SetScrollInfo(_ackWin, SB_VERT, &si, TRUE);
    centerOnMouseScreen(_ackWin);
    bringToFront(_ackWin);
}

static std::vector<Link> _aboutLinks;
static LRESULT CALLBACK aboutProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        const Palette& p = pal();
        Bitmap* flowerArt = _arts[2] ? _arts[2] : _arts[0];
        Bitmap* blurred = createBlurredWallpaper(flowerArt, rc.right, rc.bottom, 0.5f, 0.35f);
        Graphics g(mem); g.SetSmoothingMode(SmoothingModeAntiAlias);
        if (blurred) g.DrawImage(blurred, 0, 0, (int)rc.right, (int)rc.bottom);
        SolidBrush veil(gp(p.bg, isDarkTheme() ? 110 : 125));
        g.FillRectangle(&veil, 0, 0, (int)rc.right, (int)rc.bottom);

        drawGlassCard(g, blurred, RectF(18.0f, 18.0f, (float)(rc.right - 36), (float)(rc.bottom - 36)), 18.0f);
        delete blurred;

        // Rounded squircle portrait of Reze holding white narcissus flowers
        float artS = 128.0f, artX = (rc.right - artS) / 2.0f, artY = 34.0f;
        RectF artRect(artX, artY, artS, artS);
        GraphicsPath artPath; makeRoundPath(artPath, artRect, 22.0f);
        GraphicsState st = g.Save(); g.SetClip(&artPath);
        if (flowerArt) drawArtCover(g, flowerArt, artRect, 0.5f, 0.35f);
        g.Restore(st);
        Pen artRim(gp(p.accent, 185), 1.6f);
        g.DrawPath(&artRim, &artPath);

        SetBkMode(mem, TRANSPARENT);
        int wmW = textWidth(mem, fWordmark, L"Vikey");
        drawWordmark(mem, fWordmark, (rc.right - wmW) / 2, 174);
        SelectObject(mem, fSmall); SetTextColor(mem, p.secondary);
        RECT r2 = {0, 206, rc.right, 226}; DrawTextW(mem, L"Phiên bản " VIKEY_VERSION_W, -1, &r2, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        _aboutLinks.clear(); int y = 240;
        std::vector<std::vector<Seg>> lines = {
            {{L"Tony Dinh", L"", fBodyBold, p.text}},
            {{L"Dựa trên ", L"", fSmall, p.secondary}, {L"OpenKey", L"https://github.com/tuyenvm/OpenKey", fSmallBold, p.accent}, {L" của Tuyen Mai", L"", fSmall, p.secondary}},
            {{L"Mã nguồn mở theo giấy phép GPL-3.0", L"", fSmall, p.secondary}},
            {},
            {{L"vikey.app", L"https://vikey.app", fBodyBold, p.accent}, {L"   ·   ", L"", fSmall, p.tertiary}, {L"GitHub", L"https://github.com/tonyminibot/Vikey", fBodyBold, p.accent}, {L"   ·   ", L"", fSmall, p.tertiary}, {L"Ghi nhận", L"ack:", fBodyBold, p.accent}}
        };
        for (auto& l : lines) { if (l.empty()) { y += 12; continue; } y += drawSegs(mem, l, 0, y, rc.right, true, _aboutLinks) + 5; }
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_SETCURSOR: { POINT p; GetCursorPos(&p); ScreenToClient(h, &p); if (LOWORD(lp) == HTCLIENT) { SetCursor(LoadCursor(nullptr, linkAt(_aboutLinks, MAKELPARAM(p.x, p.y)) ? IDC_HAND : IDC_ARROW)); return TRUE; } break; }
    case WM_LBUTTONUP: if (const Link* l = linkAt(_aboutLinks, lp)) { if (l->url == L"ack:") showAcknowledgements(_inst); else ShellExecuteW(nullptr, L"open", l->url.c_str(), nullptr, nullptr, SW_SHOW); } return 0;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: dropWindowCanvas(h); _aboutWin = nullptr; return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void showAbout(HINSTANCE inst) {
    _inst = inst; initLook();
    if (_aboutWin) { bringToFront(_aboutWin); return; }
    static bool reg = false;
    if (!reg) { WNDCLASSW wc = {}; wc.lpfnWndProc = aboutProc; wc.hInstance = inst; wc.lpszClassName = L"VikeyAbout"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP)); RegisterClassW(&wc); reg = true; }
    RECT wr = {0, 0, 356, 392}; AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
    _aboutWin = CreateWindowExW(0, L"VikeyAbout", L"Giới thiệu Vikey", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 0, 0, wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, inst, nullptr);
    applyWindowChrome(_aboutWin);
    centerOnMouseScreen(_aboutWin);
    bringToFront(_aboutWin);
}

static std::wstring fromUtf8(const std::string& s) { int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0); std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n); return w; }
static std::wstring jget(const std::wstring& j, const wchar_t* key) {
    std::wstring k = L"\"" + std::wstring(key) + L"\":"; size_t p = j.find(k); if (p == std::wstring::npos) return L"";
    p += k.size(); if (p < j.size() && j[p] == L'"') { size_t e = p + 1; std::wstring out; while (e < j.size() && j[e] != L'"') { if (j[e] == L'\\' && e + 1 < j.size()) { e++; if (j[e] == L'u' && e + 4 < j.size()) { out += (wchar_t)wcstol(j.substr(e + 1, 4).c_str(), nullptr, 16); e += 4; } else out += j[e]; } else out += j[e]; e++; } return out; }
    size_t e = j.find_first_of(L",}", p); return j.substr(p, e - p);
}
struct Period { std::map<std::wstring, int> c; int get(const wchar_t* k) const { auto i = c.find(k); return i == c.end() ? 0 : i->second; } };
static Period sumStats(int days) {
    Period p; HKEY h; if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Vikey", 0, KEY_READ, &h) != ERROR_SUCCESS) return p;
    SYSTEMTIME st; GetLocalTime(&st); FILETIME ft; SystemTimeToFileTime(&st, &ft); ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    std::wstring from = L"0000";
    if (days > 0) { u.QuadPart -= (ULONGLONG)(days - 1) * 864000000000ULL; ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart; SYSTEMTIME f; FileTimeToSystemTime(&ft, &f); wchar_t b[16]; swprintf_s(b, L"%04d-%02d-%02d", f.wYear, f.wMonth, f.wDay); from = b; }
    for (DWORD i = 0;; i++) { wchar_t name[256]; DWORD nl = 256, type, v, vs = sizeof(v); LONG rc = RegEnumValueW(h, i, name, &nl, nullptr, &type, (LPBYTE)&v, &vs);
        if (rc == ERROR_MORE_DATA) continue;
        if (rc != ERROR_SUCCESS) break;
        std::wstring n = name; if (n.rfind(L"TLStat_", 0) != 0 || type != REG_DWORD) continue; std::wstring day = n.substr(7, 10); if (day < from) continue; p.c[n.substr(18)] += (int)v; }
    RegCloseKey(h); return p;
}

static void applyLogTheme() {
    if (!_logWin) return;
    const Palette& p = pal();
    for (HWND lv : {_logGrid, _logList}) {
        if (!lv) continue;
        ListView_SetBkColor(lv, p.panelSolid);
        ListView_SetTextBkColor(lv, p.panelSolid);
        ListView_SetTextColor(lv, p.text);
        InvalidateRect(lv, nullptr, TRUE);
    }
}

static std::wstring _logSummaryText, _logEmptyText;

static void refreshLog() {
    _logSummaryText = L"Đang tải dữ liệu nhật ký gõ…";
    core::flushTypingLog();
    Period p1 = sumStats(1), p7 = sumStats(7), pa = sumStats(0);
    auto autoN = [](const Period& p) { return p.get(L"auto_vi_word_to_en") + p.get(L"auto_en_word_to_vi") + p.get(L"auto_double_key"); };
    bool enabled = prefs::getInt(L"TypingLogEnabled", 0) != 0;
    wchar_t s[240];
    swprintf_s(s, L"Hôm nay: %d lần chuyển tay, %d lần tự chuyển (%s)",
               p1.get(L"manual"), autoN(p1), enabled ? L"Đang bật ghi nhật ký" : L"Đang tắt ghi nhật ký");
    _logSummaryText = s;

    const wchar_t* rows[][2] = {{L"Từ đã gõ", L"words"}, {L"Chuyển tay", L"manual"}, {L"Tự chuyển", L"auto"}, {L"    Tiếng Việt -> Anh (cả từ)", L"auto_vi_word_to_en"}, {L"    Anh -> Tiếng Việt (cả từ)", L"auto_en_word_to_vi"},
                               {L"    Gõ dấu hai lần -> Anh", L"auto_double_key"}, {L"Hoàn tác bằng Delete", L"undo"}, {L"Đổi theo ứng dụng / trang web", L"context"}, {L"Chuyển tay mỗi 100 từ", L"rate"}};
    ListView_DeleteAllItems(_logGrid);
    const Period* ps[3] = {&p1, &p7, &pa};
    for (int r = 0; r < 9; r++) {
        LVITEMW it = {}; it.mask = LVIF_TEXT; it.iItem = r; it.pszText = (LPWSTR)rows[r][0]; ListView_InsertItem(_logGrid, &it);
        for (int c = 0; c < 3; c++) {
            const Period& p = *ps[c]; int words = p.get(L"words_vi") + p.get(L"words_en"); std::wstring v; wchar_t b[32];
            std::wstring key = rows[r][1];
            if (key == L"words") v = std::to_wstring(words); else if (key == L"auto") v = std::to_wstring(autoN(p));
            else if (key == L"rate") { if (words) { swprintf_s(b, L"%.1f", 100.0 * p.get(L"manual") / words); v = b; } else v = L"0.0"; }
            else v = std::to_wstring(p.get(key.c_str()));
            ListView_SetItemText(_logGrid, r, c + 1, (LPWSTR)v.c_str());
        }
    }
    std::wstring logPath = core::typingLogPath();
    bool fileExists = GetFileAttributesW(logPath.c_str()) != INVALID_FILE_ATTRIBUTES;
    std::ifstream f(logPath.c_str(), std::ios::binary);
    std::vector<std::string> lines; std::string l;
    if (fileExists && !f.is_open()) {
        _logEmptyText = L"Không mở được file nhật ký gõ. Hãy bấm 'Mở file log' để kiểm tra quyền truy cập thư mục %LOCALAPPDATA%\\Vikey.";
    } else {
        while (std::getline(f, l)) { if (!l.empty()) lines.push_back(l); if (lines.size() > 2000) lines.erase(lines.begin(), lines.begin() + 1000); }
        if (lines.empty()) {
            _logEmptyText = enabled
                ? L"Chưa có sự kiện nào được ghi. Hãy gõ thử vài từ tiếng Việt xen tiếng Anh rồi bấm 'Làm mới'."
                : L"Ghi nhật ký gõ đang tắt. Hãy bật công tắc 'Ghi nhật ký gõ' bên trên để bắt đầu thu thập thống kê.";
        } else {
            wchar_t cntMsg[128]; swprintf_s(cntMsg, L"Đang hiển thị %d sự kiện gần nhất từ typing-log.jsonl.", (int)std::min<size_t>(lines.size(), 300));
            _logEmptyText = cntMsg;
        }
    }
    ListView_DeleteAllItems(_logList);
    int n = 0;
    for (auto li = lines.rbegin(); li != lines.rend() && n < 300; ++li, ++n) {
        std::wstring j = fromUtf8(*li); std::wstring e = jget(j, L"e"), to = jget(j, L"to") == L"vi" ? L"Tiếng Việt" : L"Anh", t = jget(j, L"t");
        std::wstring time = t.size() >= 19 ? t.substr(5, 14) : t; for (auto& ch : time) if (ch == L'T') ch = L' ';
        std::wstring app = jget(j, L"app"), site = jget(j, L"site"); std::wstring place = site.empty() ? app : app + L" · " + site;
        std::wstring title, detail;
        if (e == L"word") { title = jget(j, L"lang") == L"vi" ? L"Từ (Việt)" : L"Từ (Anh)"; detail = jget(j, L"keys") + L"   [" + jget(j, L"end") + L"]"; }
        else if (e == L"auto") { title = L"Tự chuyển -> " + to; std::wstring r = jget(j, L"rule"); detail = r == L"vi_word_to_en" ? L"từ tiếng Anh gõ khi đang ở Tiếng Việt" : r == L"en_word_to_vi" ? L"từ tiếng Việt gõ khi đang ở Anh" : L"gõ dấu hai lần"; }
        else if (e == L"manual") { title = L"Chuyển tay -> " + to; detail = (jget(j, L"via") == L"menu" ? L"bằng menu" : L"bằng phím tắt") + std::wstring(jget(j, L"after_auto") == L"true" ? L", ngay sau tự chuyển" : L""); }
        else if (e == L"undo") { title = L"Hoàn tác -> " + to; detail = L"bằng Delete"; }
        else if (e == L"context") { title = L"Theo app/web -> " + to; }
        else { title = e; }
        LVITEMW it = {}; it.mask = LVIF_TEXT | LVIF_PARAM; it.iItem = n; it.pszText = (LPWSTR)time.c_str(); it.lParam = e == L"manual" || e == L"undo" ? 1 : (e == L"word" ? 0 : 2); ListView_InsertItem(_logList, &it);
        ListView_SetItemText(_logList, n, 1, (LPWSTR)place.c_str()); ListView_SetItemText(_logList, n, 2, (LPWSTR)title.c_str()); ListView_SetItemText(_logList, n, 3, (LPWSTR)detail.c_str());
    }
    if (_logWin) RedrawWindow(_logWin, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static LRESULT CALLBACK logProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        const Palette& p = pal();
        Bitmap* blurred = createBlurredWallpaper(_arts[currentArtIndex()], rc.right, rc.bottom, 0.5f, 0.25f);
        Graphics g(mem); g.SetSmoothingMode(SmoothingModeAntiAlias);
        if (blurred) g.DrawImage(blurred, 0, 0, (int)rc.right, (int)rc.bottom);
        SolidBrush veil(gp(p.bg, isDarkTheme() ? 140 : 155));
        g.FillRectangle(&veil, 0, 0, (int)rc.right, (int)rc.bottom);
        drawGlassCard(g, blurred, RectF(14.0f, 10.0f, (float)(rc.right - 28), (float)(rc.bottom - 20)), 16.0f);
        delete blurred;

        SetBkMode(mem, TRANSPARENT);
        auto drawTxt = [&](HFONT f, COLORREF c, int x, int y, int w, int hgt, const std::wstring& s) {
            SelectObject(mem, f); SetTextColor(mem, c);
            RECT r = {x, y, x + w, y + hgt};
            DrawTextW(mem, s.c_str(), -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        };
        drawTxt(fTitle, p.text, 28, 20, 704, 24, _logSummaryText);
        drawTxt(fBodyBold, p.text, 80, 306, 345, 20, L"Ghi nhật ký gõ (đếm tự chuyển & chuyển tay)");
        drawTxt(fBody, p.text, 492, 306, 236, 20, L"Ghi lại phím gõ của từng từ");
        drawTxt(fSmall, p.secondary, 28, 334, 700, 18, L"Không bao giờ ghi trong ô mật khẩu, trình quản lý mật khẩu và Terminal.");
        drawTxt(fBodyBold, p.accent, 28, 360, 220, 20, L"Sự kiện gần đây");
        drawTxt(fSmall, p.secondary, 28, 382, 704, 22, _logEmptyText);

        saveWindowCanvas(h, mem, rc.right, rc.bottom);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_COMMAND:
        if (LOWORD(wp) == L_REFRESH) refreshLog();
        else if (LOWORD(wp) == L_OPEN) { core::flushTypingLog(); std::wstring p = core::typingLogPath(); if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"/select,\"" + p + L"\"").c_str(), nullptr, SW_SHOW); else ShellExecuteW(nullptr, L"open", prefs::appDataDir().c_str(), nullptr, nullptr, SW_SHOW); }
        else if (LOWORD(wp) == L_CLEAR) { if (MessageBoxW(h, L"Số liệu thống kê và file log sẽ bị xoá.", L"Xoá toàn bộ nhật ký gõ?", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) == IDOK) { core::clearTypingLog(); HKEY k; if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Vikey", 0, KEY_READ | KEY_WRITE, &k) == ERROR_SUCCESS) { std::vector<std::wstring> del; for (DWORD i = 0;; i++) { wchar_t name[256]; DWORD nl = 256; if (RegEnumValueW(k, i, name, &nl, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break; if (wcsncmp(name, L"TLStat_", 7) == 0) del.push_back(name); } for (auto& d : del) RegDeleteValueW(k, d.c_str()); RegCloseKey(k); } refreshLog(); } }
        else if (LOWORD(wp) == C_LOGENABLE) { prefs::setInt(L"TypingLogEnabled", toggleOn((HWND)lp)); core::reloadSettings(); refreshLog(); }
        else if (LOWORD(wp) == C_LOGKEYS) { prefs::setInt(L"TypingLogKeys", toggleOn((HWND)lp)); core::reloadSettings(); }
        return 0;
    case WM_NOTIFY: { NMHDR* n = (NMHDR*)lp;
        if ((n->hwndFrom == _logList || n->hwndFrom == _logGrid) && n->code == NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)lp;
            const Palette& p = pal();
            if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                cd->clrTextBk = p.panelSolid;
                if (n->hwndFrom == _logList) cd->clrText = cd->nmcd.lItemlParam == 1 ? p.amber : (cd->nmcd.lItemlParam == 2 ? p.accent : p.text);
                else cd->clrText = p.text;
                return CDRF_NEWFONT;
            }
        }
        return 0; }
    case WM_SIZE: {
        RECT r; GetClientRect(h, &r);
        MoveWindow(_logList, 28, 412, r.right - 56, std::max(60L, r.bottom - 412 - 58), TRUE);
        for (int id : {L_REFRESH, L_OPEN, L_CLEAR}) {
            HWND b = GetDlgItem(h, id); if (!b) continue;
            RECT br; GetWindowRect(b, &br); MapWindowPoints(nullptr, h, (POINT*)&br, 2);
            MoveWindow(b, id == L_CLEAR ? r.right - 28 - 110 : br.left, r.bottom - 48, id == L_CLEAR ? 110 : br.right - br.left, 30, TRUE);
        }
        return 0; }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: dropWindowCanvas(h); _logWin = nullptr; _logList = nullptr; _logSummary = nullptr; _logGrid = nullptr; _logEmpty = nullptr; return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void showTypingLog(HINSTANCE inst) {
    initLook(); registerClasses(inst);
    if (_logWin) { bringToFront(_logWin); refreshLog(); return; }
    static bool reg = false;
    if (!reg) { WNDCLASSW wc = {}; wc.lpfnWndProc = logProc; wc.hInstance = inst; wc.lpszClassName = L"VikeyLog"; wc.hbrBackground = nullptr; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP)); RegisterClassW(&wc); reg = true; }
    RECT wr = {0, 0, 760, 624}; AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, FALSE, 0);
    _logWin = CreateWindowExW(0, L"VikeyLog", L"Nhật ký gõ", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, inst, nullptr);
    applyWindowChrome(_logWin);
    HWND old = _win; _win = _logWin;
    _logGrid = child(WC_LISTVIEWW, L"", WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL, 28, 52, 704, 240, 0, fBody);
    ListView_SetExtendedListViewStyle(_logGrid, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    const wchar_t* heads[] = {L"Chỉ số", L"Hôm nay", L"7 ngày", L"Tất cả"}; int widths[] = {320, 122, 122, 122};
    for (int i = 0; i < 4; i++) { LVCOLUMNW c = {}; c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; c.pszText = (LPWSTR)heads[i]; c.cx = widths[i]; c.fmt = i ? LVCFMT_RIGHT : LVCFMT_LEFT; ListView_InsertColumn(_logGrid, i, &c); }
    HWND tgEn = child(L"VikeyToggle", L"", WS_TABSTOP, 28, 304, 44, 24, C_LOGENABLE); setToggle(tgEn, prefs::getInt(L"TypingLogEnabled", 0) != 0);
    HWND tg = child(L"VikeyToggle", L"", WS_TABSTOP, 440, 304, 44, 24, C_LOGKEYS); setToggle(tg, prefs::getInt(L"TypingLogKeys", 1) != 0);
    _logList = child(WC_LISTVIEWW, L"", WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL, 28, 412, 704, 152, 0, fBody);
    ListView_SetExtendedListViewStyle(_logList, LVS_EX_FULLROWSELECT);
    const wchar_t* eh[] = {L"Thời gian", L"Ứng dụng / trang", L"Sự kiện", L"Chi tiết"}; int ew[] = {115, 165, 145, 265};
    for (int i = 0; i < 4; i++) { LVCOLUMNW c = {}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = (LPWSTR)eh[i]; c.cx = ew[i]; ListView_InsertColumn(_logList, i, &c); }
    HWND bRef = child(L"VikeyButton", L"Làm mới", WS_TABSTOP, 28, 576, 100, 30, L_REFRESH); SetWindowLongPtrW(bRef, GWLP_USERDATA, 1);
    child(L"VikeyButton", L"Mở file log", WS_TABSTOP, 136, 576, 110, 30, L_OPEN);
    child(L"VikeyButton", L"Xoá log…", WS_TABSTOP, 622, 576, 110, 30, L_CLEAR);
    _win = old;
    applyLogTheme();
    refreshLog(); centerOnMouseScreen(_logWin); bringToFront(_logWin);
}

}}
