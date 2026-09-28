#pragma once

#include "app_context.h"

#include <windows.h>

void PaintWindowSurface(AppContext* context, HDC dc);
void DrawOwnerDrawButton(AppContext* context, const DRAWITEMSTRUCT* item);
