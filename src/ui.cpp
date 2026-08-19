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
    COLORREF windowBackground = RGB(249, 249, 249);
    COLORREF sidebarBackground = RGB(243, 243, 243);
    COLORREF cardBackground = RGB(255, 255, 255);
    COLORREF controlBackground = RGB(248, 249, 250);
    COLORREF text = RGB(26, 28, 28);
    COLORREF mutedText = RGB(64, 71, 82);
    COLORREF subtleText = RGB(95, 94, 94);
    COLORREF editBackground = RGB(255, 255, 255);
    COLORREF border = RGB(216, 220, 227);
    COLORREF accent = RGB(0, 95, 170);
    COLORREF accentPressed = RGB(0, 72, 131);
    COLORREF accentText = RGB(255, 255, 255);
    COLORREF error = RGB(186, 26, 26);
    COLORREF success = RGB(16, 124, 16);
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
        theme.sidebarBackground = GetSysColor(COLOR_WINDOW);
        theme.cardBackground = GetSysColor(COLOR_WINDOW);
        theme.controlBackground = GetSysColor(COLOR_BTNFACE);
        theme.text = GetSysColor(COLOR_WINDOWTEXT);
        theme.mutedText = GetSysColor(COLOR_WINDOWTEXT);
        theme.subtleText = GetSysColor(COLOR_WINDOWTEXT);
        theme.editBackground = GetSysColor(COLOR_WINDOW);
        theme.border = GetSysColor(COLOR_WINDOWTEXT);
        theme.accent = GetSysColor(COLOR_HIGHLIGHT);
        theme.accentPressed = GetSysColor(COLOR_HIGHLIGHT);
        theme.accentText = GetSysColor(COLOR_HIGHLIGHTTEXT);
        theme.error = GetSysColor(COLOR_WINDOWTEXT);
        theme.success = GetSysColor(COLOR_WINDOWTEXT);
        return theme;
    }
    theme.dark = mode == ThemeMode::Dark ||
                 (mode == ThemeMode::System && IsDarkModeEnabled());
    if (theme.dark) {
        theme.windowBackground = RGB(32, 32, 32);
        theme.sidebarBackground = RGB(28, 28, 28);
        theme.cardBackground = RGB(43, 43, 43);
        theme.controlBackground = RGB(50, 50, 50);
        theme.text = RGB(243, 243, 243);
        theme.mutedText = RGB(200, 200, 200);
        theme.subtleText = RGB(174, 174, 174);
        theme.editBackground = RGB(43, 43, 43);
        theme.border = RGB(72, 72, 72);
        theme.accent = RGB(0, 120, 212);
        theme.accentPressed = RGB(0, 90, 158);
        theme.error = RGB(255, 180, 171);
        theme.success = RGB(108, 203, 95);
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
    Settings settings = DefaultSettings();
    HotkeyManager hotkeys;
    ClickWorker worker;
    AppState state = AppState::Paused;
    bool closing = false;
    bool captureActive = false;
    bool hadHotkeyBeforeCapture = false;
    bool sizeMoveActive = false;
    bool sizeChangedDuringMove = false;
    ThemeColors theme;
    HBRUSH backgroundBrush = nullptr;
    HBRUSH cardBrush = nullptr;
    HBRUSH editBrush = nullptr;
    HFONT bodyFont = nullptr;
    HFONT smallFont = nullptr;
    HFONT labelFont = nullptr;
    HFONT headingFont = nullptr;
    HFONT brandFont = nullptr;
    HFONT actionFont = nullptr;
    Hotkey previousHotkey{};
    Hotkey pendingHotkey{};
    ProductMessage lastError;
};

LRESULT CALLBACK HotkeyEditProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

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
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
}

void DestroyUiResources(AppContext* context) {
    if (context == nullptr) {
        return;
    }
    DeleteObject(context->backgroundBrush);
    DeleteObject(context->cardBrush);
    DeleteObject(context->editBrush);
    DeleteObject(context->bodyFont);
    DeleteObject(context->smallFont);
    DeleteObject(context->labelFont);
    DeleteObject(context->headingFont);
    DeleteObject(context->brandFont);
    DeleteObject(context->actionFont);
    context->backgroundBrush = nullptr;
    context->cardBrush = nullptr;
    context->editBrush = nullptr;
    context->bodyFont = nullptr;
    context->smallFont = nullptr;
    context->labelFont = nullptr;
    context->headingFont = nullptr;
    context->brandFont = nullptr;
    context->actionFont = nullptr;
}

void CreateUiResources(AppContext* context) {
    if (context == nullptr || context->window == nullptr) {
        return;
    }
    DestroyUiResources(context);
    context->backgroundBrush = CreateSolidBrush(context->theme.windowBackground);
    context->cardBrush = CreateSolidBrush(context->theme.cardBackground);
    context->editBrush = CreateSolidBrush(context->theme.editBackground);
    context->bodyFont = CreateUiFont(context->window, 10, FW_NORMAL);
    context->smallFont = CreateUiFont(context->window, 8, FW_NORMAL);
    context->labelFont = CreateUiFont(context->window, 9, FW_SEMIBOLD);
    context->headingFont = CreateUiFont(context->window, 20, FW_SEMIBOLD);
    context->brandFont = CreateUiFont(context->window, 14, FW_SEMIBOLD);
    context->actionFont = CreateUiFont(context->window, 12, FW_SEMIBOLD);
}

void SetError(AppContext* context, WideTextView message) {
    if (context == nullptr) {
        return;
    }
    context->lastError.Assign(message);
    if (context->error != nullptr) {
        SetWindowTextW(context->error, context->lastError.c_str());
        ShowWindow(context->error, context->lastError.empty() ? SW_HIDE : SW_SHOW);
        InvalidateRect(context->error, nullptr, TRUE);
    }
}

void UpdateWindowClientSize(AppContext* context) {
    if (context == nullptr || context->window == nullptr) {
        return;
    }
    RECT client{};
    if (GetClientRect(context->window, &client) == FALSE || client.right <= 0 ||
        client.bottom <= 0) {
        return;
    }
    UINT dpi = GetDpiForWindow(context->window);
    if (dpi == 0) {
        dpi = GetDpiForSystem();
    }
    const WindowClientSize logical = PixelsToLogicalClientSize(
        WindowPixelSize{client.right, client.bottom}, dpi);
    const WindowClientSize clamped = ClampWindowClientSize(logical.width, logical.height);
    context->settings.clientWidth = static_cast<std::uint32_t>(clamped.width);
    context->settings.clientHeight = static_cast<std::uint32_t>(clamped.height);
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
        SetWindowTextW(control, text.c_str());
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
        const wchar_t* status = L"●  已暂停";
        if (context->state == AppState::Starting) {
            status = L"●  启动中";
        } else if (context->state == AppState::Running) {
            status = L"●  运行中";
        } else if (context->state == AppState::Stopping) {
            status = L"●  停止中";
        } else if (context->state == AppState::Error) {
            status = L"●  需要处理";
        }
        SetWindowTextW(context->status, status);
    }

    if (context->startPause != nullptr) {
        BoundedWideString<96> label;
        HotkeyText hotkey;
        FormatHotkey(context->settings.hotkey, &hotkey);
        if (context->state == AppState::Running) {
            label.Assign(L"Ⅱ  暂停（");
            label.Append(hotkey.view());
            label.Append(L"）");
        } else {
            label.Assign(L"▶  开始（");
            label.Append(hotkey.view());
            label.Append(L"）");
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
    const int bodyIds[] = {IDC_NAV_MAIN, IDC_PAGE_SUBTITLE, IDC_INTERVAL,
                           IDC_MOUSE_BUTTON_SELECT, IDC_CLICK_TYPE_SELECT,
                           IDC_STATUS, IDC_ERROR, IDC_HOTKEY, IDC_HOTKEY_CAPTURE,
                           IDC_THEME_LIGHT, IDC_THEME_DARK, IDC_THEME_SYSTEM};
    for (int id : bodyIds) {
        SetFont(GetDlgItem(context->window, id), context->bodyFont);
    }
    const int smallIds[] = {IDC_VERSION, IDC_INTERVAL_HELP,
                            IDC_STATUS_HELP, IDC_HOTKEY_HELP};
    for (int id : smallIds) {
        SetFont(GetDlgItem(context->window, id), context->smallFont);
    }
    const int labelIds[] = {IDC_CLICK_OPTIONS_LABEL, IDC_LABEL_MOUSE_BUTTON,
                            IDC_LABEL_CLICK_TYPE, IDC_LABEL_INTERVAL,
                            IDC_LABEL_HOTKEY, IDC_STATUS_LABEL};
    for (int id : labelIds) {
        SetFont(GetDlgItem(context->window, id), context->labelFont);
    }
    SetFont(GetDlgItem(context->window, IDC_BRAND), context->brandFont);
    SetFont(GetDlgItem(context->window, IDC_PAGE_TITLE), context->headingFont);
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
    const DWORD labelStyle = WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX;
    const DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
    const DWORD buttonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    const DWORD comboStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                             CBS_DROPDOWNLIST | CBS_HASSTRINGS;

    CreateChild(context, WC_STATICW, L"MouseClick", labelStyle, WS_EX_TRANSPARENT, IDC_BRAND);
    CreateChild(context, WC_STATICW, L"v1.0.0", labelStyle, WS_EX_TRANSPARENT, IDC_VERSION);
    CreateChild(context, WC_STATICW, L"⚙  主要设置", labelStyle, WS_EX_TRANSPARENT, IDC_NAV_MAIN);
    CreateChild(context, WC_STATICW, L"自动点击设置", labelStyle, WS_EX_TRANSPARENT, IDC_PAGE_TITLE);
    CreateChild(context, WC_STATICW, L"配置点击间隔、鼠标按键和全局快捷键",
                labelStyle, WS_EX_TRANSPARENT, IDC_PAGE_SUBTITLE);
    context->themeLight = CreateChild(context, WC_BUTTONW, L"亮色",
                                      buttonStyle | BS_OWNERDRAW | WS_GROUP,
                                      0, IDC_THEME_LIGHT);
    context->themeDark = CreateChild(context, WC_BUTTONW, L"暗色",
                                     buttonStyle | BS_OWNERDRAW,
                                     0, IDC_THEME_DARK);
    context->themeSystem = CreateChild(context, WC_BUTTONW, L"跟随系统",
                                       buttonStyle | BS_OWNERDRAW,
                                       0, IDC_THEME_SYSTEM);

    CreateChild(context, WC_STATICW, L"点击间隔",
                labelStyle, WS_EX_TRANSPARENT, IDC_LABEL_INTERVAL);
    CreateChild(context, WC_STATICW, L"毫秒（10–1000）", labelStyle,
                WS_EX_TRANSPARENT, IDC_INTERVAL_HELP);
    context->interval = CreateChild(context, WC_EDITW, L"100", editStyle | ES_NUMBER,
                                    0, IDC_INTERVAL);
    context->intervalSpin = CreateChild(context, UPDOWN_CLASSW, nullptr,
                                        WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                                            UDS_ARROWKEYS,
                                        0, IDC_INTERVAL_SPIN);
    SendMessageW(context->intervalSpin, UDM_SETBUDDY,
                 reinterpret_cast<WPARAM>(context->interval), 0);
    SendMessageW(context->intervalSpin, UDM_SETRANGE32,
                 static_cast<WPARAM>(kMinimumIntervalMilliseconds),
                 static_cast<LPARAM>(kMaximumIntervalMilliseconds));

    CreateChild(context, WC_STATICW, L"点击选项", labelStyle,
                WS_EX_TRANSPARENT, IDC_CLICK_OPTIONS_LABEL);
    CreateChild(context, WC_STATICW, L"鼠标按键", labelStyle,
                WS_EX_TRANSPARENT, IDC_LABEL_MOUSE_BUTTON);
    context->buttonSelect = CreateChild(context, WC_COMBOBOXW, nullptr,
                                        comboStyle | WS_GROUP,
                                        0, IDC_MOUSE_BUTTON_SELECT);
    CreateChild(context, WC_STATICW, L"点击类型", labelStyle,
                WS_EX_TRANSPARENT, IDC_LABEL_CLICK_TYPE);
    context->clickTypeSelect = CreateChild(context, WC_COMBOBOXW, nullptr,
                                           comboStyle,
                                           0, IDC_CLICK_TYPE_SELECT);
    if (!AddComboItem(context->buttonSelect, L"左键",
                      static_cast<std::uint32_t>(ClickButton::Left)) ||
        !AddComboItem(context->buttonSelect, L"中键",
                      static_cast<std::uint32_t>(ClickButton::Middle)) ||
        !AddComboItem(context->buttonSelect, L"右键",
                      static_cast<std::uint32_t>(ClickButton::Right)) ||
        !AddComboItem(context->clickTypeSelect, L"单次点击",
                      static_cast<std::uint32_t>(ClickType::Single)) ||
        !AddComboItem(context->clickTypeSelect, L"双击",
                      static_cast<std::uint32_t>(ClickType::Double))) {
        SetError(context, L"无法初始化鼠标点击选项。");
    }
    SendMessageW(context->buttonSelect, CB_SETMINVISIBLE, 3, 0);
    SendMessageW(context->clickTypeSelect, CB_SETMINVISIBLE, 2, 0);
    SetAccessibleName(context->buttonSelect, L"鼠标按键");
    SetAccessibleName(context->clickTypeSelect, L"点击类型");

    CreateChild(context, WC_STATICW, L"运行状态", labelStyle,
                WS_EX_TRANSPARENT, IDC_STATUS_LABEL);
    context->status = CreateChild(context, WC_STATICW, L"●  已暂停", labelStyle,
                                  WS_EX_TRANSPARENT, IDC_STATUS);
    CreateChild(context, WC_STATICW, L"设置已就绪", labelStyle,
                WS_EX_TRANSPARENT, IDC_STATUS_HELP);
    context->error = CreateChild(context, WC_STATICW, L"", labelStyle,
                                 WS_EX_TRANSPARENT, IDC_ERROR);

    CreateChild(context, WC_STATICW, L"当前快捷键",
                labelStyle, WS_EX_TRANSPARENT, IDC_LABEL_HOTKEY);
    CreateChild(context, WC_STATICW, L"用于开始与暂停", labelStyle,
                WS_EX_TRANSPARENT, IDC_HOTKEY_HELP);
    context->hotkey = CreateChild(context, WC_EDITW, L"Ctrl+Alt+F6",
                                  editStyle | ES_READONLY, 0, IDC_HOTKEY);
    context->hotkeyCapture = CreateChild(context, WC_BUTTONW, L"更改",
                                         buttonStyle | BS_OWNERDRAW, 0, IDC_HOTKEY_CAPTURE);

    context->startPause = CreateChild(context, WC_BUTTONW, L"▶  开始",
                                      buttonStyle | BS_OWNERDRAW, 0, IDC_START_PAUSE);

    ApplyControlFonts(context);

    context->hotkeyEditProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        context->hotkey, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&HotkeyEditProc)));
    SetWindowLongPtrW(context->hotkey, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));
    ShowWindow(GetDlgItem(context->window, IDC_NAV_MAIN), SW_HIDE);
    ShowWindow(GetDlgItem(context->window, IDC_PAGE_TITLE), SW_HIDE);
    ShowWindow(GetDlgItem(context->window, IDC_PAGE_SUBTITLE), SW_HIDE);
    ShowWindow(context->error, SW_HIDE);
}

struct LayoutMetrics {
    RECT sidebar{};
    RECT navigation{};
    RECT intervalCard{};
    RECT optionCard{};
    RECT statusCard{};
    RECT hotkeyCard{};
    RECT actionButton{};
    int mainLeft = 0;
    int mainRight = 0;
    int headerDividerY = 0;
    int actionDividerY = 0;
};

RECT MakeRect(int x, int y, int width, int height) {
    return RECT{x, y, x + width, y + height};
}

LayoutMetrics CalculateLayout(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int margin = DpiScale(window, 16);
    const int gap = DpiScale(window, 10);
    const int mainLeft = margin;
    const int mainRight = client.right > margin ? client.right - margin : mainLeft;
    const int contentWidth = mainRight > mainLeft ? mainRight - mainLeft : 0;
    const int leftCardWidth = (contentWidth - gap) / 2;
    const int intervalTop = DpiScale(window, 66);
    const int intervalHeight = DpiScale(window, 78);
    const int detailTop = intervalTop + intervalHeight + gap;
    const int detailHeight = DpiScale(window, 142);
    const int hotkeyTop = detailTop + detailHeight + gap;
    const int hotkeyHeight = DpiScale(window, 64);
    const int actionHeight = DpiScale(window, 36);
    const int actionTop = client.bottom - DpiScale(window, 44);

    LayoutMetrics layout;
    layout.sidebar = MakeRect(0, 0, 0, client.bottom);
    layout.navigation = MakeRect(0, 0, 0, 0);
    layout.intervalCard = MakeRect(mainLeft, intervalTop, contentWidth, intervalHeight);
    layout.optionCard = MakeRect(mainLeft, detailTop, leftCardWidth, detailHeight);
    layout.statusCard = MakeRect(mainLeft + leftCardWidth + gap, detailTop,
                                 contentWidth - leftCardWidth - gap, detailHeight);
    layout.hotkeyCard = MakeRect(mainLeft, hotkeyTop, contentWidth, hotkeyHeight);
    layout.actionButton = MakeRect(mainLeft, actionTop, contentWidth, actionHeight);
    layout.mainLeft = mainLeft;
    layout.mainRight = mainRight;
    layout.headerDividerY = DpiScale(window, 56);
    layout.actionDividerY = client.bottom - DpiScale(window, 54);
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
    const int pad = DpiScale(context->window, 10);
    const int labelHeight = DpiScale(context->window, 16);
    const int helpHeight = DpiScale(context->window, 14);
    const int rowHeight = DpiScale(context->window, 32);

    MoveWindow(GetDlgItem(context->window, IDC_BRAND), layout.mainLeft,
               DpiScale(context->window, 14), DpiScale(context->window, 100),
               DpiScale(context->window, 24), TRUE);
    MoveWindow(GetDlgItem(context->window, IDC_VERSION),
               layout.mainLeft + DpiScale(context->window, 104),
               DpiScale(context->window, 22), DpiScale(context->window, 80), helpHeight, TRUE);
    const int themeGap = DpiScale(context->window, 4);
    const int themeSmallWidth = DpiScale(context->window, 44);
    const int themeSystemWidth = DpiScale(context->window, 76);
    const int themeHeight = DpiScale(context->window, 28);
    const int themeY = DpiScale(context->window, 12);
    MoveWindow(context->themeSystem, layout.mainRight - themeSystemWidth, themeY,
               themeSystemWidth, themeHeight, TRUE);
    MoveWindow(context->themeDark,
               layout.mainRight - themeSystemWidth - themeGap - themeSmallWidth, themeY,
               themeSmallWidth, themeHeight, TRUE);
    MoveWindow(context->themeLight,
               layout.mainRight - themeSystemWidth - 2 * themeGap - 2 * themeSmallWidth, themeY,
               themeSmallWidth, themeHeight, TRUE);

    MoveWindow(GetDlgItem(context->window, IDC_LABEL_INTERVAL), layout.intervalCard.left + pad,
               layout.intervalCard.top + DpiScale(context->window, 8),
               DpiScale(context->window, 140), labelHeight, TRUE);
    MoveWindow(GetDlgItem(context->window, IDC_INTERVAL_HELP), layout.intervalCard.left + pad,
               layout.intervalCard.top + DpiScale(context->window, 28),
               DpiScale(context->window, 180), helpHeight, TRUE);
    MoveWindow(context->interval, layout.intervalCard.left + pad,
               layout.intervalCard.top + DpiScale(context->window, 45),
               layout.intervalCard.right - layout.intervalCard.left - 2 * pad, rowHeight, TRUE);
    MoveWindow(context->intervalSpin, layout.intervalCard.right - pad - DpiScale(context->window, 18),
               layout.intervalCard.top + DpiScale(context->window, 45),
               DpiScale(context->window, 18), rowHeight, TRUE);

    MoveWindow(GetDlgItem(context->window, IDC_CLICK_OPTIONS_LABEL), layout.optionCard.left + pad,
               layout.optionCard.top + DpiScale(context->window, 8),
               DpiScale(context->window, 150), labelHeight, TRUE);
    MoveWindow(GetDlgItem(context->window, IDC_LABEL_MOUSE_BUTTON), layout.optionCard.left + pad,
               layout.optionCard.top + DpiScale(context->window, 30),
               DpiScale(context->window, 140), labelHeight, TRUE);
    MoveWindow(GetDlgItem(context->window, IDC_LABEL_CLICK_TYPE), layout.optionCard.left + pad,
               layout.optionCard.top + DpiScale(context->window, 84),
               DpiScale(context->window, 140), labelHeight, TRUE);
    const int comboWidth = layout.optionCard.right - layout.optionCard.left - 2 * pad;
    const int comboItemHeight = DpiScale(context->window, 22);
    const int comboExtraHeight = DpiScale(context->window, 6);
    SendMessageW(context->buttonSelect, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1),
                 rowHeight - comboExtraHeight);
    SendMessageW(context->buttonSelect, CB_SETITEMHEIGHT, 0, comboItemHeight);
    SendMessageW(context->clickTypeSelect, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1),
                 rowHeight - comboExtraHeight);
    SendMessageW(context->clickTypeSelect, CB_SETITEMHEIGHT, 0, comboItemHeight);
    MoveWindow(context->buttonSelect, layout.optionCard.left + pad,
               layout.optionCard.top + DpiScale(context->window, 46), comboWidth,
               rowHeight + 3 * comboItemHeight + comboExtraHeight, TRUE);
    MoveWindow(context->clickTypeSelect, layout.optionCard.left + pad,
               layout.optionCard.top + DpiScale(context->window, 100), comboWidth,
               rowHeight + 2 * comboItemHeight + comboExtraHeight, TRUE);

    MoveWindow(GetDlgItem(context->window, IDC_STATUS_LABEL), layout.statusCard.left + pad,
               layout.statusCard.top + DpiScale(context->window, 8),
               DpiScale(context->window, 150), labelHeight, TRUE);
    MoveWindow(context->status, layout.statusCard.left + pad,
               layout.statusCard.top + DpiScale(context->window, 30),
               layout.statusCard.right - layout.statusCard.left - 2 * pad,
               DpiScale(context->window, 20), TRUE);
    MoveWindow(GetDlgItem(context->window, IDC_STATUS_HELP), layout.statusCard.left + pad,
               layout.statusCard.top + DpiScale(context->window, 54),
               layout.statusCard.right - layout.statusCard.left - 2 * pad, helpHeight, TRUE);
    MoveWindow(context->error, layout.statusCard.left + pad,
               layout.statusCard.top + DpiScale(context->window, 72),
               layout.statusCard.right - layout.statusCard.left - 2 * pad,
               layout.statusCard.bottom - layout.statusCard.top - DpiScale(context->window, 80), TRUE);

    MoveWindow(GetDlgItem(context->window, IDC_LABEL_HOTKEY), layout.hotkeyCard.left + pad,
               layout.hotkeyCard.top + DpiScale(context->window, 8),
               DpiScale(context->window, 120), labelHeight, TRUE);
    MoveWindow(GetDlgItem(context->window, IDC_HOTKEY_HELP),
               layout.hotkeyCard.left + DpiScale(context->window, 118),
               layout.hotkeyCard.top + DpiScale(context->window, 10),
               DpiScale(context->window, 180), helpHeight, TRUE);
    const int captureWidth = DpiScale(context->window, 68);
    MoveWindow(context->hotkey, layout.hotkeyCard.left + pad,
               layout.hotkeyCard.top + DpiScale(context->window, 30),
               layout.hotkeyCard.right - layout.hotkeyCard.left - 3 * pad - captureWidth,
               DpiScale(context->window, 28), TRUE);
    MoveWindow(context->hotkeyCapture, layout.hotkeyCard.right - pad - captureWidth,
               layout.hotkeyCard.top + DpiScale(context->window, 30),
               captureWidth, DpiScale(context->window, 32), TRUE);

    MoveWindow(context->startPause, layout.actionButton.left, layout.actionButton.top,
               layout.actionButton.right - layout.actionButton.left,
               layout.actionButton.bottom - layout.actionButton.top, TRUE);
}

void DrawRoundedSurface(HDC dc, const RECT& rect, COLORREF fillColor,
                        COLORREF borderColor, int radius) {
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    const HGDIOBJ previousBrush = SelectObject(dc, brush);
    const HGDIOBJ previousPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, previousPen);
    SelectObject(dc, previousBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void PaintWindowSurface(AppContext* context, HDC dc) {
    RECT client{};
    GetClientRect(context->window, &client);
    HBRUSH background = CreateSolidBrush(context->theme.windowBackground);
    FillRect(dc, &client, background);
    DeleteObject(background);

    const LayoutMetrics layout = CalculateLayout(context->window);
    HPEN dividerPen = CreatePen(PS_SOLID, 1, context->theme.border);
    const HGDIOBJ previousPen = SelectObject(dc, dividerPen);
    MoveToEx(dc, layout.mainLeft, layout.headerDividerY, nullptr);
    LineTo(dc, layout.mainRight, layout.headerDividerY);
    MoveToEx(dc, layout.mainLeft, layout.actionDividerY, nullptr);
    LineTo(dc, layout.mainRight, layout.actionDividerY);
    SelectObject(dc, previousPen);
    DeleteObject(dividerPen);

    const int radius = DpiScale(context->window, 8);
    const RECT cards[] = {layout.intervalCard, layout.optionCard,
                          layout.statusCard, layout.hotkeyCard};
    for (const RECT& card : cards) {
        if (!context->theme.highContrast) {
            RECT shadow = card;
            OffsetRect(&shadow, 0, DpiScale(context->window, 2));
            const COLORREF shadowColor = context->theme.dark ? RGB(24, 24, 24) : RGB(232, 234, 238);
            DrawRoundedSurface(dc, shadow, shadowColor, shadowColor, radius);
        }
        DrawRoundedSurface(dc, card, context->theme.cardBackground,
                           context->theme.border, radius);
    }
}

void DrawOwnerDrawButton(AppContext* context, const DRAWITEMSTRUCT* item) {
    if (context == nullptr || item == nullptr || item->CtlType != ODT_BUTTON) {
        return;
    }

    const bool disabled = (item->itemState & ODS_DISABLED) != 0 ||
                          IsWindowEnabled(item->hwndItem) == FALSE;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const bool themeButton = item->CtlID >= IDC_THEME_LIGHT && item->CtlID <= IDC_THEME_SYSTEM;
    const bool checked = themeButton && ThemeControlId(context->settings.theme) ==
                                           static_cast<int>(item->CtlID);
    const bool primary = item->CtlID == IDC_START_PAUSE;

    COLORREF fill = context->theme.controlBackground;
    COLORREF border = context->theme.border;
    COLORREF textColor = context->theme.text;
    if (primary || checked) {
        fill = pressed ? context->theme.accentPressed : context->theme.accent;
        border = fill;
        textColor = context->theme.accentText;
    } else if (pressed) {
        fill = context->theme.cardBackground;
    }
    if (disabled) {
        fill = context->theme.controlBackground;
        border = context->theme.border;
        textColor = context->theme.subtleText;
    }

    RECT bounds = item->rcItem;
    DrawRoundedSurface(item->hDC, bounds, fill, border, DpiScale(context->window, 7));

    wchar_t text[128]{};
    GetWindowTextW(item->hwndItem, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
    const HGDIOBJ previousFont = SelectObject(item->hDC,
        primary ? context->actionFont : context->bodyFont);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, textColor);
    RECT textBounds = bounds;
    if (pressed) {
        OffsetRect(&textBounds, 0, DpiScale(context->window, 1));
    }
    DrawTextW(item->hDC, text, -1, &textBounds,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(item->hDC, previousFont);

    if ((item->itemState & ODS_FOCUS) != 0) {
        RECT focus = bounds;
        InflateRect(&focus, -DpiScale(context->window, 3), -DpiScale(context->window, 3));
        DrawFocusRect(item->hDC, &focus);
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
    CheckRadioButton(context->window, IDC_THEME_LIGHT, IDC_THEME_SYSTEM,
                     ThemeControlId(context->settings.theme));
    SetDlgItemInt(context->window, IDC_INTERVAL,
                  static_cast<UINT>(context->settings.intervalMilliseconds), FALSE);
    UpdateHotkeyText(context->hotkey, context->settings.hotkey);
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
        SaveWindowSize(context);
        return;
    }
    context->settings = settings;
    SaveWindowSize(context);
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
    SetWindowTextW(context->hotkeyCapture, L"取消");
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
    SetWindowTextW(context->hotkeyCapture, L"更改");

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
    case WM_CREATE:
        context->theme = DetectTheme(context->settings.theme);
        CreateUiResources(context);
        CreateControls(context);
        SetInitialSettings(context);
        ApplyLayoutAndTheme(context);
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

    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
        if (limits != nullptr) {
            const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
            const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
            UINT dpi = GetDpiForWindow(window);
            if (dpi == 0) {
                dpi = GetDpiForSystem();
            }
            WindowPixelSize minimum{};
            WindowPixelSize maximum{};
            if (WindowRectSizeForClientSize(
                    style, exStyle, FALSE, dpi,
                    WindowClientSize{static_cast<int>(kMinimumWindowClientWidth),
                                     static_cast<int>(kMinimumWindowClientHeight)},
                    &minimum) &&
                WindowRectSizeForClientSize(
                    style, exStyle, FALSE, dpi,
                    WindowClientSize{static_cast<int>(kMaximumWindowClientWidth),
                                     static_cast<int>(kMaximumWindowClientHeight)},
                    &maximum)) {
                limits->ptMinTrackSize.x = minimum.width;
                limits->ptMinTrackSize.y = minimum.height;
                limits->ptMaxTrackSize.x = maximum.width;
                limits->ptMaxTrackSize.y = maximum.height;
            }
        }
        return 0;
    }

    case WM_SIZE:
        UpdateWindowClientSize(context);
        if (context != nullptr && context->sizeMoveActive) {
            context->sizeChangedDuringMove = true;
        }
        LayoutControls(context);
        return 0;

    case WM_ENTERSIZEMOVE:
        if (context != nullptr) {
            context->sizeMoveActive = true;
            context->sizeChangedDuringMove = false;
        }
        return 0;

    case WM_EXITSIZEMOVE:
        if (context != nullptr) {
            if (context->sizeChangedDuringMove) {
                SaveWindowSize(context);
            }
            context->sizeMoveActive = false;
            context->sizeChangedDuringMove = false;
        }
        return 0;

    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested != nullptr) {
            const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
            const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
            UINT dpi = LOWORD(wParam);
            if (dpi == 0) {
                dpi = GetDpiForWindow(window);
            }
            WindowPixelSize minimum{};
            WindowPixelSize maximum{};
            if (WindowRectSizeForClientSize(
                    style, exStyle, FALSE, dpi,
                    WindowClientSize{static_cast<int>(kMinimumWindowClientWidth),
                                     static_cast<int>(kMinimumWindowClientHeight)},
                    &minimum) &&
                WindowRectSizeForClientSize(
                    style, exStyle, FALSE, dpi,
                    WindowClientSize{static_cast<int>(kMaximumWindowClientWidth),
                                     static_cast<int>(kMaximumWindowClientHeight)},
                    &maximum)) {
                const int suggestedWidth = suggested->right - suggested->left;
                const int suggestedHeight = suggested->bottom - suggested->top;
                const int width = suggestedWidth < minimum.width
                                      ? minimum.width
                                      : suggestedWidth > maximum.width ? maximum.width
                                                                       : suggestedWidth;
                const int height = suggestedHeight < minimum.height
                                       ? minimum.height
                                       : suggestedHeight > maximum.height ? maximum.height
                                                                          : suggestedHeight;
                SetWindowPos(window, nullptr, suggested->left, suggested->top, width, height,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
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
        case IDC_INTERVAL:
            if (HIWORD(wParam) == EN_CHANGE &&
                (context->state == AppState::Paused || context->state == AppState::Error)) {
                ClearSettingsError(context);
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

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const HWND control = reinterpret_cast<HWND>(lParam);
        const int id = GetDlgCtrlID(control);
        COLORREF textColor = context->theme.mutedText;
        if (id == IDC_BRAND || id == IDC_PAGE_TITLE) {
            textColor = context->theme.text;
        } else if (id == IDC_NAV_MAIN) {
            textColor = context->theme.accent;
        } else if (id == IDC_STATUS) {
            textColor = context->state == AppState::Running
                            ? context->theme.success
                            : context->state == AppState::Error ? context->theme.error
                                                                : context->theme.accent;
        } else if (id == IDC_ERROR) {
            textColor = context->theme.error;
        }
        const bool onCard = id == IDC_CLICK_OPTIONS_LABEL ||
                            id == IDC_LABEL_MOUSE_BUTTON || id == IDC_LABEL_CLICK_TYPE ||
                            id == IDC_LABEL_INTERVAL ||
                            id == IDC_LABEL_HOTKEY || id == IDC_STATUS_LABEL ||
                            id == IDC_INTERVAL_HELP ||
                            id == IDC_STATUS || id == IDC_STATUS_HELP ||
                            id == IDC_ERROR || id == IDC_HOTKEY_HELP;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, textColor);
        return reinterpret_cast<LRESULT>(onCard ? context->cardBrush
                                                : context->backgroundBrush);
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, context->theme.editBackground);
        SetTextColor(dc, context->theme.text);
        return reinterpret_cast<LRESULT>(context->editBrush);
    }

    case WM_DRAWITEM:
        DrawOwnerDrawButton(context, reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        PaintWindowSurface(context, dc);
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
    const WindowClientSize defaultClientSize = ClampWindowClientSize(
        static_cast<int>(context.settings.clientWidth),
        static_cast<int>(context.settings.clientHeight));
    context.settings.clientWidth = static_cast<std::uint32_t>(defaultClientSize.width);
    context.settings.clientHeight = static_cast<std::uint32_t>(defaultClientSize.height);
    RECT desired{};
    if (!AdjustedWindowRectForClientSize(kMainWindowStyle, 0, FALSE, initialDpi,
                                         defaultClientSize, &desired)) {
        return 1;
    }
    HWND window = CreateWindowExW(0, kWindowClassName, kWindowTitle, kMainWindowStyle,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  desired.right - desired.left, desired.bottom - desired.top,
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
