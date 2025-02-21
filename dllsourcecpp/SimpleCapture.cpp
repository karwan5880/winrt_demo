#include "pch.h"
#include "SimpleCapture.h"
#include "Errors.h"

#include <winrt/Windows.Graphics.Capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.capture.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <windows.ui.composition.interop.h>
#include <d2d1_1.h>
#include <dxgi1_6.h>
#include <d3d11.h>

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Numerics;
    using namespace Windows::Graphics;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::DirectX::Direct3D11;
    using namespace Windows::System;
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
}


namespace util {

    struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
        IDirect3DDxgiInterfaceAccess : ::IUnknown
    {
        virtual HRESULT __stdcall GetInterface(GUID const& id, void** object) = 0;
    };

    template <typename T>
    auto GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
    {
        auto access = object.as<IDirect3DDxgiInterfaceAccess>();
        winrt::com_ptr<T> result;
        winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
        return result;
    }

}

// since we are requesting B8G8R8A8UIntNormalized
const int CHANNELS = 4;

CRITICAL_SECTION m_mutex;

class CriticalSectionGuard
{
public:
    CriticalSectionGuard() { EnterCriticalSection(&m_mutex); }
    ~CriticalSectionGuard() { LeaveCriticalSection(&m_mutex); }
};

SimpleCapture::SimpleCapture()
{
    InitializeCriticalSection(&m_mutex);
}

void SimpleCapture::StartCapture(
    winrt::IDirect3DDevice const& device,
    winrt::GraphicsCaptureItem const& item,
    winrt::DirectXPixelFormat pixelFormat,
    RECT bounds,
    bool captureCursor)
{
    m_frameId = 0;
    m_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_item = item;
    m_device = device;
    m_pixelFormat = pixelFormat;
    m_bounds = bounds;
    m_croppedBounds = bounds;
    m_captureBounds = bounds;

    //auto width = m_bounds.right - m_bounds.left;
    //auto height = m_bounds.bottom - m_bounds.top;
    //uint32_t size = width * height * CHANNELS;
    width = m_bounds.right - m_bounds.left;
    height = m_bounds.bottom - m_bounds.top;
    totalbytes = width * height * CHANNELS;
    x = m_bounds.left;
    y = m_bounds.top;

    m_d3dDevice = util::GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
    m_d3dDevice->GetImmediateContext(m_d3dContext.put());

    // Creating our frame pool with 'Create' instead of 'CreateFreeThreaded'
    // means that the frame pool's FrameArrived event is called on the thread
    // the frame pool was created on. This also means that the creating thread
    // must have a DispatcherQueue. If you use this method, it's best not to do
    // it on the UI thread.
    m_framePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(m_device, m_pixelFormat, 2, m_item.Size());
    m_session = m_framePool.CreateCaptureSession(m_item);
    if (!m_session.IsSupported())
    {
        throw std::exception("CreateCaptureSession is not supported on this version of Windows.\n");
    }
    m_session.IsCursorCaptureEnabled(captureCursor);

    auto session3 = m_session.try_as<winrt::Windows::Graphics::Capture::IGraphicsCaptureSession3>();
    if (session3) {
        session3.IsBorderRequired(false);
    }
    else
    {
        printf("Cannot disable the capture border on this version of windows\n");
    }

    m_frameArrivedToken = m_framePool.FrameArrived({ this, &SimpleCapture::OnFrameArrived });

    m_session.StartCapture();
}

bool SimpleCapture::WaitForNextFrame(uint32_t timeout)
{
    auto hr = WaitForMultipleObjects(1, &m_event, TRUE, timeout);
    return hr == 0;
}

double SimpleCapture::ReadNextFrame(uint32_t timeout, char* buffer, unsigned int size)
{
    if (m_closed) {
        //debug_hresult(L"ReadNextFrame: Capture is closed", E_FAIL, true);
        std::cout << "ReadNextFrame: Capture is closed" << std::endl;
    }
    // make sure a frame has been written.
    int hr = WaitForMultipleObjects(1, &m_event, TRUE, timeout);
    if (hr == WAIT_TIMEOUT) {
        printf("timeout waiting for FrameArrived event\n");
        return 0;
    }
    double frameTime = 0;
    winrt::com_ptr<ID3D11Texture2D> frame;
    {
        std::scoped_lock lock(frame_mutex);
        frame = m_d3dCurrentFrame;
        frameTime = m_frameTime;
    }

    if (frame != nullptr) {
        ReadPixels(frame.get(), buffer, size);
    }
    else {
        return 0;
    }

    return frameTime;
}

double SimpleCapture::ReadNextTexture(uint32_t timeout, winrt::com_ptr<ID3D11Texture2D>& result)
{
    if (m_closed) {
        //debug_hresult(L"ReadNextFrame: Capture is closed", E_FAIL, true);
        std::cout << "ReadNextTexture: Capture is closed" << std::endl;
        return 0;
    }

    // wait for next frame
    int hr = WaitForMultipleObjects(1, &m_event, TRUE, timeout);
    if (hr == WAIT_TIMEOUT) {
        printf("timeout waiting for FrameArrived event\n");
        return 0;
    }
    double frameTime = 0;
    {
        std::scoped_lock lock(frame_mutex);
        result = m_d3dCurrentFrame;;
        frameTime = m_frameTime;
    }

    return frameTime;
}

void SimpleCapture::Close()
{
    if (!m_closed)
    {
        m_framePool.FrameArrived(m_frameArrivedToken); // Remove the handler
        CriticalSectionGuard guard;
        m_closed = true;
        m_d3dCurrentFrame = nullptr;
        m_session.Close();
        m_framePool.Close();
        m_framePool = nullptr;
        m_session = nullptr;
        m_item = nullptr;
        CloseHandle(m_event);
    }
}

RECT SimpleCapture::GetCaptureBounds()
{
    // to get the proper CPU mapped memory bounds we need to call ReadPixels at least once.
    WaitForNextFrame(10000);
    winrt::com_ptr<ID3D11Texture2D> frame;
    {
        std::scoped_lock lock(frame_mutex);
        frame = m_d3dCurrentFrame;
    }
    if (frame != nullptr) {
        ReadPixels(frame.get(), nullptr, 0);
    }
    else {
        throw std::exception("No frames are arriving");
    }
    return m_captureBounds;
}

inline int32_t Clamp(int32_t x, int32_t min, int32_t max) {
    if (x < min) x = min;
    if (x > max) x = max;
    return x;
}

void SimpleCapture::OnFrameArrived(winrt::Direct3D11CaptureFramePool const& sender, winrt::IInspectable const&)
{
    //////
    //hend = std::chrono::high_resolution_clock::now();
    //auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(hend - hstart);
    //std::cout << "" << duration.count() / 1000000000.0 << " seconds for each FrameArrived " << std::endl;
    //hstart = std::chrono::high_resolution_clock::now();

    // mutex ensures we don't try and shut down this class while it is in the middle of handling a frame.
    CriticalSectionGuard guard;
    {
        if (m_closed) {
            return;
        }
    }

    {
        auto frame = sender.TryGetNextFrame();
        //auto _systemFrameTime = frame.SystemRelativeTime();
        //auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(_systemFrameTime);
        //auto frameTime = static_cast<double>(nanoseconds.count() / 1e9);
        //if (m_firstFrameTime == 0) {
        //    m_firstFrameTime = frameTime;
        //}
        //frameTime -= m_firstFrameTime;
        //m_arrivalTimes.push_back(frameTime);
        //auto frameSize = frame.ContentSize();
        
        // get the d3d surface.
        auto sourceTexture = util::GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        // We commented out other codes from wincom because those are for cropping from monitor. 
        // In this version, we directly capture from given HWND window handle, in this case Notepad.exe. 
        // So, no need for cropping. 

        //D3D11_TEXTURE2D_DESC desc;
        //sourceTexture->GetDesc(&desc);
        //////
        //auto width = Clamp(frameSize.Width, 0, desc.Width);
        //auto height = Clamp(frameSize.Height, 0, desc.Height);
        //D3D11_BOX srcBox = { // start xyz end x y z
        //    (UINT)Clamp((int32_t)m_bounds.left,0,  (UINT)width), // 171 // 1366 // 1352
        //    (UINT)Clamp((int32_t)m_bounds.top, 0, (UINT)height), // 158 // 800 // 793
        //    0,
        //    (UINT)Clamp((int32_t)m_bounds.right, 0, (UINT)width), // 1537 // 1366 // 1352
        //    (UINT)Clamp((int32_t)m_bounds.bottom, 0, (UINT)height), // 958 // 800 // 793
        //    1
        //};
        //printf("wth: %d %d %d %d", 
        //    Clamp((int32_t)m_bounds.left, 0, (UINT)width), 
        //    Clamp((int32_t)m_bounds.top, 0, (UINT)height),
        //    Clamp((int32_t)m_bounds.right, 0, (UINT)width),
        //    Clamp((int32_t)m_bounds.bottom, 0, (UINT)height));
        ////1366 800 // width height
        ////1537 171 958 158 // m_bounds
        ////// desc width > frameSize by 14, height by 7, 
        //printf("frameSize.width: %d, frameSize.height: %d\n", frameSize.Width, frameSize.Height);
        //printf("desc.width: %d, desc.height: %d\n", desc.Width, desc.Height);
        //printf("m_bounds: %d %d %d %d\n", m_bounds.right, m_bounds.left, m_bounds.bottom, m_bounds.top);
        //// Then we need to crop by using CopySubresourceRegion
        //desc.Usage = D3D11_USAGE_DEFAULT;
        //desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        //desc.CPUAccessFlags = 0;
        //desc.MiscFlags = 0;
        //desc.Width = srcBox.right - srcBox.left;
        //desc.Height = srcBox.bottom - srcBox.top;
        //
        ////1537 171 958 158 // m_bounds
        //printf("srcBox: %d %d %d %d\n", srcBox.right, srcBox.left, srcBox.bottom, srcBox.top);
        ////1352 171 793 158 // srcBox
        ////m_croppedBounds.left = 0;
        ////m_croppedBounds.top = 0;
        //m_croppedBounds.left = srcBox.left;
        //m_croppedBounds.top = srcBox.top;
        //m_croppedBounds.right = srcBox.right - srcBox.left;
        //m_croppedBounds.bottom = srcBox.bottom - srcBox.top;
        //m_captureBounds = m_croppedBounds;
        //winrt::com_ptr<ID3D11Texture2D> croppedTexture;
        //int hr = m_d3dDevice->CreateTexture2D(&desc, NULL, croppedTexture.put());
        //debug_hresult(L"CreateTexture2D", hr, true);
        //winrt::com_ptr<ID3D11DeviceContext> immediate;
        //m_d3dDevice->GetImmediateContext(immediate.put());
        //immediate->CopySubresourceRegion(croppedTexture.get(), 0, 0, 0, 0, sourceTexture.get(), 0, &srcBox);

        {
            std::scoped_lock lock(frame_mutex);
            //m_d3dCurrentFrame = croppedTexture;
            m_d3dCurrentFrame = sourceTexture;
            //m_frameTime = frameTime;
        }
    }

    SetEvent(m_event);
}

void SaveBitmap(UCHAR* pixels, D3D11_TEXTURE2D_DESC& desc, int stride) {

    winrt::com_ptr<IWICImagingFactory> wicFactory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(wicFactory),
        wicFactory.put_void());
    if (FAILED(hr)) {
        printf("Failed to create instance of WICImagingFactory\n");
        return;
    }

    winrt::com_ptr<IWICBitmapEncoder> wicEncoder;
    hr = wicFactory->CreateEncoder(
        GUID_ContainerFormatBmp,
        nullptr,
        wicEncoder.put());
    if (FAILED(hr)) {
        printf("Failed to create BMP encoder\n");
        return;
    }

    winrt::com_ptr<IWICStream> wicStream;
    hr = wicFactory->CreateStream(wicStream.put());
    if (FAILED(hr)) {
        printf("Failed to create IWICStream");
        return;
    }

    hr = wicStream->InitializeFromFilename(L"d:\\temp\\test.bmp", GENERIC_WRITE);
    if (FAILED(hr)) {
        printf("Failed to initialize stream from file name\n");
        return;
    }

    hr = wicEncoder->Initialize(wicStream.get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        printf("Failed to initialize bitmap encoder");
        return;
    }

    // Encode and commit the frame
    {
        winrt::com_ptr<IWICBitmapFrameEncode> frameEncode;
        wicEncoder->CreateNewFrame(frameEncode.put(), nullptr);
        if (FAILED(hr)) {
            printf("Failed to create IWICBitmapFrameEncode\n");
            return;
        }

        hr = frameEncode->Initialize(nullptr);
        if (FAILED(hr)) {
            printf("Failed to initialize IWICBitmapFrameEncode\n");
            return;
        }

        GUID wicFormatGuid = GUID_WICPixelFormat32bppBGRA;

        hr = frameEncode->SetPixelFormat(&wicFormatGuid);
        if (FAILED(hr)) {
            printf("SetPixelFormat failed.\n");
            return;
        }

        hr = frameEncode->SetSize(desc.Width, desc.Height);
        if (FAILED(hr)) {
            printf("SetSize(...) failed.\n");
            return;
        }

        hr = frameEncode->WritePixels(
            desc.Height,
            stride,
            desc.Height * stride,
            reinterpret_cast<BYTE*>(pixels));
        if (FAILED(hr)) {
            printf("frameEncode->WritePixels(...) failed.\n");
        }

        hr = frameEncode->Commit();
        if (FAILED(hr)) {
            printf("Failed to commit frameEncode\n");
            return;
        }
    }

    hr = wicEncoder->Commit();
    if (FAILED(hr)) {
        printf("Failed to commit encoder\n");
        return;
    }
}

void SimpleCapture::ReadPixels(ID3D11Texture2D* texture, char* buffer, unsigned int size) {
    // Copy GPU Resource to CPU
    D3D11_TEXTURE2D_DESC desc{};
    winrt::com_ptr<ID3D11Texture2D> copiedImage;

    texture->GetDesc(&desc);
    if (desc.SampleDesc.Count != 1) {
        printf("SampleDesc.Count != 1\n");
        return;
    }
    if (desc.MipLevels != 1) {
        printf("MipLevels != 1\n");
        return;
    }
    if (desc.ArraySize != 1) {
        printf("ArraySize != 1\n");
        return;
    }

    desc.Usage = D3D11_USAGE_STAGING; // A resource that supports data transfer (copy) from the GPU to the CPU.
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        printf("Format != DXGI_FORMAT_B8G8R8A8_UNORM\n");
        return;
    }

    HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, NULL, copiedImage.put());
    if (hr != S_OK) {
        //debug_hresult(L"failed to create texture", hr, true);
        std::cout << "failed to create texture" << std::endl;
    }

    // Copy the image out of the backbuffer.
    m_d3dContext->CopyResource(copiedImage.get(), texture);

    D3D11_MAPPED_SUBRESOURCE resource{};
    UINT subresource = D3D11CalcSubresource(0 /* slice */, 0 /* array slice */, 1 /* mip levels */); //  desc.MipLevels);
    m_d3dContext->ResolveSubresource(copiedImage.get(), subresource, texture, subresource, desc.Format);
    hr = m_d3dContext->Map(copiedImage.get(), subresource, D3D11_MAP_READ, 0, &resource);
    if (hr != S_OK) {
        //debug_hresult(L"failed to map texture", hr, true);
        std::cout << "failed to map texture" << std::endl;
    }


    UINT rowPitch = resource.RowPitch;
    unsigned int captureSize = rowPitch * desc.Height;

    if (m_saveBitmap) {
        SaveBitmap(reinterpret_cast<UCHAR*>(resource.pData), desc, rowPitch);
    }

    int x = m_croppedBounds.left;
    int y = m_croppedBounds.top;
    int w = (m_croppedBounds.right - m_croppedBounds.left);
    int h = m_croppedBounds.bottom - m_croppedBounds.top;
    if (rowPitch != w * 4)
    {
        // ResolveSubresource returns a buffer that is 8 byte aligned (64 bit).
        // Record this in the m_captureBounds so the caller can adjust their buffer accordingly.
        m_captureBounds.right = rowPitch / 4;
    }

    if (buffer) {
        char* ptr = buffer;
        char* src = reinterpret_cast<char*>(resource.pData);
        ::memcpy(ptr, src, min(size, captureSize));
    }

    m_d3dContext->Unmap(copiedImage.get(), subresource);

    //auto hstart = std::chrono::high_resolution_clock::now();
    // 
    //auto hend = std::chrono::high_resolution_clock::now();
    //auto hduration = std::chrono::duration_cast<std::chrono::nanoseconds>(hend - hstart);
    //std::cout << std::fixed << std::setprecision(10) << "" << hduration.count() / 1000000000.0 << " seconds from hstart to framenotdiscarded. " << std::endl;
}

//HRESULT SimpleCapture::CreateOrUpdateStagingTexture2(winrt::com_ptr<ID3D11Device> d3dDevice, const D3D11_TEXTURE2D_DESC& srcDesc, ID3D11Texture2D*& stagingTexture)
HRESULT SimpleCapture::CreateOrUpdateStagingTexture2(const D3D11_TEXTURE2D_DESC& srcDesc)
{
    if (stagingTextures3)
    {
        D3D11_TEXTURE2D_DESC currentDesc = {};
        stagingTextures3->GetDesc(&currentDesc);
        if (currentDesc.Width != srcDesc.Width ||
            currentDesc.Height != srcDesc.Height ||
            currentDesc.Format != srcDesc.Format)
        {
            stagingTextures3->Release();
            stagingTextures3 = nullptr;
        }
    }
    if (!stagingTextures3)
    {
        D3D11_TEXTURE2D_DESC stagingDesc = srcDesc;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0; // Not needed for binding.
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.MiscFlags = 0;

        //return d3dDevice->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
        return m_d3dDevice->CreateTexture2D(&stagingDesc, nullptr, &stagingTextures3);
    }
    return S_OK;
}

double SimpleCapture::ReadPixels() {

    //if (m_closed) {
    //    std::cout << "ReadPixels: Capture is closed" << std::endl;
    //}
    //// make sure a frame has been written.
    ////int hr = WaitForMultipleObjects(1, &m_event, TRUE, timeout);
    //int hr = WaitForMultipleObjects(1, &m_event, TRUE, 10000);
    //if (hr == WAIT_TIMEOUT) {
    //    printf("timeout waiting for FrameArrived event\n");
    //    return 0;
    //}
    //double frameTime = 0;
    //winrt::com_ptr<ID3D11Texture2D> frame;
    //{
    //    std::scoped_lock lock(frame_mutex);
    //    frame = m_d3dCurrentFrame;
    //    frameTime = m_frameTime;
    //}
    //if (frame != nullptr) {
    //    //ReadPixels(frame.get(), buffer, size);
    //}
    //else {
    //    return 0;
    //}
    //return frameTime;

    int res = WaitForNextFrame(10000);
    //privatetimer.Start();
    //auto start = std::chrono::high_resolution_clock::now();
    winrt::com_ptr<ID3D11Texture2D> frame;
    {
        std::scoped_lock lock(frame_mutex);
        frame = m_d3dCurrentFrame;
    }
    if (frame != nullptr) {
        D3D11_TEXTURE2D_DESC desc = {};
        //frameSurface2->GetDesc(&desc);
        frame->GetDesc(&desc);
        //hresult = CreateOrUpdateStagingTexture2(m_d3dDevice, desc, stagingTextures3);
        hresult = CreateOrUpdateStagingTexture2(desc);
        debug_hresult(L"failed to CreateOrUpdateStagingTexture2", hresult, true);
        m_d3dContext->CopyResource(stagingTextures3, frame.get());
        mappedResource2 = {};
        hresult = m_d3dContext->Map(stagingTextures3, 0, D3D11_MAP_READ, 0, &mappedResource2);
        OldpDataHolder2 = mappedResource2.pData; // assign to .pData Holder. 
        rowpitch = mappedResource2.RowPitch;
        //*buffer = OldpDataHolder2;
        m_d3dContext->Unmap(stagingTextures3, 0);
    }
    ////auto start = std::chrono::high_resolution_clock::now();
    //auto end = std::chrono::high_resolution_clock::now();
    //auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    //std::cout << "" << duration.count() / 1000000000.0 << " seconds for ReadPixels() " << std::endl;
    //auto seconds = privatetimer.Seconds();
    //std::cout << std::fixed << std::setprecision(6) << "seconds: " << seconds << std::endl;

    return 0;
}

bool SimpleCapture::GetXYWidthHeightRowpitchTotalBytes(
    void** dataPtr,
    int& _x,
    int& _y,
    long& w,
    long& h,
    long& r,
    long& t) {


    int res = WaitForNextFrame(10000);
    winrt::com_ptr<ID3D11Texture2D> frame;
    {
        std::scoped_lock lock(frame_mutex);
        frame = m_d3dCurrentFrame;
    }
    if (frame != nullptr) {

        D3D11_TEXTURE2D_DESC desc = {};
        frame->GetDesc(&desc);
        //hresult = CreateOrUpdateStagingTexture2(m_d3dDevice, desc, stagingTextures3);
        hresult = CreateOrUpdateStagingTexture2(desc);
        debug_hresult(L"failed to CreateOrUpdateStagingTexture2", hresult, true);
        m_d3dContext->CopyResource(stagingTextures3, frame.get());
        mappedResource2 = {};
        hresult = m_d3dContext->Map(stagingTextures3, 0, D3D11_MAP_READ, 0, &mappedResource2);
        OldpDataHolder2 = mappedResource2.pData;
        rowpitch = mappedResource2.RowPitch;
        m_d3dContext->Unmap(stagingTextures3, 0);

        _x = x;
        _y = y;
        w = width;
        h = height;
        r = rowpitch;
        totalbytes = height * rowpitch;
        *dataPtr = new BYTE[totalbytes];
        t = totalbytes;

        return true;
    }
    else {
        throw std::exception("No frames are arriving");
    }
    return false;
}

bool SimpleCapture::ReadBuffer(void** dataPtr)
{
    //std::cout << "SimpleCapture ReadBuffer" << std::endl;

    // So, if you want to Read from GPU -> CPU per call, you can use these ReadPixels code here. 
    // For now, we are just demonstrating directly assign *dataPtr to our previously read void* pointer. 
    
    int res = WaitForNextFrame(10000);
    winrt::com_ptr<ID3D11Texture2D> frame;
    {
        std::scoped_lock lock(frame_mutex);
        frame = m_d3dCurrentFrame;
    }
    if (frame != nullptr) {

        D3D11_TEXTURE2D_DESC desc = {};
        frame->GetDesc(&desc);
        //hresult = CreateOrUpdateStagingTexture2(m_d3dDevice, desc, stagingTextures3);
        hresult = CreateOrUpdateStagingTexture2(desc);
        debug_hresult(L"failed to CreateOrUpdateStagingTexture2", hresult, true);
        m_d3dContext->CopyResource(stagingTextures3, frame.get());
        mappedResource2 = {};
        hresult = m_d3dContext->Map(stagingTextures3, 0, D3D11_MAP_READ, 0, &mappedResource2);
        OldpDataHolder2 = mappedResource2.pData;
        rowpitch = mappedResource2.RowPitch;
        m_d3dContext->Unmap(stagingTextures3, 0);
    }

    // We directly assign the copied frame buffer to *dataPtr. then we return true. 
    *dataPtr = OldpDataHolder2;
    if (*dataPtr == nullptr) {
        std::cout << "*dataPtr == nullptr. " << std::endl;
        return false;
    }
    return true;
}
