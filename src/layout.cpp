#include "layout.h"

#include "window_geometry.h"

int DpiScale(HWND window, int value) {
    const UINT dpi = window == nullptr ? GetDpiForSystem() : GetDpiForWindow(window);
    return MulDiv(value, static_cast<int>(dpi), 96);
}

namespace {

// The main content is a single vertical stack of rows, described here at
// logical (96-DPI) sizes and resolved top-to-bottom. Exactly one row is
// flexible (kFlexHeight) and absorbs whatever vertical space is left over, so
// adding, reordering, or resizing a card is a matter of editing this table
// instead of hand-balancing top/bottom pixel offsets.
constexpr int kFlexHeight = -1;

enum class Row {
    Header,   // spacer for the title/header area; no card rect is emitted
    Interval, // full-width interval card
    Detail,   // two side-by-side cards: options (left) + status (right)
    Hotkey,   // full-width hotkey card
    Action,   // full-width start/pause button
};

struct RowSpec {
    Row row;
    int height;   // logical px, or kFlexHeight for the flexible row
    int gapAfter; // logical px to the next row; the last entry is the bottom margin
};

constexpr RowSpec kRows[] = {
    {Row::Header,   58,          8},
    {Row::Interval, 68,          12},
    {Row::Detail,   kFlexHeight, 12},
    {Row::Hotkey,   68,          10},
    {Row::Action,   46,          16},
};

constexpr int kContentMargin = 16;   // left/right margin around the stack
constexpr int kColumnGap = 12;       // gap between the two columns of the detail row
constexpr int kMinFlexHeight = 140;  // fallback flexible height when the window is too short

RECT MakeRect(int x, int y, int width, int height) {
    return RECT{x, y, x + width, y + height};
}

// Scaled height consumed by every fixed row plus every gap, i.e. all vertical
// space that is not available to the flexible row.
int FixedStackHeight(HWND window) {
    int total = 0;
    for (const RowSpec& spec : kRows) {
        if (spec.height != kFlexHeight) {
            total += DpiScale(window, spec.height);
        }
        total += DpiScale(window, spec.gapAfter);
    }
    return total;
}

} // namespace

LayoutMetrics CalculateLayout(HWND window) {
    RECT client{};
    GetClientRect(window, &client);

    const int margin = DpiScale(window, kContentMargin);
    const int mainLeft = margin;
    const int mainRight = client.right > margin ? client.right - margin : mainLeft;
    const int contentWidth = mainRight > mainLeft ? mainRight - mainLeft : 0;

    const int flexRaw = client.bottom - FixedStackHeight(window);
    const int flexHeight = flexRaw > 0 ? flexRaw : DpiScale(window, kMinFlexHeight);

    LayoutMetrics layout;
    layout.mainLeft = mainLeft;
    layout.mainRight = mainRight;

    int y = 0;
    for (const RowSpec& spec : kRows) {
        const int height = spec.height == kFlexHeight ? flexHeight : DpiScale(window, spec.height);
        switch (spec.row) {
        case Row::Header:
            break; // spacer only
        case Row::Interval:
            layout.intervalCard = MakeRect(mainLeft, y, contentWidth, height);
            break;
        case Row::Detail: {
            const int columnGap = DpiScale(window, kColumnGap);
            const int leftWidth = (contentWidth - columnGap) / 2;
            const int rightWidth = contentWidth - leftWidth - columnGap;
            layout.optionCard = MakeRect(mainLeft, y, leftWidth, height);
            layout.statusCard = MakeRect(mainLeft + leftWidth + columnGap, y, rightWidth, height);
            break;
        }
        case Row::Hotkey:
            layout.hotkeyCard = MakeRect(mainLeft, y, contentWidth, height);
            break;
        case Row::Action:
            layout.actionButton = MakeRect(mainLeft, y, contentWidth, height);
            break;
        }
        y += height + DpiScale(window, spec.gapAfter);
    }

    // Interval input box: right-aligned inside the interval card, leaving room for the "ms" label.
    const int intervalHeight = layout.intervalCard.bottom - layout.intervalCard.top;
    const int boxW1 = DpiScale(window, 175);
    const int boxH1 = DpiScale(window, 34);
    const int msW = DpiScale(window, 24);
    const int boxRight1 = layout.intervalCard.right - DpiScale(window, 16) - msW;
    const int boxLeft1 = boxRight1 - boxW1;
    const int boxTop1 = layout.intervalCard.top + (intervalHeight - boxH1) / 2;
    layout.intervalInputBox = MakeRect(boxLeft1, boxTop1, boxW1, boxH1);

    // Hotkey input box: right-aligned inside the hotkey card, left of the "change" button.
    const int hotkeyHeight = layout.hotkeyCard.bottom - layout.hotkeyCard.top;
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
