#include "painting.h"

#include "layout.h"
#include "resource.h"

#include <windows.h>

namespace {

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

} // namespace

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
