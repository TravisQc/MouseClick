#include "layout.h"

#include "window_geometry.h"

int DpiScale(HWND window, int value) {
    const UINT dpi = window == nullptr ? GetDpiForSystem() : GetDpiForWindow(window);
    return MulDiv(value, static_cast<int>(dpi), 96);
}

namespace {

RECT MakeRect(int x, int y, int width, int height) {
    return RECT{x, y, x + width, y + height};
}

} // namespace

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
