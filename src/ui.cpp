#include "ui.h"

#include "click_worker.h"
#include "domain.h"
#include "hotkey.h"
#include "resource.h"
#include "settings.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <oleacc.h>
#include <objbase.h>
#include <uxtheme.h>
#include <winreg.h>

#include <cstdint>

namespace {

constexpr wchar_t kWindowClassName[] = L"MouseClick.MainWindow";
constexpr wchar_t kWindowTitle[] = L"MouseClick";
constexpr DWORD kMainWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                                   WS_MINIMIZEBOX | WS_THICKFRAME;

class ComApartment final {
public:
    ComApartment() : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComApartment() {
        if (SUCCEEDED(result_)) {
            CoUninitialize();
        }
    }

private:
    HRESULT result_ = E_FAIL;
};

struct ThemeColors {
    bool dark = false;
    bool highContrast = false;
    COLORREF windowBackground = RGB(244, 247, 252);
    COLORREF topRightWave = RGB(227, 238, 251);
    COLORREF cardBackground = RGB(255, 255, 255);
    COLORREF cardBorder = RGB(232, 238, 246);
    COLORREF text = RGB(26, 32, 44);
    COLORREF mutedText = RGB(85, 100, 118);
    COLORREF subtleText = RGB(131, 146, 167);
    COLORREF controlBackground = RGB(248, 250, 253);
    COLORREF controlBorder = RGB(220, 228, 238);
    COLORREF editBackground = RGB(255, 255, 255);
    COLORREF hotkeyBoxBackground = RGB(248, 250, 253);
    COLORREF accent = RGB(36, 114, 246);
    COLORREF accentHover = RGB(27, 102, 229);
    COLORREF accentPressed = RGB(20, 87, 201);
    COLORREF accentText = RGB(255, 255, 255);
    COLORREF error = RGB(224, 49, 49);
    COLORREF success = RGB(0, 192, 112);
    COLORREF intervalIconBg = RGB(234, 242, 253);
    COLORREF intervalIconFg = RGB(26, 115, 232);
    COLORREF optionsIconBg = RGB(242, 239, 254);
    COLORREF optionsIconFg = RGB(124, 92, 252);
    COLORREF statusIconBg = RGB(230, 249, 238);
    COLORREF statusIconFg = RGB(0, 192, 112);
    COLORREF hotkeyIconBg = RGB(255, 243, 230);
    COLORREF hotkeyIconFg = RGB(245, 130, 32);
    COLORREF badgeBg = RGB(234, 239, 246);
    COLORREF badgeText = RGB(108, 122, 142);
    COLORREF changeBtnBg = RGB(234, 242, 253);
    COLORREF changeBtnHover = RGB(220, 233, 251);
    COLORREF changeBtnPressed = RGB(207, 223, 247);
    COLORREF changeBtnText = RGB(22, 119, 255);
    COLORREF themeTrackBg = RGB(236, 240, 246);
    COLORREF themeTrackBorder = RGB(222, 228, 238);
    COLORREF themeActiveBg = RGB(255, 255, 255);
    COLORREF themeActiveBorder = RGB(214, 220, 230);
    COLORREF themeActiveText = RGB(20, 25, 35);
    COLORREF themeInactiveText = RGB(100, 114, 132);
    COLORREF themeHoverBg = RGB(244, 247, 251);
    COLORREF themeHoverText = RGB(35, 45, 60);
};

bool IsHighContrast() {
    HIGHCONTRASTW highContrast{};
    highContrast.cbSize = sizeof(highContrast);
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0) != FALSE &&
           (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

bool IsDarkModeEnabled() {
    if (IsHighContrast()) {
        return false;
    }

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD appsUseLightTheme = 1;
    DWORD valueSize = sizeof(appsUseLightTheme);
    DWORD valueType = 0;
    const LONG result = RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &valueType,
                                         reinterpret_cast<LPBYTE>(&appsUseLightTheme), &valueSize);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && valueType == REG_DWORD && appsUseLightTheme == 0;
}

ThemeColors DetectTheme(ThemeMode mode) {
    ThemeColors theme;
    theme.highContrast = IsHighContrast();
    if (theme.highContrast) {
        theme.windowBackground = GetSysColor(COLOR_WINDOW);
        theme.topRightWave = GetSysColor(COLOR_WINDOW);
        theme.cardBackground = GetSysColor(COLOR_WINDOW);
        theme.cardBorder = GetSysColor(COLOR_WINDOWTEXT);
        theme.controlBackground = GetSysColor(COLOR_BTNFACE);
        theme.controlBorder = GetSysColor(COLOR_WINDOWTEXT);
        theme.text = GetSysColor(COLOR_WINDOWTEXT);
        theme.mutedText = GetSysColor(COLOR_WINDOWTEXT);
        theme.subtleText = GetSysColor(COLOR_WINDOWTEXT);
        theme.editBackground = GetSysColor(COLOR_WINDOW);
        theme.hotkeyBoxBackground = GetSysColor(COLOR_WINDOW);
        theme.accent = GetSysColor(COLOR_HIGHLIGHT);
        theme.accentHover = GetSysColor(COLOR_HIGHLIGHT);
        theme.accentPressed = GetSysColor(COLOR_HIGHLIGHT);
        theme.accentText = GetSysColor(COLOR_HIGHLIGHTTEXT);
        theme.error = GetSysColor(COLOR_WINDOWTEXT);
        theme.success = GetSysColor(COLOR_WINDOWTEXT);
        theme.themeTrackBg = GetSysColor(COLOR_WINDOW);
        theme.themeTrackBorder = GetSysColor(COLOR_WINDOWTEXT);
        theme.themeActiveBg = GetSysColor(COLOR_HIGHLIGHT);
        theme.themeActiveBorder = GetSysColor(COLOR_WINDOWTEXT);
        theme.themeActiveText = GetSysColor(COLOR_HIGHLIGHTTEXT);
        theme.themeInactiveText = GetSysColor(COLOR_WINDOWTEXT);
        theme.themeHoverBg = GetSysColor(COLOR_BTNFACE);
        theme.themeHoverText = GetSysColor(COLOR_WINDOWTEXT);
        return theme;
    }
    theme.dark = mode == ThemeMode::Dark ||
                 (mode == ThemeMode::System && IsDarkModeEnabled());
    if (theme.dark) {
        theme.windowBackground = RGB(26, 29, 36);
        theme.topRightWave = RGB(36, 42, 54);
        theme.cardBackground = RGB(34, 38, 48);
        theme.cardBorder = RGB(48, 54, 68);
        theme.text = RGB(240, 243, 248);
        theme.mutedText = RGB(170, 180, 196);
        theme.subtleText = RGB(120, 132, 150);
        theme.controlBackground = RGB(42, 48, 60);
        theme.controlBorder = RGB(60, 68, 84);
        theme.editBackground = RGB(34, 38, 48);
        theme.hotkeyBoxBackground = RGB(42, 48, 60);
        theme.accent = RGB(50, 130, 255);
        theme.accentHover = RGB(70, 145, 255);
        theme.accentPressed = RGB(30, 110, 235);
        theme.error = RGB(255, 120, 120);
        theme.success = RGB(70, 210, 130);
        theme.intervalIconBg = RGB(35, 55, 85);
        theme.intervalIconFg = RGB(90, 160, 255);
        theme.optionsIconBg = RGB(48, 42, 75);
        theme.optionsIconFg = RGB(160, 135, 255);
        theme.statusIconBg = RGB(30, 60, 48);
        theme.statusIconFg = RGB(70, 210, 130);
        theme.hotkeyIconBg = RGB(65, 48, 30);
        theme.hotkeyIconFg = RGB(255, 160, 60);
        theme.badgeBg = RGB(44, 50, 64);
        theme.badgeText = RGB(160, 175, 195);
        theme.changeBtnBg = RGB(35, 55, 85);
        theme.changeBtnHover = RGB(45, 70, 105);
        theme.changeBtnPressed = RGB(30, 48, 75);
        theme.changeBtnText = RGB(90, 160, 255);
        theme.themeTrackBg = RGB(32, 36, 46);
        theme.themeTrackBorder = RGB(48, 54, 68);
        theme.themeActiveBg = RGB(52, 60, 76);
        theme.themeActiveBorder = RGB(68, 78, 98);
        theme.themeActiveText = RGB(255, 255, 255);
        theme.themeInactiveText = RGB(145, 158, 178);
        theme.themeHoverBg = RGB(40, 46, 58);
        theme.themeHoverText = RGB(225, 235, 248);
    }
    return theme;
}

struct AppContext {
    HWND window = nullptr;
    HWND buttonSelect = nullptr;
    HWND clickTypeSelect = nullptr;
    HWND interval = nullptr;
    HWND intervalSpin = nullptr;
    HWND hotkey = nullptr;
    HWND hotkeyCapture = nullptr;
    HWND themeLight = nullptr;
    HWND themeDark = nullptr;
    HWND themeSystem = nullptr;
    HWND startPause = nullptr;
    HWND status = nullptr;
    HWND error = nullptr;
    WNDPROC hotkeyEditProc = nullptr;
    WNDPROC startPauseProc = nullptr;
    WNDPROC captureProc = nullptr;
    WNDPROC themeBtnProc = nullptr;
    Settings settings = DefaultSettings();
    HotkeyManager hotkeys;
    ClickWorker worker;
    AppState state = AppState::Paused;
    bool closing = false;
    bool captureActive = false;
    bool hadHotkeyBeforeCapture = false;
    ThemeColors theme;
    HBRUSH backgroundBrush = nullptr;
    HBRUSH cardBrush = nullptr;
    HBRUSH editBrush = nullptr;
    HBRUSH hotkeyBoxBrush = nullptr;
    HFONT titleFont = nullptr;
    HFONT badgeFont = nullptr;
    HFONT subFont = nullptr;
    HFONT cardTitleFont = nullptr;
    HFONT cardSubFont = nullptr;
    HFONT labelFont = nullptr;
    HFONT bodyFont = nullptr;
    HFONT statusBigFont = nullptr;
    HFONT actionFont = nullptr;
    HFONT hotkeyFont = nullptr;
    HFONT btnChangeFont = nullptr;
    Hotkey previousHotkey{};
    Hotkey pendingHotkey{};
    ProductMessage lastError;

    int hoveredCaptionBtn = 0; // 0=none, 1=min, 2=close
    int pressedCaptionBtn = 0;
    int hoveredThemeBtn = 0;   // 0=none, or IDC_THEME_LIGHT, IDC_THEME_DARK, IDC_THEME_SYSTEM
    bool startHovered = false;
    bool captureHovered = false;
};

LRESULT CALLBACK HotkeyEditProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ButtonHoverProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

int DpiScale(HWND window, int value) {
    const UINT dpi = window == nullptr ? GetDpiForSystem() : GetDpiForWindow(window);
    return MulDiv(value, static_cast<int>(dpi), 96);
}

void SetFont(HWND control, HFONT font) {
    if (control != nullptr && font != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

HFONT CreateUiFont(HWND window, int pointSize, int weight) {
    const int height = -MulDiv(pointSize, static_cast<int>(GetDpiForWindow(window)), 72);
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

void DestroyUiResources(AppContext* context) {
    if (context == nullptr) {
        return;
    }
    DeleteObject(context->backgroundBrush);
    DeleteObject(context->cardBrush);
    DeleteObject(context->editBrush);
    DeleteObject(context->hotkeyBoxBrush);
    DeleteObject(context->titleFont);
    DeleteObject(context->badgeFont);
    DeleteObject(context->subFont);
    DeleteObject(context->cardTitleFont);
    DeleteObject(context->cardSubFont);
    DeleteObject(context->labelFont);
    DeleteObject(context->bodyFont);
    DeleteObject(context->statusBigFont);
    DeleteObject(context->actionFont);
    DeleteObject(context->hotkeyFont);
    DeleteObject(context->btnChangeFont);
    context->backgroundBrush = nullptr;
    context->cardBrush = nullptr;
    context->editBrush = nullptr;
    context->hotkeyBoxBrush = nullptr;
    context->titleFont = nullptr;
    context->badgeFont = nullptr;
    context->subFont = nullptr;
    context->cardTitleFont = nullptr;
    context->cardSubFont = nullptr;
    context->labelFont = nullptr;
    context->bodyFont = nullptr;
    context->statusBigFont = nullptr;
    context->actionFont = nullptr;
    context->hotkeyFont = nullptr;
    context->btnChangeFont = nullptr;
}

void CreateUiResources(AppContext* context) {
    if (context == nullptr || context->window == nullptr) {
        return;
    }
    DestroyUiResources(context);
    context->backgroundBrush = CreateSolidBrush(context->theme.windowBackground);
    context->cardBrush = CreateSolidBrush(context->theme.cardBackground);
    context->editBrush = CreateSolidBrush(context->theme.editBackground);
    context->hotkeyBoxBrush = CreateSolidBrush(context->theme.hotkeyBoxBackground);
    context->titleFont = CreateUiFont(context->window, 18, FW_BOLD);
    context->badgeFont = CreateUiFont(context->window, 8, FW_SEMIBOLD);
    context->subFont = CreateUiFont(context->window, 9, FW_NORMAL);
    context->cardTitleFont = CreateUiFont(context->window, 11, FW_SEMIBOLD);
    context->cardSubFont = CreateUiFont(context->window, 8, FW_NORMAL);
    context->labelFont = CreateUiFont(context->window, 9, FW_NORMAL);
    context->bodyFont = CreateUiFont(context->window, 10, FW_NORMAL);
    context->statusBigFont = CreateUiFont(context->window, 14, FW_BOLD);
    context->actionFont = CreateUiFont(context->window, 12, FW_SEMIBOLD);
    context->hotkeyFont = CreateUiFont(context->window, 10, FW_SEMIBOLD);
    context->btnChangeFont = CreateUiFont(context->window, 9, FW_SEMIBOLD);
}

struct LayoutMetrics {
    RECT intervalCard{};
    RECT optionCard{};
    RECT statusCard{};
    RECT hotkeyCard{};
    RECT actionButton{};
    RECT intervalInputBox{};
    RECT hotkeyInputBox{};
    int mainLeft = 0;
    int mainRight = 0;
};

RECT MakeRect(int x, int y, int width, int height) {
    return RECT{x, y, x + width, y + height};
}

LayoutMetrics CalculateLayout(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int margin = DpiScale(window, 16);
    const int gap = DpiScale(window, 12);
    const int mainLeft = margin;
    const int mainRight = client.right > margin ? client.right - margin : mainLeft;
    const int contentWidth = mainRight > mainLeft ? mainRight - mainLeft : 0;
    const int leftCardWidth = (contentWidth - gap) / 2;
    const int rightCardWidth = contentWidth - leftCardWidth - gap;

    const int headerHeight = DpiScale(window, 58);
    const int intervalTop = headerHeight + DpiScale(window, 8);
    const int intervalHeight = DpiScale(window, 68);

    const int actionHeight = DpiScale(window, 46);
    const int actionBottom = client.bottom - DpiScale(window, 16);
    const int actionTop = actionBottom > actionHeight ? actionBottom - actionHeight : 0;

    const int hotkeyHeight = DpiScale(window, 68);
    const int hotkeyBottom = actionTop > DpiScale(window, 10) ? actionTop - DpiScale(window, 10) : 0;
    const int hotkeyTop = hotkeyBottom > hotkeyHeight ? hotkeyBottom - hotkeyHeight : 0;

    const int detailTop = intervalTop + intervalHeight + gap;
    const int detailBottom = hotkeyTop > gap ? hotkeyTop - gap : detailTop;
    const int detailHeight = detailBottom > detailTop ? detailBottom - detailTop : DpiScale(window, 140);

    LayoutMetrics layout;
    layout.intervalCard = MakeRect(mainLeft, intervalTop, contentWidth, intervalHeight);
    layout.optionCard = MakeRect(mainLeft, detailTop, leftCardWidth, detailHeight);
    layout.statusCard = MakeRect(mainLeft + leftCardWidth + gap, detailTop, rightCardWidth, detailHeight);
    layout.hotkeyCard = MakeRect(mainLeft, hotkeyTop, contentWidth, hotkeyHeight);
    layout.actionButton = MakeRect(mainLeft, actionTop, contentWidth, actionHeight);
    layout.mainLeft = mainLeft;
    layout.mainRight = mainRight;

    // Card 1 input box rect
    const int boxW1 = DpiScale(window, 175);
    const int boxH1 = DpiScale(window, 34);
    const int msW = DpiScale(window, 24);
    const int boxRight1 = layout.intervalCard.right - DpiScale(window, 16) - msW;
    const int boxLeft1 = boxRight1 - boxW1;
    const int boxTop1 = layout.intervalCard.top + (intervalHeight - boxH1) / 2;
    layout.intervalInputBox = MakeRect(boxLeft1, boxTop1, boxW1, boxH1);

    // Card 4 input box rect
    const int btnW = DpiScale(window, 76);
    const int boxW4 = DpiScale(window, 185);
    const int boxH4 = DpiScale(window, 34);
    const int btnRight = layout.hotkeyCard.right - DpiScale(window, 16);
    const int btnLeft = btnRight - btnW;
    const int boxRight4 = btnLeft - DpiScale(window, 10);
    const int boxLeft4 = boxRight4 - boxW4;
    const int boxTop4 = layout.hotkeyCard.top + (hotkeyHeight - boxH4) / 2;
    layout.hotkeyInputBox = MakeRect(boxLeft4, boxTop4, boxW4, boxH4);

    return layout;
}

bool IsLayoutValid(const LayoutMetrics& layout, const RECT& client, HWND window) {
    if (client.right <= 0 || client.bottom <= 0) {
        return true;
    }
    const RECT cards[] = {layout.intervalCard, layout.optionCard,
                          layout.statusCard, layout.hotkeyCard, layout.actionButton};
    for (const RECT& rect : cards) {
        if (!RectWithinClient(rect, client.right, client.bottom)) {
            return false;
        }
    }
    if (RectsOverlap(layout.optionCard, layout.statusCard) ||
        RectsOverlap(layout.intervalCard, layout.optionCard) ||
        RectsOverlap(layout.intervalCard, layout.statusCard) ||
        RectsOverlap(layout.hotkeyCard, layout.optionCard) ||
        RectsOverlap(layout.hotkeyCard, layout.statusCard) ||
        RectsOverlap(layout.actionButton, layout.hotkeyCard)) {
        return false;
    }
    return layout.actionButton.top >= layout.hotkeyCard.bottom + DpiScale(window, 8);
}

void SetError(AppContext* context, WideTextView message) {
    if (context == nullptr) {
        return;
    }
    context->lastError.Assign(message);
    if (context->error != nullptr) {
        SetWindowTextW(context->error, context->lastError.c_str());
    }
    if (context->window != nullptr) {
        RECT client{};
        GetClientRect(context->window, &client);
        const LayoutMetrics layout = CalculateLayout(context->window);
        InvalidateRect(context->window, &layout.statusCard, TRUE);
    }
}

void UpdateWindowClientSize(AppContext* context) {
    if (context == nullptr) {
        return;
    }
    context->settings.clientWidth = kDefaultWindowClientWidth;
    context->settings.clientHeight = kDefaultWindowClientHeight;
}

void SaveWindowSize(AppContext* context) {
    if (context == nullptr) {
        return;
    }
    ProductMessage saveError;
    if (!SaveSettings(context->settings, &saveError)) {
        SetError(context, saveError.view());
    }
}

void UpdateHotkeyText(HWND control, const Hotkey& hotkey) {
    HotkeyText text;
    if (control != nullptr && FormatHotkey(hotkey, &text)) {
        BoundedWideString<64> spaced;
        for (std::size_t i = 0; i < text.length(); ++i) {
            if (text.c_str()[i] == L'+') {
                spaced.Append(L" + ");
            } else {
                wchar_t ch[2] = {text.c_str()[i], L'\0'};
                spaced.Append(ch);
            }
        }
        SetWindowTextW(control, spaced.c_str());
    }
}

void SetControlsEnabled(AppContext* context, bool enabled) {
    if (context == nullptr) {
        return;
    }
    EnableWindow(context->buttonSelect, enabled);
    EnableWindow(context->clickTypeSelect, enabled);
    EnableWindow(context->interval, enabled);
    EnableWindow(context->intervalSpin, enabled);
    EnableWindow(context->hotkey, enabled);
    EnableWindow(context->hotkeyCapture, enabled);
    EnableWindow(context->themeLight, enabled);
    EnableWindow(context->themeDark, enabled);
    EnableWindow(context->themeSystem, enabled);
}

void UpdateStatePresentation(AppContext* context) {
    if (context == nullptr) {
        return;
    }

    const bool active = context->state == AppState::Starting ||
                        context->state == AppState::Running ||
                        context->state == AppState::Stopping;
    SetControlsEnabled(context, !active);

    if (context->status != nullptr) {
        const wchar_t* status = L"已暂停";
        if (context->state == AppState::Starting) {
            status = L"启动中";
        } else if (context->state == AppState::Running) {
            status = L"运行中";
        } else if (context->state == AppState::Stopping) {
            status = L"停止中";
        } else if (context->state == AppState::Error) {
            status = L"需要处理";
        }
        SetWindowTextW(context->status, status);
    }

    if (context->startPause != nullptr) {
        BoundedWideString<96> label;
        HotkeyText hotkey;
        FormatHotkey(context->settings.hotkey, &hotkey);
        if (context->state == AppState::Running) {
            label.Assign(L"Ⅱ  暂停 (");
            label.Append(hotkey.view());
            label.Append(L")");
        } else {
            label.Assign(L"▶  开始 (");
            label.Append(hotkey.view());
            label.Append(L")");
        }
        if (context->state == AppState::Starting) {
            label.Assign(L"启动中...");
        } else if (context->state == AppState::Stopping) {
            label.Assign(L"停止中...");
        }
        SetWindowTextW(context->startPause, label.c_str());
        EnableWindow(context->startPause, context->state != AppState::Starting &&
                                              context->state != AppState::Stopping);
        InvalidateRect(context->startPause, nullptr, TRUE);
    }

    if (context->window != nullptr) {
        const LayoutMetrics layout = CalculateLayout(context->window);
        InvalidateRect(context->window, &layout.statusCard, TRUE);
    }
}

void SetState(AppContext* context, AppState state, WideTextView error = {}) {
    if (context == nullptr) {
        return;
    }
    context->state = state;
    if (!error.empty()) {
        SetError(context, error);
    } else if (state != AppState::Error) {
        SetError(context, WideTextView{});
    }
    UpdateStatePresentation(context);
}

void ClearSettingsError(AppContext* context) {
    if (context == nullptr) {
        return;
    }
    if (context->state == AppState::Error && !context->worker.IsRunning()) {
        SetState(context, AppState::Paused);
    } else {
        SetError(context, WideTextView{});
    }
}

HWND CreateChild(AppContext* context, const wchar_t* className, const wchar_t* text,
                 DWORD style, DWORD exStyle, int id) {
    return CreateWindowExW(exStyle, className, text, style,
                           0, 0, 0, 0, context->window,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           GetModuleHandleW(nullptr), nullptr);
}

int ThemeControlId(ThemeMode mode) {
    if (mode == ThemeMode::Light) {
        return IDC_THEME_LIGHT;
    }
    if (mode == ThemeMode::Dark) {
        return IDC_THEME_DARK;
    }
    return IDC_THEME_SYSTEM;
}

ThemeMode ThemeModeFromControlId(int id) {
    if (id == IDC_THEME_LIGHT) {
        return ThemeMode::Light;
    }
    if (id == IDC_THEME_DARK) {
        return ThemeMode::Dark;
    }
    return ThemeMode::System;
}

void ApplyControlFonts(AppContext* context) {
    if (context == nullptr || context->window == nullptr) {
        return;
    }
    SetFont(context->interval, context->bodyFont);
    SetFont(context->buttonSelect, context->bodyFont);
    SetFont(context->clickTypeSelect, context->bodyFont);
    SetFont(context->hotkey, context->hotkeyFont);
    SetFont(context->hotkeyCapture, context->btnChangeFont);
    SetFont(context->startPause, context->actionFont);
}

bool AddComboItem(HWND comboBox, const wchar_t* label, std::uint32_t value) {
    const LRESULT index = SendMessageW(comboBox, CB_ADDSTRING, 0,
                                       reinterpret_cast<LPARAM>(label));
    if (index == CB_ERR || index == CB_ERRSPACE) {
        return false;
    }
    return SendMessageW(comboBox, CB_SETITEMDATA, static_cast<WPARAM>(index),
                        static_cast<LPARAM>(value)) != CB_ERR;
}

bool SelectComboItemData(HWND comboBox, std::uint32_t value) {
    const LRESULT count = SendMessageW(comboBox, CB_GETCOUNT, 0, 0);
    for (LRESULT index = 0; index < count; ++index) {
        if (SendMessageW(comboBox, CB_GETITEMDATA, static_cast<WPARAM>(index), 0) ==
            static_cast<LRESULT>(value)) {
            return SendMessageW(comboBox, CB_SETCURSEL, static_cast<WPARAM>(index), 0) != CB_ERR;
        }
    }
    return false;
}

bool ReadComboItemData(HWND comboBox, std::uint32_t* value) {
    if (comboBox == nullptr || value == nullptr) {
        return false;
    }
    const LRESULT index = SendMessageW(comboBox, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR) {
        return false;
    }
    const LRESULT itemData = SendMessageW(comboBox, CB_GETITEMDATA,
                                          static_cast<WPARAM>(index), 0);
    if (itemData == CB_ERR || itemData < 0) {
        return false;
    }
    *value = static_cast<std::uint32_t>(itemData);
    return true;
}

void SetAccessibleName(HWND control, const wchar_t* name) {
    if (control == nullptr || name == nullptr) {
        return;
    }
    constexpr CLSID propertyServicesClassId{
        0xb5f8350b, 0x0548, 0x48b1, {0xa6, 0xee, 0x88, 0xbd, 0x00, 0xb4, 0xa5, 0xe7}};
    constexpr IID propertyServicesInterfaceId{
        0x6e26e776, 0x04f0, 0x495d, {0x80, 0xe4, 0x33, 0x30, 0x35, 0x2e, 0x31, 0x69}};
    IAccPropServices* services = nullptr;
    if (SUCCEEDED(CoCreateInstance(propertyServicesClassId, nullptr,
                                   CLSCTX_INPROC_SERVER, propertyServicesInterfaceId,
                                   reinterpret_cast<void**>(&services)))) {
        constexpr MSAAPROPID accessibleNameProperty{
            0x608d3df8, 0x8128, 0x4aa7, {0xa4, 0x28, 0xf5, 0x5e, 0x49, 0x26, 0x72, 0x91}};
        services->SetHwndPropStr(control, static_cast<DWORD>(OBJID_CLIENT),
                                 CHILDID_SELF, accessibleNameProperty, name);
        services->Release();
    }
}

void CreateControls(AppContext* context) {
    const DWORD labelStyle = WS_CHILD | SS_LEFT | SS_NOPREFIX;
    const DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;
    const DWORD buttonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    const DWORD comboStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                             CBS_DROPDOWNLIST | CBS_HASSTRINGS;

    // Background legacy labels (kept hidden for safety)
    CreateChild(context, WC_STATICW, L"MouseClick", labelStyle, 0, IDC_BRAND);
    CreateChild(context, WC_STATICW, L"v1.0.0", labelStyle, 0, IDC_VERSION);
    CreateChild(context, WC_STATICW, L"", labelStyle, 0, IDC_NAV_MAIN);
    CreateChild(context, WC_STATICW, L"", labelStyle, 0, IDC_PAGE_TITLE);
    CreateChild(context, WC_STATICW, L"", labelStyle, 0, IDC_PAGE_SUBTITLE);
    CreateChild(context, WC_STATICW, L"点击间隔", labelStyle, 0, IDC_LABEL_INTERVAL);
    CreateChild(context, WC_STATICW, L"毫秒（10–1000）", labelStyle, 0, IDC_INTERVAL_HELP);
    CreateChild(context, WC_STATICW, L"点击选项", labelStyle, 0, IDC_CLICK_OPTIONS_LABEL);
    CreateChild(context, WC_STATICW, L"鼠标按键", labelStyle, 0, IDC_LABEL_MOUSE_BUTTON);
    CreateChild(context, WC_STATICW, L"点击类型", labelStyle, 0, IDC_LABEL_CLICK_TYPE);
    CreateChild(context, WC_STATICW, L"运行状态", labelStyle, 0, IDC_STATUS_LABEL);
    CreateChild(context, WC_STATICW, L"设置已就绪", labelStyle, 0, IDC_STATUS_HELP);
    CreateChild(context, WC_STATICW, L"当前快捷键", labelStyle, 0, IDC_LABEL_HOTKEY);
    CreateChild(context, WC_STATICW, L"用于开始与暂停", labelStyle, 0, IDC_HOTKEY_HELP);

    context->status = CreateChild(context, WC_STATICW, L"已暂停", labelStyle, 0, IDC_STATUS);
    context->error = CreateChild(context, WC_STATICW, L"", labelStyle, 0, IDC_ERROR);

    context->themeLight = CreateChild(context, WC_BUTTONW, L"亮色", buttonStyle | BS_OWNERDRAW, 0, IDC_THEME_LIGHT);
    context->themeDark = CreateChild(context, WC_BUTTONW, L"暗色", buttonStyle | BS_OWNERDRAW, 0, IDC_THEME_DARK);
    context->themeSystem = CreateChild(context, WC_BUTTONW, L"跟随系统", buttonStyle | BS_OWNERDRAW, 0, IDC_THEME_SYSTEM);

    // Interactive Tab-ordered controls:
    // Tab 1: Interval
    context->interval = CreateChild(context, WC_EDITW, L"20", editStyle | ES_NUMBER, 0, IDC_INTERVAL);
    context->intervalSpin = CreateChild(context, UPDOWN_CLASSW, nullptr,
                                        WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                                            UDS_ARROWKEYS,
                                        0, IDC_INTERVAL_SPIN);
    SendMessageW(context->intervalSpin, UDM_SETBUDDY, reinterpret_cast<WPARAM>(context->interval), 0);
    SendMessageW(context->intervalSpin, UDM_SETRANGE32,
                 static_cast<WPARAM>(kMinimumIntervalMilliseconds),
                 static_cast<LPARAM>(kMaximumIntervalMilliseconds));

    // Tab 2: Button select
    context->buttonSelect = CreateChild(context, WC_COMBOBOXW, nullptr, comboStyle | WS_GROUP, 0, IDC_MOUSE_BUTTON_SELECT);

    // Tab 3: Click type select
    context->clickTypeSelect = CreateChild(context, WC_COMBOBOXW, nullptr, comboStyle, 0, IDC_CLICK_TYPE_SELECT);

    if (!AddComboItem(context->buttonSelect, L"左键", static_cast<std::uint32_t>(ClickButton::Left)) ||
        !AddComboItem(context->buttonSelect, L"中键", static_cast<std::uint32_t>(ClickButton::Middle)) ||
        !AddComboItem(context->buttonSelect, L"右键", static_cast<std::uint32_t>(ClickButton::Right)) ||
        !AddComboItem(context->clickTypeSelect, L"单次点击", static_cast<std::uint32_t>(ClickType::Single)) ||
        !AddComboItem(context->clickTypeSelect, L"双击", static_cast<std::uint32_t>(ClickType::Double))) {
        SetError(context, L"无法初始化鼠标点击选项。");
    }
    SendMessageW(context->buttonSelect, CB_SETMINVISIBLE, 3, 0);
    SendMessageW(context->clickTypeSelect, CB_SETMINVISIBLE, 2, 0);
    SetAccessibleName(context->buttonSelect, L"鼠标按键");
    SetAccessibleName(context->clickTypeSelect, L"点击类型");

    // Tab 4: Hotkey display
    context->hotkey = CreateChild(context, WC_EDITW, L"Ctrl + Alt + F6",
                                  editStyle | ES_READONLY | ES_CENTER, 0, IDC_HOTKEY);

    // Tab 5: Hotkey capture
    context->hotkeyCapture = CreateChild(context, WC_BUTTONW, L"更改",
                                         buttonStyle | BS_OWNERDRAW, 0, IDC_HOTKEY_CAPTURE);

    // Tab 6: Start/Pause action
    context->startPause = CreateChild(context, WC_BUTTONW, L"▶  开始",
                                      buttonStyle | BS_OWNERDRAW, 0, IDC_START_PAUSE);

    ApplyControlFonts(context);

    // Subclass hotkey edit for keyboard capture
    context->hotkeyEditProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        context->hotkey, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&HotkeyEditProc)));
    SetWindowLongPtrW(context->hotkey, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));

    // Subclass buttons for mouse hover effects
    context->startPauseProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        context->startPause, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ButtonHoverProc)));
    SetWindowLongPtrW(context->startPause, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));

    context->captureProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        context->hotkeyCapture, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ButtonHoverProc)));
    SetWindowLongPtrW(context->hotkeyCapture, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));

    context->themeBtnProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        context->themeLight, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ButtonHoverProc)));
    SetWindowLongPtrW(context->themeLight, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));

    SetWindowLongPtrW(context->themeDark, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ButtonHoverProc));
    SetWindowLongPtrW(context->themeDark, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));

    SetWindowLongPtrW(context->themeSystem, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ButtonHoverProc));
    SetWindowLongPtrW(context->themeSystem, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));

    // Hide background helper statics
    const int hideIds[] = {IDC_BRAND, IDC_VERSION, IDC_NAV_MAIN, IDC_PAGE_TITLE,
                           IDC_PAGE_SUBTITLE, IDC_LABEL_INTERVAL, IDC_INTERVAL_HELP,
                           IDC_CLICK_OPTIONS_LABEL, IDC_LABEL_MOUSE_BUTTON,
                           IDC_LABEL_CLICK_TYPE, IDC_STATUS_LABEL, IDC_STATUS_HELP,
                           IDC_STATUS, IDC_ERROR, IDC_LABEL_HOTKEY, IDC_HOTKEY_HELP};
    for (int id : hideIds) {
        ShowWindow(GetDlgItem(context->window, id), SW_HIDE);
    }
}

void LayoutControls(AppContext* context) {
    if (context == nullptr || context->window == nullptr) {
        return;
    }
    RECT client{};
    GetClientRect(context->window, &client);
    if (client.right <= 0 || client.bottom <= 0) {
        return;
    }
    const LayoutMetrics layout = CalculateLayout(context->window);
    if (!IsLayoutValid(layout, client, context->window)) {
        SetError(context, L"窗口空间不足，无法完整显示控件。");
        return;
    }

    // Interval Edit & Spin inside layout.intervalInputBox
    const int spinW = DpiScale(context->window, 18);
    const int editH = DpiScale(context->window, 22);
    const int boxPad = DpiScale(context->window, 8);
    const int editY = layout.intervalInputBox.top + (layout.intervalInputBox.bottom - layout.intervalInputBox.top - editH) / 2;

    MoveWindow(context->interval, layout.intervalInputBox.left + boxPad,
               editY, layout.intervalInputBox.right - layout.intervalInputBox.left - boxPad - spinW - DpiScale(context->window, 4),
               editH, TRUE);
    MoveWindow(context->intervalSpin, layout.intervalInputBox.right - spinW - DpiScale(context->window, 2),
               layout.intervalInputBox.top + DpiScale(context->window, 2),
               spinW, layout.intervalInputBox.bottom - layout.intervalInputBox.top - DpiScale(context->window, 4), TRUE);

    // Option comboboxes
    const int pad = DpiScale(context->window, 16);
    const int comboW = layout.optionCard.right - layout.optionCard.left - 2 * pad;
    const int comboH = DpiScale(context->window, 34);
    const int comboItemH = DpiScale(context->window, 24);

    SendMessageW(context->buttonSelect, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), comboH - DpiScale(context->window, 8));
    SendMessageW(context->buttonSelect, CB_SETITEMHEIGHT, 0, comboItemH);
    SendMessageW(context->clickTypeSelect, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), comboH - DpiScale(context->window, 8));
    SendMessageW(context->clickTypeSelect, CB_SETITEMHEIGHT, 0, comboItemH);

    const int labelH = DpiScale(context->window, 16);
    const int y1 = layout.optionCard.top + DpiScale(context->window, 50);
    const int combo1Y = y1 + labelH + DpiScale(context->window, 4);
    const int y2 = combo1Y + comboH + DpiScale(context->window, 8);
    const int combo2Y = y2 + labelH + DpiScale(context->window, 4);

    MoveWindow(context->buttonSelect, layout.optionCard.left + pad, combo1Y,
               comboW, comboH + 3 * comboItemH, TRUE);
    MoveWindow(context->clickTypeSelect, layout.optionCard.left + pad, combo2Y,
               comboW, comboH + 2 * comboItemH, TRUE);

    // Hotkey display box and Change button
    const int hotkeyEditH = DpiScale(context->window, 22);
    const int hotkeyEditY = layout.hotkeyInputBox.top + (layout.hotkeyInputBox.bottom - layout.hotkeyInputBox.top - hotkeyEditH) / 2;
    MoveWindow(context->hotkey, layout.hotkeyInputBox.left + boxPad,
               hotkeyEditY, layout.hotkeyInputBox.right - layout.hotkeyInputBox.left - 2 * boxPad,
               hotkeyEditH, TRUE);

    const int btnW = DpiScale(context->window, 76);
    MoveWindow(context->hotkeyCapture, layout.hotkeyCard.right - DpiScale(context->window, 16) - btnW,
               layout.hotkeyInputBox.top, btnW,
               layout.hotkeyInputBox.bottom - layout.hotkeyInputBox.top, TRUE);

    // Big action button at bottom
    MoveWindow(context->startPause, layout.actionButton.left, layout.actionButton.top,
               layout.actionButton.right - layout.actionButton.left,
               layout.actionButton.bottom - layout.actionButton.top, TRUE);

    // Segmented theme control layout in header
    const UINT dpi = GetDpiForWindow(context->window);
    const int captionBtnW = MulDiv(40, static_cast<int>(dpi), 96);
    const int captionBtnAreaW = 2 * captionBtnW;

    const int themeGap = DpiScale(context->window, 12);
    const int themeH = DpiScale(context->window, 28);
    const int themeY = DpiScale(context->window, 14);
    const int themeRight = client.right - captionBtnAreaW - themeGap;

    const bool compactTheme = client.right < DpiScale(context->window, 540);
    const int segW1 = DpiScale(context->window, compactTheme ? 36 : 42);
    const int segW2 = DpiScale(context->window, compactTheme ? 36 : 42);
    const int segW3 = DpiScale(context->window, compactTheme ? 38 : 64);
    const int totalThemeW = segW1 + segW2 + segW3;
    const int themeLeft = themeRight - totalThemeW;

    SetWindowTextW(context->themeLight, L"亮色");
    SetWindowTextW(context->themeDark, L"暗色");
    SetWindowTextW(context->themeSystem, compactTheme ? L"系统" : L"跟随系统");

    MoveWindow(context->themeLight, themeLeft, themeY, segW1, themeH, TRUE);
    MoveWindow(context->themeDark, themeLeft + segW1, themeY, segW2, themeH, TRUE);
    MoveWindow(context->themeSystem, themeLeft + segW1 + segW2, themeY, segW3, themeH, TRUE);
}

void DrawRoundedSurface(HDC dc, const RECT& rect, COLORREF fillColor,
                        COLORREF borderColor, int radius) {
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    const HGDIOBJ prevBrush = SelectObject(dc, brush);
    const HGDIOBJ prevPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, prevPen);
    SelectObject(dc, prevBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawTopRightWave(HDC dc, int w, int h, COLORREF waveColor, UINT dpi) {
    const int scale = static_cast<int>(dpi);
    auto S = [scale](int v) { return MulDiv(v, scale, 96); };

    POINT pts[9];
    pts[0] = {w - S(130), 0};
    pts[1] = {w - S(110), S(12)};
    pts[2] = {w - S(90), S(26)};
    pts[3] = {w - S(70), S(42)};
    pts[4] = {w - S(55), S(58)};
    pts[5] = {w - S(38), S(72)};
    pts[6] = {w - S(20), S(80)};
    pts[7] = {w, S(84)};
    pts[8] = {w, 0};

    HBRUSH brush = CreateSolidBrush(waveColor);
    HPEN pen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);

    Polygon(dc, pts, 9);

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawCaptionButtons(HDC dc, const RECT& client, int hovered, int pressed, bool dark, UINT dpi) {
    const int btnW = MulDiv(40, static_cast<int>(dpi), 96);
    const int btnH = MulDiv(30, static_cast<int>(dpi), 96);
    const int strokeW = MulDiv(1, static_cast<int>(dpi), 96) > 0 ? MulDiv(1, static_cast<int>(dpi), 96) : 1;

    for (int btn = 1; btn <= 2; ++btn) {
        RECT r{client.right - (3 - btn) * btnW, 0, client.right - (2 - btn) * btnW, btnH};
        COLORREF iconColor = dark ? RGB(160, 175, 195) : RGB(90, 106, 128);

        if (hovered == btn) {
            if (btn == 2) { // Close button hover
                HBRUSH redBrush = CreateSolidBrush(RGB(232, 17, 35));
                FillRect(dc, &r, redBrush);
                DeleteObject(redBrush);
                iconColor = RGB(255, 255, 255);
            } else {
                HBRUSH hoverBrush = CreateSolidBrush(dark ? RGB(45, 52, 66) : RGB(214, 227, 244));
                FillRect(dc, &r, hoverBrush);
                DeleteObject(hoverBrush);
            }
        }

        const int cx = (r.left + r.right) / 2;
        const int cy = (r.top + r.bottom) / 2;

        HPEN pen = CreatePen(PS_SOLID, strokeW, iconColor);
        HGDIOBJ oldPen = SelectObject(dc, pen);

        if (btn == 1) { // Minimize: horizontal line
            const int hw = MulDiv(5, static_cast<int>(dpi), 96);
            MoveToEx(dc, cx - hw, cy, nullptr);
            LineTo(dc, cx + hw + 1, cy);
        } else if (btn == 2) { // Close: X
            const int s = MulDiv(4, static_cast<int>(dpi), 96);
            MoveToEx(dc, cx - s, cy - s, nullptr);
            LineTo(dc, cx + s + 1, cy + s + 1);
            MoveToEx(dc, cx + s, cy - s, nullptr);
            LineTo(dc, cx - s - 1, cy + s + 1);
        }

        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
}

void DrawAppIcon(HDC dc, int x, int y, int size, UINT dpi) {
    const int radius = MulDiv(12, static_cast<int>(dpi), 96);
    RECT rect{x, y, x + size, y + size};

    HBRUSH bgBrush = CreateSolidBrush(RGB(37, 117, 252));
    HPEN nullPen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ oldBrush = SelectObject(dc, bgBrush);
    HGDIOBJ oldPen = SelectObject(dc, nullPen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);

    const int cx = x + MulDiv(24, static_cast<int>(dpi), 96);
    const int cy = y + MulDiv(25, static_cast<int>(dpi), 96);
    const int capR = MulDiv(6, static_cast<int>(dpi), 96);
    const int offset = MulDiv(4, static_cast<int>(dpi), 96);

    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(dc, whiteBrush);

    Ellipse(dc, cx - offset - capR, cy - offset - capR, cx - offset + capR + 1, cy - offset + capR + 1);
    Ellipse(dc, cx + offset - capR, cy + offset - capR, cx + offset + capR + 1, cy + offset + capR + 1);

    const int nx = MulDiv(4, static_cast<int>(dpi), 96);
    const int ny = MulDiv(4, static_cast<int>(dpi), 96);
    POINT bodyPts[4];
    bodyPts[0] = {cx - offset + nx, cy - offset - ny};
    bodyPts[1] = {cx + offset + nx, cy + offset - ny};
    bodyPts[2] = {cx + offset - nx, cy + offset + ny};
    bodyPts[3] = {cx - offset - nx, cy - offset + ny};
    Polygon(dc, bodyPts, 4);

    const int strokeW = MulDiv(1, static_cast<int>(dpi), 96) > 0 ? MulDiv(1, static_cast<int>(dpi), 96) : 1;
    HPEN bluePen = CreatePen(PS_SOLID, strokeW, RGB(37, 117, 252));
    SelectObject(dc, bluePen);
    MoveToEx(dc, cx - offset - MulDiv(3, static_cast<int>(dpi), 96), cy - offset - MulDiv(3, static_cast<int>(dpi), 96), nullptr);
    LineTo(dc, cx - offset + MulDiv(3, static_cast<int>(dpi), 96), cy - offset + MulDiv(3, static_cast<int>(dpi), 96));
    DeleteObject(bluePen);

    const int rayW = MulDiv(2, static_cast<int>(dpi), 96) > 0 ? MulDiv(2, static_cast<int>(dpi), 96) : 1;
    HPEN whitePen = CreatePen(PS_SOLID, rayW, RGB(255, 255, 255));
    SelectObject(dc, whitePen);

    MoveToEx(dc, cx - MulDiv(11, static_cast<int>(dpi), 96), cy - MulDiv(11, static_cast<int>(dpi), 96), nullptr);
    LineTo(dc, cx - MulDiv(16, static_cast<int>(dpi), 96), cy - MulDiv(16, static_cast<int>(dpi), 96));
    MoveToEx(dc, cx - MulDiv(7, static_cast<int>(dpi), 96), cy - MulDiv(13, static_cast<int>(dpi), 96), nullptr);
    LineTo(dc, cx - MulDiv(9, static_cast<int>(dpi), 96), cy - MulDiv(19, static_cast<int>(dpi), 96));
    MoveToEx(dc, cx - MulDiv(13, static_cast<int>(dpi), 96), cy - MulDiv(7, static_cast<int>(dpi), 96), nullptr);
    LineTo(dc, cx - MulDiv(19, static_cast<int>(dpi), 96), cy - MulDiv(9, static_cast<int>(dpi), 96));

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(whitePen);
    DeleteObject(whiteBrush);
    DeleteObject(nullPen);
    DeleteObject(bgBrush);
}

void DrawClockIcon(HDC dc, int cx, int cy, int radius, COLORREF bg, COLORREF fg, UINT dpi) {
    HBRUSH bgBrush = CreateSolidBrush(bg);
    HPEN nullPen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ oldBrush = SelectObject(dc, bgBrush);
    HGDIOBJ oldPen = SelectObject(dc, nullPen);
    Ellipse(dc, cx - radius, cy - radius, cx + radius + 1, cy + radius + 1);

    const int clockR = MulDiv(9, static_cast<int>(dpi), 96);
    const int strokeW = MulDiv(2, static_cast<int>(dpi), 96) > 0 ? MulDiv(2, static_cast<int>(dpi), 96) : 1;
    HPEN fgPen = CreatePen(PS_SOLID, strokeW, fg);
    SelectObject(dc, fgPen);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, cx - clockR, cy - clockR, cx + clockR + 1, cy + clockR + 1);

    MoveToEx(dc, cx, cy, nullptr);
    LineTo(dc, cx, cy - MulDiv(5, static_cast<int>(dpi), 96));
    MoveToEx(dc, cx, cy, nullptr);
    LineTo(dc, cx + MulDiv(4, static_cast<int>(dpi), 96), cy);

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(fgPen);
    DeleteObject(nullPen);
    DeleteObject(bgBrush);
}

void DrawPointerIcon(HDC dc, int cx, int cy, int radius, COLORREF bg, COLORREF fg, UINT dpi) {
    HBRUSH bgBrush = CreateSolidBrush(bg);
    HPEN nullPen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ oldBrush = SelectObject(dc, bgBrush);
    HGDIOBJ oldPen = SelectObject(dc, nullPen);
    Ellipse(dc, cx - radius, cy - radius, cx + radius + 1, cy + radius + 1);

    const int scale = static_cast<int>(dpi);
    auto S = [scale](int v) { return MulDiv(v, scale, 96); };

    POINT pts[7];
    pts[0] = {cx - S(5), cy - S(7)};
    pts[1] = {cx + S(5), cy + S(2)};
    pts[2] = {cx + S(1), cy + S(2)};
    pts[3] = {cx + S(3), cy + S(7)};
    pts[4] = {cx + 0,    cy + S(7)};
    pts[5] = {cx - S(2), cy + S(2)};
    pts[6] = {cx - S(5), cy + S(3)};

    HBRUSH fgBrush = CreateSolidBrush(fg);
    SelectObject(dc, fgBrush);
    Polygon(dc, pts, 7);

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(fgBrush);
    DeleteObject(nullPen);
    DeleteObject(bgBrush);
}

void DrawPulseIcon(HDC dc, int cx, int cy, int radius, COLORREF bg, COLORREF fg, UINT dpi) {
    HBRUSH bgBrush = CreateSolidBrush(bg);
    HPEN nullPen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ oldBrush = SelectObject(dc, bgBrush);
    HGDIOBJ oldPen = SelectObject(dc, nullPen);
    Ellipse(dc, cx - radius, cy - radius, cx + radius + 1, cy + radius + 1);

    const int scale = static_cast<int>(dpi);
    auto S = [scale](int v) { return MulDiv(v, scale, 96); };
    const int strokeW = S(2) > 0 ? S(2) : 1;

    HPEN fgPen = CreatePen(PS_SOLID, strokeW, fg);
    SelectObject(dc, fgPen);

    POINT pts[6];
    pts[0] = {cx - S(9), cy};
    pts[1] = {cx - S(4), cy};
    pts[2] = {cx - S(1), cy - S(6)};
    pts[3] = {cx + S(2), cy + S(6)};
    pts[4] = {cx + S(5), cy};
    pts[5] = {cx + S(9), cy};

    Polyline(dc, pts, 6);

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(fgPen);
    DeleteObject(nullPen);
    DeleteObject(bgBrush);
}

void DrawKeyboardIcon(HDC dc, int cx, int cy, int radius, COLORREF bg, COLORREF fg, UINT dpi) {
    HBRUSH bgBrush = CreateSolidBrush(bg);
    HPEN nullPen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ oldBrush = SelectObject(dc, bgBrush);
    HGDIOBJ oldPen = SelectObject(dc, nullPen);
    Ellipse(dc, cx - radius, cy - radius, cx + radius + 1, cy + radius + 1);

    const int scale = static_cast<int>(dpi);
    auto S = [scale](int v) { return MulDiv(v, scale, 96); };
    const int strokeW = S(1) > 0 ? S(1) : 1;

    HPEN fgPen = CreatePen(PS_SOLID, strokeW, fg);
    SelectObject(dc, fgPen);
    SelectObject(dc, GetStockObject(NULL_BRUSH));

    const int kw = S(10);
    const int kh = S(7);
    const int kr = S(2);
    RoundRect(dc, cx - kw, cy - kh, cx + kw + 1, cy + kh + 1, kr, kr);

    MoveToEx(dc, cx - S(6), cy - S(2), nullptr);
    LineTo(dc, cx - S(3), cy - S(2));
    MoveToEx(dc, cx - S(1), cy - S(2), nullptr);
    LineTo(dc, cx + S(1), cy - S(2));
    MoveToEx(dc, cx + S(3), cy - S(2), nullptr);
    LineTo(dc, cx + S(6), cy - S(2));

    MoveToEx(dc, cx - S(5), cy + S(3), nullptr);
    LineTo(dc, cx + S(5), cy + S(3));

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(fgPen);
    DeleteObject(nullPen);
    DeleteObject(bgBrush);
}

void DrawStatusBadge(HDC dc, int cx, int cy, int radius, AppState state, COLORREF circleBg, COLORREF symbolFg, UINT dpi) {
    HBRUSH bgBrush = CreateSolidBrush(circleBg);
    HPEN nullPen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ oldBrush = SelectObject(dc, bgBrush);
    HGDIOBJ oldPen = SelectObject(dc, nullPen);
    Ellipse(dc, cx - radius, cy - radius, cx + radius + 1, cy + radius + 1);

    const int scale = static_cast<int>(dpi);
    auto S = [scale](int v) { return MulDiv(v, scale, 96); };

    HBRUSH fgBrush = CreateSolidBrush(symbolFg);
    SelectObject(dc, fgBrush);

    if (state == AppState::Running) {
        POINT pts[3];
        pts[0] = {cx - S(5), cy - S(7)};
        pts[1] = {cx + S(7), cy};
        pts[2] = {cx - S(5), cy + S(7)};
        Polygon(dc, pts, 3);
    } else {
        const int s = S(7);
        const int r = S(3);
        RoundRect(dc, cx - s, cy - s, cx + s + 1, cy + s + 1, r, r);
    }

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(fgBrush);
    DeleteObject(nullPen);
    DeleteObject(bgBrush);
}

void DrawPencilIcon(HDC dc, int x, int y, COLORREF color, UINT dpi) {
    const int scale = static_cast<int>(dpi);
    auto S = [scale](int v) { return MulDiv(v, scale, 96); };

    const int strokeW = S(1) > 0 ? S(1) : 1;
    HPEN pen = CreatePen(PS_SOLID, strokeW, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);

    MoveToEx(dc, x + S(8), y + 0, nullptr);
    LineTo(dc, x + S(2), y + S(6));
    MoveToEx(dc, x + S(10), y + S(2), nullptr);
    LineTo(dc, x + S(4), y + S(8));

    MoveToEx(dc, x + S(2), y + S(6), nullptr);
    LineTo(dc, x + 0, y + S(8));
    LineTo(dc, x + S(4), y + S(8));

    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void PaintWindowSurface(AppContext* context, HDC dc) {
    RECT client{};
    GetClientRect(context->window, &client);
    const UINT dpi = GetDpiForWindow(context->window);

    // 1. Fill window background
    HBRUSH bgBrush = CreateSolidBrush(context->theme.windowBackground);
    FillRect(dc, &client, bgBrush);
    DeleteObject(bgBrush);

    // 2. Draw top-right decorative wave
    DrawTopRightWave(dc, client.right, client.bottom, context->theme.topRightWave, dpi);

    // 3. Draw caption buttons
    DrawCaptionButtons(dc, client, context->hoveredCaptionBtn, context->pressedCaptionBtn, context->theme.dark, dpi);

    const LayoutMetrics layout = CalculateLayout(context->window);

    // 4. Header: App Icon, Title, Badge, Subtitle
    const int iconSize = DpiScale(context->window, 44);
    const int iconX = layout.mainLeft;
    const int iconY = DpiScale(context->window, 12);
    DrawAppIcon(dc, iconX, iconY, iconSize, dpi);

    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, context->titleFont);
    SetTextColor(dc, context->theme.text);

    const int titleX = iconX + iconSize + DpiScale(context->window, 12);
    const int titleY = iconY + DpiScale(context->window, 2);
    TextOutW(dc, titleX, titleY, L"MouseClick", 10);

    SIZE titleSize{};
    GetTextExtentPoint32W(dc, L"MouseClick", 10, &titleSize);

    // Version badge
    const int badgeX = titleX + titleSize.cx + DpiScale(context->window, 8);
    const int badgeY = titleY + DpiScale(context->window, 4);
    const int badgeW = DpiScale(context->window, 46);
    const int badgeH = DpiScale(context->window, 20);
    RECT badgeRect{badgeX, badgeY, badgeX + badgeW, badgeY + badgeH};
    DrawRoundedSurface(dc, badgeRect, context->theme.badgeBg, context->theme.badgeBg, DpiScale(context->window, 10));

    SelectObject(dc, context->badgeFont);
    SetTextColor(dc, context->theme.badgeText);
    DrawTextW(dc, L"v1.0.0", -1, &badgeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Subtitle
    SelectObject(dc, context->subFont);
    SetTextColor(dc, context->theme.subtleText);
    TextOutW(dc, titleX, titleY + DpiScale(context->window, 24), L"简单 · 高效 · 自动点击", 13);

    const int cardRadius = DpiScale(context->window, 14);

    // 5. Card 1: Interval
    DrawRoundedSurface(dc, layout.intervalCard, context->theme.cardBackground, context->theme.cardBorder, cardRadius);

    const int card1CenterY = (layout.intervalCard.top + layout.intervalCard.bottom) / 2;
    const int iconRadius1 = DpiScale(context->window, 18);
    const int iconCx1 = layout.intervalCard.left + DpiScale(context->window, 16) + iconRadius1;
    DrawClockIcon(dc, iconCx1, card1CenterY, iconRadius1, context->theme.intervalIconBg, context->theme.intervalIconFg, dpi);

    const int textX1 = iconCx1 + iconRadius1 + DpiScale(context->window, 12);
    SelectObject(dc, context->cardTitleFont);
    SetTextColor(dc, context->theme.text);
    TextOutW(dc, textX1, card1CenterY - DpiScale(context->window, 16), L"点击间隔", 4);

    SelectObject(dc, context->cardSubFont);
    SetTextColor(dc, context->theme.subtleText);
    TextOutW(dc, textX1, card1CenterY + DpiScale(context->window, 2), L"毫秒 (10 - 1000)", 14);

    // Input box surface
    DrawRoundedSurface(dc, layout.intervalInputBox, context->theme.editBackground, context->theme.controlBorder, DpiScale(context->window, 6));

    // "ms" text
    SelectObject(dc, context->cardSubFont);
    SetTextColor(dc, context->theme.mutedText);
    TextOutW(dc, layout.intervalInputBox.right + DpiScale(context->window, 8),
             card1CenterY - DpiScale(context->window, 7), L"ms", 2);

    // 6. Card 2: Click Options
    DrawRoundedSurface(dc, layout.optionCard, context->theme.cardBackground, context->theme.cardBorder, cardRadius);

    const int iconRadius2 = DpiScale(context->window, 16);
    const int iconCx2 = layout.optionCard.left + DpiScale(context->window, 16) + iconRadius2;
    const int iconCy2 = layout.optionCard.top + DpiScale(context->window, 14) + iconRadius2;
    DrawPointerIcon(dc, iconCx2, iconCy2, iconRadius2, context->theme.optionsIconBg, context->theme.optionsIconFg, dpi);

    const int textX2 = iconCx2 + iconRadius2 + DpiScale(context->window, 10);
    SelectObject(dc, context->cardTitleFont);
    SetTextColor(dc, context->theme.text);
    TextOutW(dc, textX2, iconCy2 - DpiScale(context->window, 9), L"点击选项", 4);

    SelectObject(dc, context->labelFont);
    SetTextColor(dc, context->theme.mutedText);
    const int pad = DpiScale(context->window, 16);
    TextOutW(dc, layout.optionCard.left + pad, layout.optionCard.top + DpiScale(context->window, 50), L"鼠标按键", 4);

    const int comboH = DpiScale(context->window, 34);
    const int y2 = layout.optionCard.top + DpiScale(context->window, 50) + DpiScale(context->window, 16) + DpiScale(context->window, 4) + comboH + DpiScale(context->window, 8);
    TextOutW(dc, layout.optionCard.left + pad, y2, L"点击类型", 4);

    // 7. Card 3: Status
    DrawRoundedSurface(dc, layout.statusCard, context->theme.cardBackground, context->theme.cardBorder, cardRadius);

    const int iconRadius3 = DpiScale(context->window, 16);
    const int iconCx3 = layout.statusCard.left + DpiScale(context->window, 16) + iconRadius3;
    const int iconCy3 = layout.statusCard.top + DpiScale(context->window, 14) + iconRadius3;
    DrawPulseIcon(dc, iconCx3, iconCy3, iconRadius3, context->theme.statusIconBg, context->theme.statusIconFg, dpi);

    const int textX3 = iconCx3 + iconRadius3 + DpiScale(context->window, 10);
    SelectObject(dc, context->cardTitleFont);
    SetTextColor(dc, context->theme.text);
    TextOutW(dc, textX3, iconCy3 - DpiScale(context->window, 9), L"运行状态", 4);

    // Large center status circle badge
    const int cardCenterX = (layout.statusCard.left + layout.statusCard.right) / 2;
    const int statusCircleY = layout.statusCard.top + DpiScale(context->window, 68);
    const int statusCircleRadius = DpiScale(context->window, 24);

    COLORREF circleBg = RGB(235, 243, 254);
    COLORREF symbolFg = RGB(0, 102, 255);
    COLORREF statusTextFg = RGB(0, 102, 255);
    const wchar_t* statusText = L"已暂停";
    const wchar_t* hintText = L"设置已就绪，点击下方按钮开始";

    if (context->state == AppState::Running) {
        circleBg = RGB(230, 249, 238);
        symbolFg = RGB(0, 192, 112);
        statusTextFg = RGB(0, 192, 112);
        statusText = L"运行中";
        hintText = L"正在自动点击中，按快捷键或下方按钮暂停";
    } else if (context->state == AppState::Starting) {
        statusText = L"启动中";
        hintText = L"正在启动点击线程...";
    } else if (context->state == AppState::Stopping) {
        statusText = L"停止中";
        hintText = L"正在停止点击线程...";
    } else if (context->state == AppState::Error) {
        circleBg = RGB(254, 238, 238);
        symbolFg = RGB(224, 49, 49);
        statusTextFg = RGB(224, 49, 49);
        statusText = L"需要处理";
        hintText = context->lastError.empty() ? L"请检查设置" : context->lastError.c_str();
    }

    DrawStatusBadge(dc, cardCenterX, statusCircleY, statusCircleRadius, context->state, circleBg, symbolFg, dpi);

    // Status big text
    SelectObject(dc, context->statusBigFont);
    SetTextColor(dc, statusTextFg);
    RECT statusRect{layout.statusCard.left, statusCircleY + statusCircleRadius + DpiScale(context->window, 10),
                    layout.statusCard.right, statusCircleY + statusCircleRadius + DpiScale(context->window, 34)};
    DrawTextW(dc, statusText, -1, &statusRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Status hint text
    SelectObject(dc, context->cardSubFont);
    SetTextColor(dc, context->state == AppState::Error ? RGB(224, 49, 49) : context->theme.subtleText);
    RECT hintRect{layout.statusCard.left + DpiScale(context->window, 12),
                  statusRect.bottom + DpiScale(context->window, 4),
                  layout.statusCard.right - DpiScale(context->window, 12),
                  statusRect.bottom + DpiScale(context->window, 24)};
    DrawTextW(dc, hintText, -1, &hintRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // 8. Card 4: Hotkey
    DrawRoundedSurface(dc, layout.hotkeyCard, context->theme.cardBackground, context->theme.cardBorder, cardRadius);

    const int card4CenterY = (layout.hotkeyCard.top + layout.hotkeyCard.bottom) / 2;
    const int iconRadius4 = DpiScale(context->window, 18);
    const int iconCx4 = layout.hotkeyCard.left + DpiScale(context->window, 16) + iconRadius4;
    DrawKeyboardIcon(dc, iconCx4, card4CenterY, iconRadius4, context->theme.hotkeyIconBg, context->theme.hotkeyIconFg, dpi);

    const int textX4 = iconCx4 + iconRadius4 + DpiScale(context->window, 12);
    SelectObject(dc, context->cardTitleFont);
    SetTextColor(dc, context->theme.text);
    TextOutW(dc, textX4, card4CenterY - DpiScale(context->window, 16), L"当前快捷键", 5);

    SelectObject(dc, context->cardSubFont);
    SetTextColor(dc, context->theme.subtleText);
    TextOutW(dc, textX4, card4CenterY + DpiScale(context->window, 2), L"用于开始与暂停", 7);

    // Hotkey input box surface
    DrawRoundedSurface(dc, layout.hotkeyInputBox, context->theme.hotkeyBoxBackground, context->theme.controlBorder, DpiScale(context->window, 6));

    SelectObject(dc, oldFont);
}

void DrawOwnerDrawButton(AppContext* context, const DRAWITEMSTRUCT* item) {
    if (context == nullptr || item == nullptr || item->CtlType != ODT_BUTTON) {
        return;
    }

    const bool disabled = (item->itemState & ODS_DISABLED) != 0 ||
                          IsWindowEnabled(item->hwndItem) == FALSE;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;

    if (item->CtlID == IDC_START_PAUSE) {
        FillRect(item->hDC, &item->rcItem, context->backgroundBrush);

        COLORREF bg = RGB(36, 114, 246);
        if (disabled) {
            bg = context->theme.dark ? RGB(45, 65, 95) : RGB(160, 196, 250);
        } else if (pressed) {
            bg = RGB(20, 87, 201);
        } else if (context->startHovered) {
            bg = RGB(27, 102, 229);
        }

        DrawRoundedSurface(item->hDC, item->rcItem, bg, bg, DpiScale(context->window, 10));

        wchar_t text[128]{};
        GetWindowTextW(item->hwndItem, text, static_cast<int>(sizeof(text) / sizeof(text[0])));

        SetBkMode(item->hDC, TRANSPARENT);
        SetTextColor(item->hDC, disabled ? (context->theme.dark ? RGB(120, 140, 170) : RGB(220, 230, 245)) : RGB(255, 255, 255));
        HGDIOBJ oldFont = SelectObject(item->hDC, context->actionFont);

        RECT textR = item->rcItem;
        if (pressed) {
            OffsetRect(&textR, 0, DpiScale(context->window, 1));
        }
        DrawTextW(item->hDC, text, -1, &textR,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(item->hDC, oldFont);

        if ((item->itemState & ODS_FOCUS) != 0) {
            RECT focus = item->rcItem;
            InflateRect(&focus, -DpiScale(context->window, 3), -DpiScale(context->window, 3));
            DrawFocusRect(item->hDC, &focus);
        }
        return;
    }

    if (item->CtlID == IDC_HOTKEY_CAPTURE) {
        FillRect(item->hDC, &item->rcItem, context->cardBrush);

        COLORREF bg = pressed ? context->theme.changeBtnPressed : (context->captureHovered ? context->theme.changeBtnHover : context->theme.changeBtnBg);
        COLORREF fg = context->theme.changeBtnText;
        DrawRoundedSurface(item->hDC, item->rcItem, bg, bg, DpiScale(context->window, 6));

        SetBkMode(item->hDC, TRANSPARENT);
        SetTextColor(item->hDC, fg);
        HGDIOBJ oldFont = SelectObject(item->hDC, context->btnChangeFont);

        if (context->captureActive) {
            DrawTextW(item->hDC, L"取消", -1, const_cast<RECT*>(&item->rcItem),
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        } else {
            const int textW = DpiScale(context->window, 28);
            const int iconW = DpiScale(context->window, 12);
            const int gap = DpiScale(context->window, 4);
            const int totalW = iconW + gap + textW;
            const int startX = (item->rcItem.left + item->rcItem.right - totalW) / 2;
            const int startY = (item->rcItem.top + item->rcItem.bottom) / 2;

            DrawPencilIcon(item->hDC, startX, startY - DpiScale(context->window, 5), fg, GetDpiForWindow(context->window));

            RECT textR = item->rcItem;
            textR.left = startX + iconW + gap;
            DrawTextW(item->hDC, L"更改", -1, &textR,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        SelectObject(item->hDC, oldFont);
        return;
    }

    if (item->CtlID == IDC_THEME_LIGHT || item->CtlID == IDC_THEME_DARK ||
        item->CtlID == IDC_THEME_SYSTEM) {
        RECT client{};
        GetClientRect(context->window, &client);
        const bool compactTheme = client.right < DpiScale(context->window, 540);
        const int segW1 = DpiScale(context->window, compactTheme ? 36 : 42);
        const int segW2 = DpiScale(context->window, compactTheme ? 36 : 42);
        const int segW3 = DpiScale(context->window, compactTheme ? 38 : 64);
        const int totalThemeW = segW1 + segW2 + segW3;
        const int h = item->rcItem.bottom - item->rcItem.top;

        // 1. Fill background with window background first (covers outside of rounded corners)
        FillRect(item->hDC, &item->rcItem, context->backgroundBrush);

        // 2. Draw continuous track pill
        RECT trackR;
        if (item->CtlID == IDC_THEME_LIGHT) {
            trackR = RECT{0, 0, totalThemeW, h};
        } else if (item->CtlID == IDC_THEME_DARK) {
            trackR = RECT{-segW1, 0, totalThemeW - segW1, h};
        } else {
            trackR = RECT{-(segW1 + segW2), 0, totalThemeW - (segW1 + segW2), h};
        }
        DrawRoundedSurface(item->hDC, trackR, context->theme.themeTrackBg,
                           context->theme.themeTrackBorder, h);

        // 3. Check selection and hover state
        const bool isSelected =
            (item->CtlID == IDC_THEME_LIGHT && context->settings.theme == ThemeMode::Light) ||
            (item->CtlID == IDC_THEME_DARK && context->settings.theme == ThemeMode::Dark) ||
            (item->CtlID == IDC_THEME_SYSTEM && context->settings.theme == ThemeMode::System);
        const bool isHovered = (context->hoveredThemeBtn == static_cast<int>(item->CtlID));

        const int inset = DpiScale(context->window, 2);
        RECT pillR = item->rcItem;
        InflateRect(&pillR, -inset, -inset);
        const int pillRadius = pillR.bottom - pillR.top;

        COLORREF textColor = context->theme.themeInactiveText;

        if (isSelected) {
            DrawRoundedSurface(item->hDC, pillR, context->theme.themeActiveBg,
                               context->theme.themeActiveBorder, pillRadius);
            textColor = context->theme.themeActiveText;
        } else if (isHovered && !disabled) {
            DrawRoundedSurface(item->hDC, pillR, context->theme.themeHoverBg,
                               context->theme.themeHoverBg, pillRadius);
            textColor = context->theme.themeHoverText;
        }

        // 4. Draw button text
        wchar_t text[32]{};
        GetWindowTextW(item->hwndItem, text, static_cast<int>(sizeof(text) / sizeof(text[0])));

        SetBkMode(item->hDC, TRANSPARENT);
        SetTextColor(item->hDC, textColor);
        HGDIOBJ oldFont = SelectObject(item->hDC, context->badgeFont);

        DrawTextW(item->hDC, text, -1, const_cast<RECT*>(&item->rcItem),
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(item->hDC, oldFont);

        if ((item->itemState & ODS_FOCUS) != 0) {
            RECT focus = pillR;
            InflateRect(&focus, -DpiScale(context->window, 2), -DpiScale(context->window, 2));
            DrawFocusRect(item->hDC, &focus);
        }
        return;
    }
}

void ApplyDarkTitleBar(HWND window, bool dark) {
    if (window == nullptr) {
        return;
    }
    const BOOL enabled = dark ? TRUE : FALSE;
    constexpr DWORD useImmersiveDarkMode = 20;
    constexpr DWORD useImmersiveDarkModeLegacy = 19;
    if (DwmSetWindowAttribute(window, useImmersiveDarkMode, &enabled, sizeof(enabled)) != S_OK) {
        DwmSetWindowAttribute(window, useImmersiveDarkModeLegacy, &enabled, sizeof(enabled));
    }

    constexpr DWORD windowCornerPreference = 33;
    constexpr DWORD systemBackdropType = 38;
    const int roundedCorners = 2;
    const int micaBackdrop = 2;
    DwmSetWindowAttribute(window, windowCornerPreference, &roundedCorners, sizeof(roundedCorners));
    DwmSetWindowAttribute(window, systemBackdropType, &micaBackdrop, sizeof(micaBackdrop));

    MARGINS margins{0, 0, 1, 0};
    DwmExtendFrameIntoClientArea(window, &margins);
}

void ApplyControlTheme(AppContext* context) {
    if (context == nullptr) {
        return;
    }

    const wchar_t* themeName = context->theme.dark ? L"DarkMode_Explorer" : L"Explorer";
    const HWND controls[] = {context->buttonSelect, context->clickTypeSelect,
                             context->interval, context->intervalSpin, context->hotkey,
                             context->hotkeyCapture, context->themeLight, context->themeDark,
                             context->themeSystem, context->startPause};
    for (HWND control : controls) {
        if (control != nullptr) {
            SetWindowTheme(control, context->theme.highContrast ? nullptr : themeName, nullptr);
            InvalidateRect(control, nullptr, TRUE);
        }
    }
    CreateUiResources(context);
    ApplyControlFonts(context);
    RedrawWindow(context->window, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void SetInitialSettings(AppContext* context) {
    if (context == nullptr) {
        return;
    }
    if (!SelectComboItemData(context->buttonSelect,
                             static_cast<std::uint32_t>(context->settings.button))) {
        SetError(context, L"无法恢复鼠标按键选择。");
    }
    if (!SelectComboItemData(context->clickTypeSelect,
                             static_cast<std::uint32_t>(context->settings.clickType))) {
        SetError(context, L"无法恢复点击类型选择。");
    }
    SetDlgItemInt(context->window, IDC_INTERVAL,
                  static_cast<UINT>(context->settings.intervalMilliseconds), FALSE);
    UpdateHotkeyText(context->hotkey, context->settings.hotkey);
    CheckRadioButton(context->window, IDC_THEME_LIGHT, IDC_THEME_SYSTEM,
                     ThemeControlId(context->settings.theme));
}

bool ReadSettingsFromControls(AppContext* context, Settings* settings) {
    if (context == nullptr || settings == nullptr) {
        return false;
    }
    Settings next = context->settings;
    std::uint32_t value = 0;
    if (!ReadComboItemData(context->buttonSelect, &value) ||
        !IsValidClickButton(static_cast<ClickButton>(value))) {
        SetError(context, L"请选择左键、中键或右键。");
        SetFocus(context->buttonSelect);
        return false;
    }
    next.button = static_cast<ClickButton>(value);
    if (!ReadComboItemData(context->clickTypeSelect, &value) ||
        !IsValidClickType(static_cast<ClickType>(value))) {
        SetError(context, L"请选择单次点击或双击。");
        SetFocus(context->clickTypeSelect);
        return false;
    }
    next.clickType = static_cast<ClickType>(value);

    BOOL translated = FALSE;
    const UINT interval = GetDlgItemInt(context->window, IDC_INTERVAL, &translated, FALSE);
    if (!translated) {
        SetError(context, L"间隔必须是 10 至 1000 毫秒之间的整数。");
        SetFocus(context->interval);
        return false;
    }
    next.intervalMilliseconds = interval;
    ProductMessage validationError;
    if (!ValidateSettings(next, &validationError)) {
        SetError(context, validationError.view());
        SetFocus(context->interval);
        return false;
    }
    *settings = next;
    return true;
}

void SaveCurrentSettings(AppContext* context) {
    Settings settings;
    if (!ReadSettingsFromControls(context, &settings)) {
        return;
    }
    context->settings = settings;
    ProductMessage saveError;
    if (!SaveSettings(context->settings, &saveError)) {
        SetError(context, saveError.view());
    }
}

void ToggleRunning(AppContext* context) {
    if (context == nullptr || context->captureActive ||
        context->state == AppState::Starting || context->state == AppState::Stopping) {
        return;
    }
    if (context->state == AppState::Running) {
        SetState(context, AppState::Stopping);
        context->worker.Stop();
        context->worker.Reap();
        SetState(context, AppState::Paused);
        return;
    }

    Settings settings;
    if (!ReadSettingsFromControls(context, &settings)) {
        SetState(context, AppState::Error, context->lastError.view());
        return;
    }
    context->settings = settings;
    ProductMessage saveError;
    if (!SaveSettings(settings, &saveError)) {
        SetError(context, saveError.view());
    } else {
        SetError(context, WideTextView{});
    }

    WorkerConfig workerConfig{settings.button, settings.clickType,
                              settings.intervalMilliseconds};
    SetState(context, AppState::Starting);
    if (!context->worker.Start(context->window, WM_APP_WORKER_EVENT, workerConfig)) {
        SetState(context, AppState::Error, L"无法启动点击线程。");
    }
}

void StartHotkeyCapture(AppContext* context) {
    if (context == nullptr || context->captureActive ||
        context->state == AppState::Running || context->state == AppState::Starting ||
        context->state == AppState::Stopping) {
        return;
    }
    context->captureActive = true;
    context->hadHotkeyBeforeCapture = context->hotkeys.IsRegistered();
    context->previousHotkey = context->settings.hotkey;
    context->hotkeys.Unregister();
    SetWindowTextW(context->hotkey, L"请按下快捷键...");
    InvalidateRect(context->hotkeyCapture, nullptr, TRUE);
    SetControlsEnabled(context, true);
    SetFocus(context->hotkey);
}

void RestorePreviousHotkey(AppContext* context) {
    if (context == nullptr) {
        return;
    }
    DWORD ignored = ERROR_SUCCESS;
    if (context->hadHotkeyBeforeCapture) {
        context->hotkeys.Register(context->window, context->previousHotkey, &ignored);
    }
    context->settings.hotkey = context->previousHotkey;
    UpdateHotkeyText(context->hotkey, context->settings.hotkey);
}

void FinishHotkeyCapture(AppContext* context, bool cancelled) {
    if (context == nullptr || !context->captureActive) {
        return;
    }
    context->captureActive = false;
    InvalidateRect(context->hotkeyCapture, nullptr, TRUE);

    if (cancelled) {
        RestorePreviousHotkey(context);
        SetError(context, WideTextView{});
        UpdateStatePresentation(context);
        return;
    }

    DWORD registrationError = ERROR_SUCCESS;
    if (!context->hotkeys.Register(context->window, context->pendingHotkey, &registrationError)) {
        RestorePreviousHotkey(context);
        SetError(context, L"快捷键已被系统或其他程序占用。");
        UpdateStatePresentation(context);
        return;
    }

    context->settings.hotkey = context->pendingHotkey;
    UpdateHotkeyText(context->hotkey, context->settings.hotkey);
    ProductMessage saveError;
    if (!SaveSettings(context->settings, &saveError)) {
        SetError(context, saveError.view());
    } else {
        SetError(context, WideTextView{});
    }
    UpdateStatePresentation(context);
}

void OnHotkeyCandidate(AppContext* context) {
    if (context == nullptr) {
        return;
    }
    FinishHotkeyCapture(context, false);
}

LRESULT CALLBACK HotkeyEditProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    AppContext* context = reinterpret_cast<AppContext*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_GETDLGCODE) {
        return DLGC_WANTALLKEYS;
    }
    if (message == WM_LBUTTONUP && context != nullptr && !context->captureActive) {
        StartHotkeyCapture(context);
        return 0;
    }
    if (context != nullptr && context->captureActive &&
        (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)) {
        const UINT virtualKey = static_cast<UINT>(wParam);
        if (virtualKey == VK_ESCAPE) {
            PostMessageW(context->window, WM_APP_HOTKEY_CANCELLED, 0, 0);
            return 0;
        }
        if (virtualKey == VK_SHIFT || virtualKey == VK_CONTROL || virtualKey == VK_MENU) {
            SetError(context, L"快捷键必须包含一个非修饰键。请继续按下普通键。");
            return 0;
        }
        if (virtualKey == VK_LWIN || virtualKey == VK_RWIN) {
            SetError(context, L"快捷键不能使用 Windows 键。");
            return 0;
        }

        Hotkey candidate{};
        candidate.modifiers = 0;
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            candidate.modifiers |= MOD_CONTROL;
        }
        if ((GetKeyState(VK_MENU) & 0x8000) != 0) {
            candidate.modifiers |= MOD_ALT;
        }
        if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
            candidate.modifiers |= MOD_SHIFT;
        }
        candidate.virtualKey = virtualKey;
        ProductMessage validationError;
        if (!IsValidHotkey(candidate, &validationError)) {
            SetError(context, validationError.view());
            return 0;
        }
        context->pendingHotkey = candidate;
        UpdateHotkeyText(context->hotkey, candidate);
        PostMessageW(context->window, WM_APP_HOTKEY_CAPTURED, 0, 0);
        return 0;
    }
    if (context != nullptr && context->captureActive &&
        (message == WM_KEYUP || message == WM_SYSKEYUP)) {
        return 0;
    }
    return CallWindowProcW(context != nullptr && context->hotkeyEditProc != nullptr
                               ? context->hotkeyEditProc
                               : DefWindowProcW,
                           window, message, wParam, lParam);
}

LRESULT CALLBACK ButtonHoverProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_ERASEBKGND) {
        return 1;
    }
    AppContext* context = reinterpret_cast<AppContext*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    const int id = GetDlgCtrlID(window);
    if (context != nullptr) {
        if (id == IDC_THEME_LIGHT || id == IDC_THEME_DARK || id == IDC_THEME_SYSTEM) {
            if (message == WM_MOUSEMOVE) {
                if (context->hoveredThemeBtn != id) {
                    context->hoveredThemeBtn = id;
                    InvalidateRect(window, nullptr, TRUE);
                    TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, window, 0};
                    TrackMouseEvent(&tme);
                }
            } else if (message == WM_MOUSELEAVE) {
                if (context->hoveredThemeBtn == id) {
                    context->hoveredThemeBtn = 0;
                    InvalidateRect(window, nullptr, TRUE);
                }
            }
        } else {
            bool* hovered = (id == IDC_START_PAUSE) ? &context->startHovered : &context->captureHovered;
            if (message == WM_MOUSEMOVE) {
                if (!*hovered) {
                    *hovered = true;
                    InvalidateRect(window, nullptr, TRUE);
                    TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, window, 0};
                    TrackMouseEvent(&tme);
                }
            } else if (message == WM_MOUSELEAVE) {
                *hovered = false;
                InvalidateRect(window, nullptr, TRUE);
            }
        }
    }
    WNDPROC prevProc = nullptr;
    if (context != nullptr) {
        if (id == IDC_START_PAUSE) {
            prevProc = context->startPauseProc;
        } else if (id == IDC_HOTKEY_CAPTURE) {
            prevProc = context->captureProc;
        } else if (id == IDC_THEME_LIGHT || id == IDC_THEME_DARK || id == IDC_THEME_SYSTEM) {
            prevProc = context->themeBtnProc;
        }
    }
    return CallWindowProcW(prevProc != nullptr ? prevProc : DefWindowProcW,
                           window, message, wParam, lParam);
}

void ApplyLayoutAndTheme(AppContext* context) {
    if (context == nullptr) {
        return;
    }
    context->theme = DetectTheme(context->settings.theme);
    ApplyControlTheme(context);
    LayoutControls(context);
    ApplyDarkTitleBar(context->window, context->theme.dark);
    InvalidateRect(context->window, nullptr, TRUE);
}

LRESULT CALLBACK MainWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    AppContext* context = reinterpret_cast<AppContext*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        context = static_cast<AppContext*>(create->lpCreateParams);
        context->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));
    }

    switch (message) {
    case WM_NCCALCSIZE:
        return 0; // Seamless client frame: entire window is client area

    case WM_NCHITTEST: {
        POINT pt{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
        ScreenToClient(window, &pt);
        RECT client{};
        GetClientRect(window, &client);

        const int btnW = MulDiv(40, static_cast<int>(GetDpiForWindow(window)), 96);
        const int btnH = MulDiv(30, static_cast<int>(GetDpiForWindow(window)), 96);
        if (pt.y < btnH && pt.x >= client.right - 2 * btnW) {
            return HTCLIENT;
        }

        const int themeH = DpiScale(window, 28);
        const int themeY = DpiScale(window, 14);
        const int themeGap = DpiScale(window, 12);
        const int themeRight = client.right - 2 * btnW - themeGap;
        const bool compactTheme = client.right < DpiScale(window, 540);
        const int segW1 = DpiScale(window, compactTheme ? 36 : 42);
        const int segW2 = DpiScale(window, compactTheme ? 36 : 42);
        const int segW3 = DpiScale(window, compactTheme ? 38 : 64);
        const int themeLeft = themeRight - (segW1 + segW2 + segW3);

        if (pt.x >= themeLeft && pt.x < themeRight && pt.y >= themeY && pt.y < themeY + themeH) {
            return HTCLIENT;
        }

        if (pt.y < DpiScale(window, 56)) {
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_NCLBUTTONDBLCLK:
        return 0;

    case WM_MOUSEMOVE: {
        if (context != nullptr) {
            POINT pt{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
            RECT client{};
            GetClientRect(window, &client);
            const int btnW = MulDiv(40, static_cast<int>(GetDpiForWindow(window)), 96);
            const int btnH = MulDiv(30, static_cast<int>(GetDpiForWindow(window)), 96);

            int hovered = 0;
            if (pt.y < btnH) {
                if (pt.x >= client.right - btnW && pt.x < client.right) {
                    hovered = 2; // Close
                } else if (pt.x >= client.right - 2 * btnW && pt.x < client.right - btnW) {
                    hovered = 1; // Min
                }
            }

            if (hovered != context->hoveredCaptionBtn) {
                context->hoveredCaptionBtn = hovered;
                RECT btnArea{client.right - 2 * btnW, 0, client.right, btnH};
                InvalidateRect(window, &btnArea, FALSE);
            }

            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, window, 0};
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        if (context != nullptr && context->hoveredCaptionBtn != 0) {
            context->hoveredCaptionBtn = 0;
            RECT client{};
            GetClientRect(window, &client);
            const int btnW = MulDiv(40, static_cast<int>(GetDpiForWindow(window)), 96);
            const int btnH = MulDiv(30, static_cast<int>(GetDpiForWindow(window)), 96);
            RECT btnArea{client.right - 2 * btnW, 0, client.right, btnH};
            InvalidateRect(window, &btnArea, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (context != nullptr && context->hoveredCaptionBtn != 0) {
            context->pressedCaptionBtn = context->hoveredCaptionBtn;
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (context != nullptr && context->pressedCaptionBtn != 0) {
            const int pressed = context->pressedCaptionBtn;
            context->pressedCaptionBtn = 0;
            if (pressed == context->hoveredCaptionBtn) {
                if (pressed == 1) {
                    ShowWindow(window, SW_MINIMIZE);
                } else if (pressed == 2) {
                    PostMessageW(window, WM_CLOSE, 0, 0);
                }
            }
            return 0;
        }
        break;
    }

    case WM_CREATE:
        context->theme = DetectTheme(context->settings.theme);
        CreateUiResources(context);
        CreateControls(context);
        SetInitialSettings(context);
        ApplyLayoutAndTheme(context);
        {
            HMENU sysMenu = GetSystemMenu(window, FALSE);
            if (sysMenu != nullptr) {
                DeleteMenu(sysMenu, SC_SIZE, MF_BYCOMMAND);
                DeleteMenu(sysMenu, SC_MAXIMIZE, MF_BYCOMMAND);
                DeleteMenu(sysMenu, SC_RESTORE, MF_BYCOMMAND);
            }
        }
        {
            DWORD hotkeyError = ERROR_SUCCESS;
            if (!context->hotkeys.Register(window, context->settings.hotkey, &hotkeyError)) {
                SetError(context, L"默认快捷键无法注册，请录制其他组合。");
            }
        }
        if (!context->lastError.empty()) {
            SetError(context, context->lastError.view());
        }
        UpdateStatePresentation(context);
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_SIZE || (wParam & 0xFFF0) == SC_MAXIMIZE) {
            return 0;
        }
        break;

    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
        if (limits != nullptr) {
            UINT dpi = GetDpiForWindow(window);
            if (dpi == 0) {
                dpi = GetDpiForSystem();
            }
            const WindowPixelSize fixedPix = LogicalClientSizeToPixels(
                WindowClientSize{static_cast<int>(kDefaultWindowClientWidth),
                                 static_cast<int>(kDefaultWindowClientHeight)}, dpi);
            limits->ptMinTrackSize.x = fixedPix.width;
            limits->ptMinTrackSize.y = fixedPix.height;
            limits->ptMaxTrackSize.x = fixedPix.width;
            limits->ptMaxTrackSize.y = fixedPix.height;
        }
        return 0;
    }

    case WM_SIZE:
        UpdateWindowClientSize(context);
        LayoutControls(context);
        InvalidateRect(window, nullptr, FALSE);
        return 0;

    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested != nullptr) {
            UINT dpi = LOWORD(wParam);
            if (dpi == 0) {
                dpi = GetDpiForWindow(window);
            }
            const WindowPixelSize fixedPix = LogicalClientSizeToPixels(
                WindowClientSize{static_cast<int>(kDefaultWindowClientWidth),
                                 static_cast<int>(kDefaultWindowClientHeight)}, dpi);
            SetWindowPos(window, nullptr, suggested->left, suggested->top, fixedPix.width, fixedPix.height,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        ApplyLayoutAndTheme(context);
        return 0;
    }

    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
    case WM_SYSCOLORCHANGE:
        ApplyLayoutAndTheme(context);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_START_PAUSE:
            if (HIWORD(wParam) == BN_CLICKED) {
                ToggleRunning(context);
            }
            return 0;
        case IDC_HOTKEY_CAPTURE:
            if (HIWORD(wParam) == BN_CLICKED) {
                if (context->captureActive) {
                    FinishHotkeyCapture(context, true);
                } else {
                    StartHotkeyCapture(context);
                }
            }
            return 0;
        case IDC_MOUSE_BUTTON_SELECT:
        case IDC_CLICK_TYPE_SELECT:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                ClearSettingsError(context);
            }
            return 0;
        case IDC_INTERVAL:
            if (HIWORD(wParam) == EN_CHANGE &&
                (context->state == AppState::Paused || context->state == AppState::Error)) {
                ClearSettingsError(context);
            }
            return 0;
        case IDC_THEME_LIGHT:
        case IDC_THEME_DARK:
        case IDC_THEME_SYSTEM:
            if (HIWORD(wParam) == BN_CLICKED) {
                context->settings.theme = ThemeModeFromControlId(LOWORD(wParam));
                CheckRadioButton(context->window, IDC_THEME_LIGHT, IDC_THEME_SYSTEM,
                                 LOWORD(wParam));
                ApplyLayoutAndTheme(context);
                ProductMessage saveError;
                if (!SaveSettings(context->settings, &saveError)) {
                    SetError(context, saveError.view());
                }
            }
            return 0;
        default:
            return 0;
        }

    case WM_HOTKEY:
        if (wParam == kHotkeyCommandId) {
            ToggleRunning(context);
        }
        return 0;

    case WM_APP_WORKER_EVENT: {
        const auto event = static_cast<WorkerEventKind>(wParam);
        const DWORD error = static_cast<DWORD>(lParam);
        if (event == WorkerEventKind::Running) {
            SetState(context, AppState::Running);
        } else if (event == WorkerEventKind::InputError) {
            const wchar_t* errorMessage = error == ERROR_ACCESS_DENIED
                                              ? L"输入被目标窗口拒绝，连点已停止。"
                                              : L"鼠标输入注入失败，连点已停止。";
            SetState(context, AppState::Error, errorMessage);
        } else if (event == WorkerEventKind::Stopped) {
            context->worker.Reap();
            if (context->state == AppState::Stopping) {
                SetState(context, AppState::Paused);
            } else if (context->state != AppState::Error) {
                SetState(context, AppState::Paused);
            }
        }
        return 0;
    }

    case WM_APP_HOTKEY_CAPTURED:
        OnHotkeyCandidate(context);
        return 0;

    case WM_APP_HOTKEY_CANCELLED:
        FinishHotkeyCapture(context, true);
        return 0;

    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const HWND control = reinterpret_cast<HWND>(lParam);
        if (control == context->hotkey) {
            SetBkColor(dc, context->theme.hotkeyBoxBackground);
            SetTextColor(dc, context->captureActive ? context->theme.accent : context->theme.text);
            return reinterpret_cast<LRESULT>(context->hotkeyBoxBrush);
        }
        SetBkColor(dc, context->theme.editBackground);
        SetTextColor(dc, context->theme.text);
        return reinterpret_cast<LRESULT>(context->editBrush);
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, context->theme.text);
        return reinterpret_cast<LRESULT>(context->cardBrush);
    }

    case WM_CTLCOLORBTN: {
        const HWND control = reinterpret_cast<HWND>(lParam);
        if (control == context->hotkeyCapture) {
            return reinterpret_cast<LRESULT>(context->cardBrush);
        }
        return reinterpret_cast<LRESULT>(context->backgroundBrush);
    }

    case WM_DRAWITEM:
        DrawOwnerDrawButton(context, reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC hdc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        PaintWindowSurface(context, memDC);

        BitBlt(hdc, 0, 0, client.right, client.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        EndPaint(window, &paint);
        return 0;
    }

    case WM_CLOSE:
        if (!context->closing) {
            context->closing = true;
            context->worker.Stop();
            context->worker.Reap();
            context->hotkeys.Unregister();
            SaveCurrentSettings(context);
            DestroyWindow(window);
        }
        return 0;

    case WM_DESTROY:
        DestroyUiResources(context);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int RunApplication(HINSTANCE instance, int showCommand) {
    ComApartment comApartment;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&commonControls);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = &MainWindowProc;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_MOUSECLICK));
    windowClass.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_MOUSECLICK));
    if (windowClass.hIcon == nullptr) {
        windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    if (windowClass.hIconSm == nullptr) {
        windowClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    }
    windowClass.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    if (RegisterClassExW(&windowClass) == 0) {
        return 1;
    }

    AppContext context;
    ProductMessage loadWarning;
    LoadSettings(&context.settings, &loadWarning);
    if (!loadWarning.empty()) {
        context.lastError.Assign(loadWarning.view());
    }

    const UINT initialDpi = GetDpiForSystem();
    const WindowClientSize defaultClientSize{static_cast<int>(kDefaultWindowClientWidth),
                                             static_cast<int>(kDefaultWindowClientHeight)};
    context.settings.clientWidth = kDefaultWindowClientWidth;
    context.settings.clientHeight = kDefaultWindowClientHeight;

    const WindowPixelSize initialPixelSize = LogicalClientSizeToPixels(defaultClientSize, initialDpi);
    HWND window = CreateWindowExW(0, kWindowClassName, kWindowTitle, kMainWindowStyle,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  initialPixelSize.width, initialPixelSize.height,
                                  nullptr, nullptr, instance, &context);
    if (window == nullptr) {
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(window, &message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
