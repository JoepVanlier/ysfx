
#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN64)
#define NOMINMAX  // sigh
#include <windows.h>

void forceWindowDown(void* windowHandle)
{
    HWND hwnd = (HWND) windowHandle;
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    HWND foregroundWindow = GetForegroundWindow();
    SetWindowPos(hwnd, foregroundWindow, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
};

#else
void forceWindowDown(void* windowHandle) {
    (void) windowHandle;
}
#endif
