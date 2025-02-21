#pragma once
#include <mutex>
#include "Timer.h"

class SimpleCapture
{
public:
    SimpleCapture();
    ~SimpleCapture() { Close(); }

    void StartCapture(
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device,
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat pixelFormat,
        RECT bounds,
        bool captureCursor);

    bool WaitForNextFrame(uint32_t timeout);

    // When copying GPU to CPU data the row pitch can be 64 or 128 bit aligned, which can mean
    // you need to pass in a slightly bigger buffer. This ensures ReadNextFrame can do a single
    // optimized memcpy operation but it also means it is up to you to crop the final image
    // to remove that extra data on the right side of each row.
    RECT GetCaptureBounds();
    void Close();

    double ReadNextFrame(uint32_t timeout, char* buffer, unsigned int size);

    RECT GetTextureBounds() { return m_croppedBounds; }
    double ReadNextTexture(uint32_t timeout, winrt::com_ptr<ID3D11Texture2D>& result);

    std::vector<double> GetArrivalTimes() { return m_arrivalTimes; }

    //karwan5880
    double ReadPixels();
    bool stoploop = false;
    bool ReadBuffer(void** dataPtr);
    bool GetXYWidthHeightRowpitchTotalBytes(
        void** dataPtr,
        int& x,
        int& y,
        long& w,
        long& h,
        long& r,
        long& t);
    int rowpitch = 0;
    int width = 0;
    int height = 0;
    int totalbytes = 0;
    int x = 0;
    int y = 0;
    HRESULT CreateOrUpdateStagingTexture2(const D3D11_TEXTURE2D_DESC& srcDesc);
    //karwan5880
    ID3D11Texture2D* stagingTextures3{ nullptr };
    winrt::com_ptr<ID3D11Texture2D> frameSurface2{ nullptr };
    winrt::com_ptr<ID3D11DeviceContext> context{ nullptr };
    D3D11_MAPPED_SUBRESOURCE mappedResource2 = {};
    HRESULT hresult;
    void* OldpDataHolder2 = nullptr;
    util::Timer privatetimer = util::Timer();
    std::chrono::steady_clock::time_point hstart;
    std::chrono::steady_clock::time_point hend;

private:
    void OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

private:
    void ReadPixels(ID3D11Texture2D* texture, char* buffer, unsigned int size);
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_device{ nullptr };
    winrt::com_ptr<ID3D11Device> m_d3dDevice{ nullptr };
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext{ nullptr };
    std::mutex frame_mutex; // to protect updating and reading of m_d3dCurrentFrame
    winrt::com_ptr<ID3D11Texture2D> m_d3dCurrentFrame{ nullptr };
    winrt::Windows::Graphics::DirectX::DirectXPixelFormat m_pixelFormat = winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized;
    bool m_closed = false;
    winrt::event_token m_frameArrivedToken;
    RECT m_bounds = { 0 };
    RECT m_croppedBounds = { 0 };;
    RECT m_captureBounds = { 0 };
    unsigned long long m_frameId = 0;
    double m_frameTime = 0;
    HANDLE m_event = NULL;
    bool m_saveBitmap = false;
    std::vector<double> m_arrivalTimes;
    double m_firstFrameTime = 0;
};