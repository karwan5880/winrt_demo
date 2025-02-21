#pragma once
#include "pch.h"

#ifndef HELPER_H
#define HELPER_H


namespace Helper {

    BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
    HWND FindProcessWindow(LPCWSTR processName);
    HWND LoopWindow(LPCWSTR name);

    struct WindowInfo {
        HWND hwnd;
        int width;
        int height;
    };

    struct EnumData {
        std::vector<WindowInfo> foundWindows;
        const wchar_t* targetTitle;
    };

    struct EnumWindowsData {
        LPCWSTR processName;
        HWND foundWindow;
    };


}

#endif