#pragma once
#include "pch.h"
#include "Helper.h"


class Scapture {
private:

public:
    Scapture();
    ~Scapture();

    void that_one_important_function_that_run_in_the_background_to_write_frame_thread_thing();
    bool get_one_frame(void** dataPtr, long& _rowpitch, long& _width, long& _height, long& _totalbytes);
    bool get_one_frame_fast_version_without_checking_for_width_height_rowpitch_changed(void** dataPtr);
    bool get_confirmation_that_width_height_rowpitch_remain_the_same_else(void** dataPtr, long& _rowpitch, long& _width, long& _height, long& _totalbytes);

    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    winrt::com_ptr<ID3D11Texture2D> frameSurface;
    ID3D11Texture2D* stagingTextures[10];
    int sTiConstant = 10;
    bool callertrue = true;
    bool stoploop = false;
    bool heventsimilar = false;
    int width = 10;
    int height = 10;
    int rowpitch = 10;
    int newwidth = 20;
    int newheight = 20;
    int newrowpitch = 20;
    int heightrowpitch = 400;
    int totalbytes = 400;
    HANDLE resourceReadyEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    void* OldpDataHolder = nullptr;
    void donothing(void** dataPtr);
    void setevent();

    //ID3D11Query* fenceQueries[2] = { nullptr, nullptr };
    //bool discardframe = true;
    //int mr = 0;
    //void** scapturedataPtr = nullptr;
    //bool mappedresourcenotready = true;
    //bool memmovenotfinished = true;
    //bool framecopyfinished = false;
    //void* tempdataptr = nullptr;
    //void renewrowpitchandtotalbytes();
    //std::promise<void> resourceReadyPromise;
    //std::future<void> resourceReadyFuture = resourceReadyPromise.get_future();
    //std::mutex resourceMutex;
    //std::condition_variable resourceCV;
    //bool resourceReady = false; 
    //int verify_random_number = 0;
    //std::chrono::steady_clock::time_point hstart;
    //std::chrono::steady_clock::time_point hend;





    winrt::com_ptr<ID3D11Device> w_d3dDevice;
    winrt::com_ptr<ID3D11DeviceContext> w_d3dContext;

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_d3dDevice{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_captureSession{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem{ nullptr };
    winrt::event_token m_frameArrivedToken;



};
