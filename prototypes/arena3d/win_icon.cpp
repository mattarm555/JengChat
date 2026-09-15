#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "win_icon.h"

void SetJengTaskbarIcon(void* windowHandle)
{
    HWND hwnd = static_cast<HWND>(windowHandle);

    if (!hwnd)
        return;

    HINSTANCE instance = GetModuleHandleA(nullptr);

    HICON bigIcon = static_cast<HICON>(
        LoadImageA(
            instance,
            MAKEINTRESOURCEA(101),
            IMAGE_ICON,
            0,
            0,
            LR_DEFAULTSIZE
        )
    );

    HICON smallIcon = static_cast<HICON>(
        LoadImageA(
            instance,
            MAKEINTRESOURCEA(101),
            IMAGE_ICON,
            16,
            16,
            LR_DEFAULTCOLOR
        )
    );

    if (bigIcon)
    {
        SendMessageA(
            hwnd,
            WM_SETICON,
            ICON_BIG,
            reinterpret_cast<LPARAM>(bigIcon)
        );
    }

    if (smallIcon)
    {
        SendMessageA(
            hwnd,
            WM_SETICON,
            ICON_SMALL,
            reinterpret_cast<LPARAM>(smallIcon)
        );
    }
}

#endif
