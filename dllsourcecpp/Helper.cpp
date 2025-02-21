#pragma once
#include "pch.h"
#include "Helper.h"

namespace Helper {

    BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
        EnumWindowsData* pData = reinterpret_cast<EnumWindowsData*>(lParam);
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess) {
            WCHAR processName[MAX_PATH] = L"";
            if (GetModuleFileNameExW(hProcess, NULL, processName, MAX_PATH)) {
                std::wstring name(processName);
                std::transform(name.begin(), name.end(), name.begin(), ::towlower);
                std::wstring targetName(pData->processName);
                std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::towlower);
                //std::wcout << L"targetName=" << targetName << L"processName=" << processName << std::endl;
                std::wcout << "targetName=" << targetName << " processName=" << name << std::endl;

                if (name.find(targetName) != std::wstring::npos) {
                    RECT rect;
                    if (GetWindowRect(hwnd, &rect)) {
                        int width = rect.right - rect.left;
                        int height = rect.bottom - rect.top;
                        //if (width > 700 && height > 500) {
                        if (width > 100 && height > 100) {
                            //std::locale originalLocale("");
                            //std::wcout.imbue(std::locale("C"));  // Use "C" locale (no thousand separators)
                            std::wcout << std::hex << std::noshowbase  // Disable locale-based formatting
                                //<< "Found window larger than 700x500. Handle: "
                                << "Found window larger than 100x100. Handle: "
                                //<< reinterpret_cast<uintptr_t>(hwnd)
                                << hwnd
                                << ", Width: " << width
                                << ", Height: " << height << std::endl;
                            std::wcout << "Found process: " << pData->processName << ". Window handle: "
                                //<< reinterpret_cast<uintptr_t>(hwnd) << std::endl;
                                << hwnd << std::endl;
                            pData->foundWindow = hwnd;
                            CloseHandle(hProcess);
                            return FALSE; // Stop enumerating
                        }
                    }
                }
            }
            CloseHandle(hProcess);
        }
        return TRUE; // Continue enumerating
    }

    HWND FindProcessWindow(LPCWSTR processName) {
        EnumWindowsData data = { processName, NULL };
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
        return data.foundWindow;
    }

    HWND LoopWindow(LPCWSTR name) {
        //_setmode(_fileno(stdout), _O_U16TEXT); // Or _O_U16TEXT
        //_setmode(_fileno(stdout), _O_U8TEXT); // Or _O_U16TEXT
        //std::locale::global(std::locale("")); // Set global locale
        std::wcout.imbue(std::locale());      // Apply locale to wcout
        std::wcout << name << " " << std::endl;
        HWND foundWindow = FindProcessWindow(name);
        if (foundWindow) {
            std::cout << "Process found!" << std::endl;
        }
        else {
            std::cout << "Process not found." << std::endl;
        }
        return foundWindow;
    }



}