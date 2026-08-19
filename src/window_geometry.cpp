#include "window_geometry.h"

namespace {

int ScaleLogicalValue(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi == 0 ? USER_DEFAULT_SCREEN_DPI : dpi),
                  USER_DEFAULT_SCREEN_DPI);
}

int UnscalePixelValue(int value, UINT dpi) {
    return MulDiv(value, USER_DEFAULT_SCREEN_DPI,
                  static_cast<int>(dpi == 0 ? USER_DEFAULT_SCREEN_DPI : dpi));
}

} // namespace

WindowClientSize DefaultWindowClientSize() {
    return WindowClientSize{static_cast<int>(kDefaultWindowClientWidth),
                            static_cast<int>(kDefaultWindowClientHeight)};
}

WindowClientSize ClampWindowClientSize(int width, int height) {
    const int clampedWidth = width < static_cast<int>(kMinimumWindowClientWidth)
                                 ? static_cast<int>(kMinimumWindowClientWidth)
                                 : width > static_cast<int>(kMaximumWindowClientWidth)
                                       ? static_cast<int>(kMaximumWindowClientWidth)
                                       : width;
    const int clampedHeight = height < static_cast<int>(kMinimumWindowClientHeight)
                                  ? static_cast<int>(kMinimumWindowClientHeight)
                                  : height > static_cast<int>(kMaximumWindowClientHeight)
                                        ? static_cast<int>(kMaximumWindowClientHeight)
                                        : height;
    return WindowClientSize{clampedWidth, clampedHeight};
}

bool IsValidWindowClientSize(std::uint32_t width, std::uint32_t height) {
    return width >= kMinimumWindowClientWidth && width <= kMaximumWindowClientWidth &&
           height >= kMinimumWindowClientHeight && height <= kMaximumWindowClientHeight;
}

WindowPixelSize LogicalClientSizeToPixels(WindowClientSize logical, UINT dpi) {
    return WindowPixelSize{ScaleLogicalValue(logical.width, dpi),
                           ScaleLogicalValue(logical.height, dpi)};
}

WindowClientSize PixelsToLogicalClientSize(WindowPixelSize pixels, UINT dpi) {
    return WindowClientSize{UnscalePixelValue(pixels.width, dpi),
                            UnscalePixelValue(pixels.height, dpi)};
}

bool AdjustedWindowRectForClientSize(DWORD style, DWORD exStyle, BOOL hasMenu,
                                     UINT dpi, WindowClientSize logical, RECT* rect) {
    if (rect == nullptr || logical.width <= 0 || logical.height <= 0) {
        return false;
    }
    const WindowPixelSize pixels = LogicalClientSizeToPixels(logical, dpi);
    *rect = RECT{0, 0, pixels.width, pixels.height};
    if (AdjustWindowRectExForDpi(rect, style, hasMenu, exStyle, dpi) != FALSE) {
        return true;
    }
    return AdjustWindowRectEx(rect, style, hasMenu, exStyle) != FALSE;
}

bool WindowRectSizeForClientSize(DWORD style, DWORD exStyle, BOOL hasMenu,
                                 UINT dpi, WindowClientSize logical,
                                 WindowPixelSize* size) {
    if (size == nullptr) {
        return false;
    }
    RECT rect{};
    if (!AdjustedWindowRectForClientSize(style, exStyle, hasMenu, dpi, logical, &rect)) {
        return false;
    }
    size->width = rect.right - rect.left;
    size->height = rect.bottom - rect.top;
    return size->width > 0 && size->height > 0;
}

bool RectWithinClient(const RECT& rect, int clientWidth, int clientHeight) {
    return rect.left >= 0 && rect.top >= 0 && rect.right <= clientWidth &&
           rect.bottom <= clientHeight && rect.right > rect.left && rect.bottom > rect.top;
}

bool RectsOverlap(const RECT& first, const RECT& second) {
    RECT intersection{};
    return IntersectRect(&intersection, &first, &second) != FALSE;
}
