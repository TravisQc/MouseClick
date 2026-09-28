#include "theme.h"

#include <dwmapi.h>
#include <uxtheme.h>
#include <winreg.h>

namespace {

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

} // namespace

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
