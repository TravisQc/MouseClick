#pragma once

#include "click_worker.h"
#include "domain.h"
#include "hotkey.h"
#include "settings.h"
#include "theme.h"

#include <windows.h>

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
