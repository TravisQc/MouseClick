#include "controls.h"

#include "layout.h"
#include "resource.h"
#include "window_proc.h"

#include <commctrl.h>
#include <objbase.h>
#include <oleacc.h>
#include <uxtheme.h>

#include <cstdint>

namespace {

LRESULT CALLBACK HotkeyEditProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ButtonHoverProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

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

} // namespace

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

ThemeMode ThemeModeFromControlId(int id) {
    if (id == IDC_THEME_LIGHT) {
        return ThemeMode::Light;
    }
    if (id == IDC_THEME_DARK) {
        return ThemeMode::Dark;
    }
    return ThemeMode::System;
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
