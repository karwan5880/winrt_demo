#include "pch.h"
#include "Scapture.h"

#define DLLEXPORT extern "C" __declspec(dllexport)

std::thread messageLoopThread;
Scapture scapture; // I declare all variables in global scope in .dll development, someone please tell me if this is the right way to do .dll stuff.  
// Scapture stands for Screenshot Capture. 

// Thread function for the message loop
DWORD WINAPI MessageLoopThreadProc(LPVOID lpParameter) {
    MSG msg;
    BOOL bRet;
    winrt::init_apartment();
    try {
        scapture.that_one_important_function_that_run_in_the_background_to_write_frame_thread_thing();
    }
    catch (winrt::hresult_error const& ex) {
    }
    return 0;
}

DLLEXPORT bool IWantOneFrame(void** dataPtr, long& _rowpitch, long& _width, long& _height, long& _totalbytes) {
    bool result = scapture.get_one_frame(dataPtr, _rowpitch, _width, _height, _totalbytes);
    return result;
}

DLLEXPORT bool IWantOneFrameStraight(void** dataPtr) {
    bool result = scapture.get_one_frame_fast_version_without_checking_for_width_height_rowpitch_changed(dataPtr);
    return result;
}

DLLEXPORT void DoNothing(void** dataPtr) {
    scapture.donothing(dataPtr);
}

DLLEXPORT bool StartLoop() {
    if (messageLoopThread.joinable()) {
        return false;
    }
    messageLoopThread = std::thread(MessageLoopThreadProc, nullptr);
}

DLLEXPORT bool StopCapturing() {
    if (messageLoopThread.joinable()) {
        scapture.stoploop = true;
        scapture.setevent();
        PostThreadMessage(GetThreadId(static_cast<HANDLE>(messageLoopThread.native_handle())), WM_QUIT, 0, 0);
        messageLoopThread.join();
    }
    return true;
}