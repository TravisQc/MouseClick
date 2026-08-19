#pragma once

#include <windows.h>

#include <cstdint>

constexpr std::uint32_t kMinimumWindowClientWidth = 480;
constexpr std::uint32_t kMinimumWindowClientHeight = 440;
constexpr std::uint32_t kMaximumWindowClientWidth = 600;
constexpr std::uint32_t kMaximumWindowClientHeight = 500;
constexpr std::uint32_t kDefaultWindowClientWidth = 600;
constexpr std::uint32_t kDefaultWindowClientHeight = 500;

struct WindowClientSize {
    int width = 0;
    int height = 0;
};

struct WindowPixelSize {
    int width = 0;
    int height = 0;
};

WindowClientSize DefaultWindowClientSize();
WindowClientSize ClampWindowClientSize(int width, int height);
bool IsValidWindowClientSize(std::uint32_t width, std::uint32_t height);
WindowPixelSize LogicalClientSizeToPixels(WindowClientSize logical, UINT dpi);
WindowClientSize PixelsToLogicalClientSize(WindowPixelSize pixels, UINT dpi);
bool AdjustedWindowRectForClientSize(DWORD style, DWORD exStyle, BOOL hasMenu,
                                     UINT dpi, WindowClientSize logical, RECT* rect);
bool WindowRectSizeForClientSize(DWORD style, DWORD exStyle, BOOL hasMenu,
                                 UINT dpi, WindowClientSize logical,
                                 WindowPixelSize* size);
bool RectWithinClient(const RECT& rect, int clientWidth, int clientHeight);
bool RectsOverlap(const RECT& first, const RECT& second);
