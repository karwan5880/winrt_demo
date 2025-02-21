// the .dll, main file for the project. 
#include "pch.h"
#include "Scapture.h"

#define DLLEXPORT extern "C" __declspec(dllexport)

//std::thread messageLoopThread;
//Scapture scapture; // I declare all variables in global scope in .dll development, someone please tell me if this is the right way to do .dll stuff.  
// Scapture stands for Screenshot Capture. 

//// Thread function for the message loop
//DWORD WINAPI MessageLoopThreadProc(LPVOID lpParameter) {
//    MSG msg;
//    BOOL bRet;
//    winrt::init_apartment();
//    try {
//        scapture.that_one_important_function_that_run_in_the_background_to_write_frame_thread_thing();
//    }
//    catch (winrt::hresult_error const& ex) {
//    }
//    return 0;
//}
//
//DLLEXPORT bool IWantOneFrame(void** dataPtr, long& _rowpitch, long& _width, long& _height, long& _totalbytes) {
//    bool result = scapture.get_one_frame(dataPtr, _rowpitch, _width, _height, _totalbytes);
//    return result;
//}
//
//DLLEXPORT bool IWantOneFrameStraight(void** dataPtr) {
//    bool result = scapture.get_one_frame_fast_version_without_checking_for_width_height_rowpitch_changed(dataPtr);
//    return result;
//}
//
//DLLEXPORT void DoNothing(void** dataPtr) {
//    scapture.donothing(dataPtr);
//}
//
//DLLEXPORT bool StartLoop() {
//    if (messageLoopThread.joinable()) {
//        return false;
//    }
//    messageLoopThread = std::thread(MessageLoopThreadProc, nullptr);
//}
//
//DLLEXPORT bool StopCapturing() {
//    if (messageLoopThread.joinable()) {
//        scapture.stoploop = true;
//        scapture.setevent();
//        PostThreadMessage(GetThreadId(static_cast<HANDLE>(messageLoopThread.native_handle())), WM_QUIT, 0, 0);
//        messageLoopThread.join();
//    }
//    return true;
//}


/* The above are the old codes. */
/* Below are new implementations using wincom SimpleCapture.cpp */

#include "Timer.h"
#include "Errors.h"
#include "SimpleCapture.h"

//wincom
namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Metadata;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::Imaging;
    using namespace Windows::System;
}

inline auto CreateDirect3DDevice(IDXGIDevice* dxgi_device)
{
    winrt::com_ptr<::IInspectable> d3d_device;
    int hr = CreateDirect3D11DeviceFromDXGIDevice(dxgi_device, d3d_device.put());
    debug_hresult(L"CreateDirect3D11DeviceFromDXGIDevice", hr);
    return d3d_device.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

std::mutex m_list_lock;
std::vector<std::shared_ptr<SimpleCapture>> m_captures;

unsigned int add_capture(std::shared_ptr<SimpleCapture> capture)
{
    std::scoped_lock lock(m_list_lock);
    std::shared_ptr<SimpleCapture> ptr;
    for (int i = 0; i < m_captures.size(); i++) {
        if (m_captures[i] == nullptr) {
            m_captures[i] = capture;
            return i;
        }
    }
    m_captures.push_back(capture);
    return (int)(m_captures.size() - 1);
}

std::shared_ptr<SimpleCapture> get_capture(unsigned int h)
{
    std::shared_ptr<SimpleCapture> ptr;
    std::scoped_lock lock(m_list_lock);
    if (h >= 0 && h < m_captures.size()) {
        ptr = m_captures[h];
    }
    return ptr;
}

//winrt::Windows::Graphics::Capture::GraphicsCaptureItem GetWindowCaptureItem() {
winrt::Windows::Graphics::Capture::GraphicsCaptureItem GetWindowCaptureItem(int& x, int& y, int& w, int& h) {
    HWND windowHandle = Helper::LoopWindow(L"Notepad"); // Class name for Notepad is "Notepad"
    if (!windowHandle)
    {
        std::cout << "cant find window. " << std::endl;
        return nullptr;
    }
    RECT windowRect;
    if (GetWindowRect(windowHandle, &windowRect)) {
        x = windowRect.left;
        y = windowRect.top;
        /*if (windowRect.right < 0) {
            w = windowRect.right + windowRect.left;
        }
        else {
            w = windowRect.right - windowRect.left;
        }*/
        w = windowRect.right - windowRect.left;
        h = windowRect.bottom - windowRect.top;
        printf("windowRect: %d %d %d %d", windowRect.left, windowRect.top, windowRect.right, windowRect.bottom);
        printf("xywh: %d %d %d %d", x, y, w, h);
        //-1530 76 -164 876
    }
    else {
        printf("getwindowrect failed!");
    }
    auto interopFactory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>(); // Get the Interop factory
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };
    winrt::check_hresult(interopFactory->CreateForWindow(windowHandle, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(), reinterpret_cast<void**>(winrt::put_abi(item))));
    return item;
}
//unsigned int __declspec(dllexport) __stdcall StartCapture(int x, int y, int width, int height, bool captureCursor)
//DLLEXPORT int StartCapture(int x, int y, int width, int height, bool captureCursor)
DLLEXPORT int StartCapture()
{
    std::cout << ".dll StartCapture " << std::endl;
    bool captureCursor = true;
    try {
        //auto mon = FindMonitor(x, y, width, height, false);
        //if (mon.hmon == nullptr)
        //{
        //    printf("Monitor not found that fully contains the bounds (%d, %d) (%d x %d)\n", x, y, width, height);
        //    FindMonitor(x, y, width, height, true);
        //    debug_hresult(L"Monitor not found", E_FAIL, true);
        //}

        winrt::com_ptr<ID3D11Device> d3dDevice;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, d3dDevice.put(), nullptr, nullptr);
        debug_hresult(L"D3D11CreateDevice", hr);

        auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
        auto device = CreateDirect3DDevice(dxgiDevice.get());

        //winrt::GraphicsCaptureItem item{ nullptr };
        //item = CreateCaptureItemForMonitor(mon.hmon);
        int x, y, w, h;
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item = GetWindowCaptureItem(x, y, w, h);

        //RECT bounds = { mon.x, mon.y, mon.x + width, mon.y + height };
        RECT bounds = { x, y, x + w, y + h };
        auto capture = std::make_shared<SimpleCapture>();
        capture->StartCapture(device, item, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, bounds, captureCursor);
        return add_capture(capture);
    }
    catch (winrt::hresult_error const& ex) {
        int hr = (int)(ex.code());
        debug_hresult(ex.message().c_str(), hr, true);
    }
    catch (std::exception const& se) {
        std::wstring msg = to_utf16(se.what());
        debug_hresult(msg.c_str(), E_FAIL, true);
    }
    return -1;
}

DLLEXPORT bool GetXYWidthHeightRowpitchTotalBytes(
    void** dataPtr,
    int& x,
    int& y,
    long& w,
    long& h,
    long& r,
    long& t) {

    printf(".dll GetXYWidthHeightRowpitchTotalBytes\n");
    std::shared_ptr<SimpleCapture> ptr = get_capture(0);
    if (ptr != nullptr) {
        ptr->GetXYWidthHeightRowpitchTotalBytes(dataPtr, x, y, w, h, r, t);
        return true;
    }
    return false;
}

std::thread readPixelsThread;

DWORD WINAPI readPixelsThreadProc(LPVOID lpParameter) {
    //printf(".dll readPixelsThreadProc\n");
    MSG msg;
    BOOL bRet;
    util::Timer threadtimer = util::Timer();
    threadtimer.Start();
    try {
        //printf(".dll readPixelsThreadProc get_capture(0)\n");
        std::shared_ptr<SimpleCapture> ptr = get_capture(0);
        if (ptr != nullptr) {
            //printf(".dll readPixelsThreadProc ptr != nullptr\n");
            while (!ptr->stoploop) {
                //printf(".dll readPixelsThreadProc ptr != nullptr ptr->ReadPixels()\n");
                //threadtimer.Start();
                //ptr->ReadPixels();
                //auto seconds = threadtimer.Seconds();
                //auto seconds = threadtimer.Milliseconds();
                //auto seconds = threadtimer.Microseconds();
                //std::cout << std::fixed << std::setprecision(6) << "microseconds: " << seconds << std::endl;
                //threadtimer.Sleep(1000); // 0.001000
                //threadtimer.Sleep(10000); // 0.01000
                //threadtimer.Sleep(100000); // 0.1000
                threadtimer.Sleep(1000000); // 1.0000
                ////Sleep(10);
                ////Sleep(2000);
            }
        }
        else {
            //printf(".dll readPixelsThreadProc ptr == nullptr\n");
        }
    }
    catch (winrt::hresult_error const& ex) {
        std::cout << "readPixelsThreadProc error. " << std::endl;
    }
    return 0;
}

DLLEXPORT int InitiateReadPixelsThread()
{
    std::cout << "InitiateReadPixelsThread" << std::endl;
    if (readPixelsThread.joinable()) {
        std::cout << "Already capturing. " << std::endl;
        return false;
    }
    std::cout << "InitiateReadPixelsThread creating thread. " << std::endl;
    readPixelsThread = std::thread(readPixelsThreadProc, nullptr);
    std::cout << "InitiateReadPixelsThread all done. " << std::endl;
    return 1;
}

DLLEXPORT bool StopReadPixelsThread() {
    if (readPixelsThread.joinable()) {
        std::shared_ptr<SimpleCapture> ptr = get_capture(0);
        if (ptr != nullptr) {
            ptr->stoploop = true;
        }
        //PostThreadMessage(GetThreadId(static_cast<HANDLE>(readPixelsThread.native_handle())), WM_QUIT, 0, 0);
        readPixelsThread.join();
    }
    return true;
}

DLLEXPORT bool ReadBuffer(void** dataPtr) {
    std::shared_ptr<SimpleCapture> ptr = get_capture(0);
    if (ptr != nullptr) {
        return ptr->ReadBuffer(dataPtr);
    }
    return false;
}
