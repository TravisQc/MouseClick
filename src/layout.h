#pragma once

#include <windows.h>

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

int DpiScale(HWND window, int value);
LayoutMetrics CalculateLayout(HWND window);
bool IsLayoutValid(const LayoutMetrics& layout, const RECT& client, HWND window);
