#pragma once

#include "domain.h"

#include <windows.h>

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

ThemeColors DetectTheme(ThemeMode mode);
void ApplyDarkTitleBar(HWND window, bool dark);
