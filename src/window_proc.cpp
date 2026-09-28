#include "ui.h"

#include "app_context.h"
#include "controls.h"
#include "layout.h"
#include "painting.h"
#include "resource.h"
#include "theme.h"
#include "window_geometry.h"
#include "window_proc.h"

#include <commctrl.h>
#include <objbase.h>

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
