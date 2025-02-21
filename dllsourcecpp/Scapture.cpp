#pragma once
#include "pch.h"
#include "Scapture.h"
#include "Helper.h"

using namespace winrt;

// not sure what this is, copied from somewhere. 
struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
	IDirect3DDxgiInterfaceAccess : ::IUnknown
{
	virtual HRESULT __stdcall GetInterface(GUID const& id, void** object) = 0;
};

// this also copy from the following link:
// https://gist.github.com/kennykerr/15a62c8218254bc908de672e5ed405fa
template <typename T>
auto GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
{
	auto access = object.as<IDirect3DDxgiInterfaceAccess>();
	winrt::com_ptr<T> result;
	winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
	return result;
}

// constructor
Scapture::Scapture()
{
	printf("Scapture constructor. ");

	for (int i = 0; i < sTiConstant; i++) {
		stagingTextures[i] = nullptr;
	}
}

// destructor
Scapture::~Scapture() {
	printf("Scapture destructor. ");
	// did i forgot something here
	// does it matter
}

// dont know what is this, copied from somewhere, looks important
struct __declspec(uuid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d"))
	IMemoryBufferByteAccess : public ::IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetBuffer(BYTE** value, UINT32* capacity) = 0;
};

// the function that saves us a bit of overhead time. 
HRESULT CreateOrUpdateStagingTexture(winrt::com_ptr<ID3D11Device> d3dDevice, const D3D11_TEXTURE2D_DESC& srcDesc, ID3D11Texture2D*& stagingTexture)
{
	if (stagingTexture)
	{
		D3D11_TEXTURE2D_DESC currentDesc = {};
		stagingTexture->GetDesc(&currentDesc);
		if (currentDesc.Width != srcDesc.Width ||
			currentDesc.Height != srcDesc.Height ||
			currentDesc.Format != srcDesc.Format)
		{
			stagingTexture->Release();
			stagingTexture = nullptr;
		}
	}

	if (!stagingTexture)
	{
		D3D11_TEXTURE2D_DESC stagingDesc = srcDesc;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags = 0; // Not needed for binding.
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stagingDesc.MiscFlags = 0;

		return d3dDevice->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
	}
	return S_OK;
}

// I generate this function from Gemini Flash-Thinking-Experimental and ChatGPT o3-mini
winrt::com_ptr<ID3D11Device> CreateD3DDevice() {
	winrt::com_ptr<ID3D11Device> d3dDevice;
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};
	HRESULT hr = D3D11CreateDevice(
		nullptr, // default adapter
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		creationFlags,
		featureLevels,
		ARRAYSIZE(featureLevels),
		D3D11_SDK_VERSION,
		d3dDevice.put(),
		nullptr,
		nullptr);
	winrt::check_hresult(hr);
	return d3dDevice;
}

// important function to get the desired window, should improve it such that it receives a string argument instead of hardcoded in. 
winrt::Windows::Graphics::Capture::GraphicsCaptureItem GetWindowCaptureItem() {	
	HWND windowHandle = Helper::LoopWindow(L"Notepad"); // using custom findwindow function because the FindWindowW() provided by Windows API SUCK!!!
	//HWND windowHandle = Helper::LoopWindow(L"chrome"); 
	if (!windowHandle)
	{
		std::cout << "cant find window. " << std::endl;
		return nullptr;
	}
	auto interopFactory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>(); // Get the Interop factory
	winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };
	winrt::check_hresult(interopFactory->CreateForWindow(windowHandle, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(), reinterpret_cast<void**>(winrt::put_abi(item))));
	return item;
}

// here comes the deal. 
void Scapture::that_one_important_function_that_run_in_the_background_to_write_frame_thread_thing() {

	//winrt::init_apartment(); // not sure if this is needed. 

	// 1. Get Capture Item
	winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem = GetWindowCaptureItem();
	if (!captureItem)
	{
		std::wcerr << L"Error: GetWindowCaptureItem failed." << std::endl;
		return;
	}
	else {
		std::cout << "captureItem created. " << std::endl;
	}

	// 2. Create D3D Device
	winrt::com_ptr<ID3D11Device> d3dDevice = CreateD3DDevice();
	winrt::com_ptr<IDXGIDevice> dxgiDevice;
	d3dDevice.as(dxgiDevice);
	winrt::com_ptr<::IInspectable> inspectable;
	winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
	auto winrtD3DDevice = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
	winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_d3dDevice = winrtD3DDevice;
	std::cout << "create d3d device success. " << std::endl;

	// 3. Create Frame Pool
	//winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
	winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
		m_d3dDevice,
		winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
		1,
		//2,
		captureItem.Size()
	);
	std::cout << "create framepool success. " << std::endl;

	//4. Create Capture Session
	winrt::Windows::Graphics::Capture::GraphicsCaptureSession captureSession = framePool.CreateCaptureSession(captureItem);
	std::cout << "create capture session success. " << std::endl;

	auto session3 = captureSession.try_as<winrt::Windows::Graphics::Capture::IGraphicsCaptureSession3>();
	if (session3) {
		session3.IsBorderRequired(false);
	}
	else
	{
		printf("Cannot disable the capture border on this version of windows\n");
	}

	ID3D11DeviceContext* context = nullptr;
	d3dDevice->GetImmediateContext(&context);

	D3D11_TEXTURE2D_DESC stagingDesc = {};
	HRESULT hr;
	int sTi = 0; // sTi stands for Staging Texture Index;

	bool isWait = true; // some random variables/flags here and there. 
	bool recreateStaging = false;
	bool isinitialize = true;
	int coin_token = 0;
	int framecounter = 0;
	bool realrun = false;
	int frameskip = 0; // some random variables/flags here and there. 

	// 5. Register FrameArrived (using lambda)
	std::cout << "creating framearrived callback. " << std::endl;
	auto frameArrivedToken = framePool.FrameArrived(winrt::auto_revoke, [&](auto& sender, auto& args)
		{
			//std::wcout << L"this is a frame. " << std::endl;
			//std::wcout << L"Frame Arrived! - Minimal Example" << std::endl;
			//std::wcout << L"coin_token test: " << coin_token++ << std::endl;

			winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame = sender.TryGetNextFrame();
			winrt::Windows::Graphics::SizeInt32 frame_content_size = frame.ContentSize();

			if (frame) {
				//printf("frame == true\n");
				if (isinitialize) { // Initially I plan to set the width and height variable using first few arrived frame. 
					//printf("initialize == true\n"); // But I have better implementation later on. 
					framecounter++; // framecounter for debugging purpose. 
					isinitialize = false;
					winrt::com_ptr<ID3D11Texture2D> frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
					D3D11_TEXTURE2D_DESC desc = {};
					frameSurface->GetDesc(&desc);
					width = desc.Width;
					height = desc.Height;
					frame.Close();
					//printf("initialize complete. ");
					return;
				}
				//if (discardframe) { // I had one idea here to discard frame or not to discard frame. 
					//discardframe = false;
				else if (!callertrue) { // I was using a flag to decide whether to proceed to process the frame or not. 
					printf("!callertrue (frame discarded. )\n"); // But later on I have a better design. 
					frame.Close();
					return;
				}
				else { // If arrived this else block, that means we process the frame. 
					//printf("frame not discarded\n");
					//callertrue = false; // The bad design. 
					//
					frameskip++; // New Idea. 
					//if (frameskip > 10) { // to saves energy/gpu power. 
					if (frameskip > 5) { // to saves energy, gpu power. 
						//if (frameskip > 2) { // to saves energy/gpu power. 
						//if (frameskip > 1) { // to saves energy/gpu power. 
						//if (frameskip > 0) { // this means process all frames no skip. 
						frameskip = 0;
						// proceed
					}
					else { // skip n frames before processing one frame. 
						frame.Close();
						return;
					}
				}
			}

			if (frame) {				
				frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
				//winrt::com_ptr<ID3D11Texture2D> frameSurface; <-- declare in header (Scapture class private variable. )

				// Retrieve texture description (e.g. width, height, format)
				D3D11_TEXTURE2D_DESC desc = {};
				frameSurface->GetDesc(&desc);

				// Ensure the staging texture for the current buffer is valid and up-to-date.
				// This function is to reduce the overhead of needing to recreate new stagingtexture every loop!
				// I use sTi = 10; meaning the StagingTexturesIndex = 10, meaning i create 10 stagingtextures (array of size 10. )
				// Why I do this, I was experimenting if the frame bytes pointer got deleted or not. (Unmapped)
				CreateOrUpdateStagingTexture(d3dDevice, desc, stagingTextures[sTi]);
				//HRESULT hr = CreateOrUpdateStagingTexture(d3dDevice, desc, stagingTextures[sTi]);
				//if (FAILED(hr)) {
				//	//std::cerr << "Failed to create/update staging texture." << std::endl;
				//	return;
				//}
				
				// Copy the captured frame (GPU texture) into the staging texture.
				//context->CopyResource(stagingTexture, frameSurface.get());
				context->CopyResource(stagingTextures[sTi], frameSurface.get());

				if (true) // Useless true block because I was experimenting D3D11_MAP_FLAG_DO_NOT_WAIT. 
				{
					// Map the staging texture to get a CPU pointer to its data.
					//D3D11_MAPPED_SUBRESOURCE mappedResource = {};
					// I changed this mappedResource to Scapture.h class private member. 
					mappedResource = {};
					//hr = context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
					//hr = context->Map(stagingTextures[sTi], 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mappedResource);
					hr = context->Map(stagingTextures[sTi], 0, D3D11_MAP_READ, 0, &mappedResource);
					if (SUCCEEDED(hr)) {
						//// The data pointer now points to the beginning of the image data.
						//// Assuming a 32-bit per pixel format (e.g. BGRA), each pixel is 4 bytes.

						//rowpitch = mappedResource.RowPitch;
						//size_t bytes_per_pixel = 4;
						//size_t total_bytes = (desc.Height - 1) * mappedResource.RowPitch + (desc.Width * bytes_per_pixel); // This is the wrong one I believe
						////size_t total_bytes = desc.Height * desc.Width * bytes_per_pixel; // also wrong one. 
						//BYTE* data_ptr = reinterpret_cast<BYTE*>(mappedResource.pData); // not needed. 

						////// Print out the first 10 bytes.
						////std::cout << "First 10 bytes of captured frame: ";
						////for (int i = 0; i < 10; i++) {
						////	std::cout << static_cast<int>(data_ptr[i]) << " ";
						////}
						////std::cout << std::endl;


						if (framecounter < 100) framecounter++; // need better design here. 
						//if (framecounter >= 4) {
						if (realrun) {
							//printf("real run so wont skip write_frames. \n");
							newwidth = desc.Width;
							newheight = desc.Height;
							newrowpitch = mappedResource.RowPitch;
							OldpDataHolder = mappedResource.pData; // temporarily
							// This is our current design, saves .pData to OldpDataHolder a.k.a. "write" operation.  
							// I tried directly memmove or memcpy the mapppedResource.pData to our dataPtr. 
							// memmove / memcpy took 0.001 second, which is slow. 
							// Here I tried memcpy memmove, 
							// I tried conditional variable mutex, 
							// I tried future promise, 
							// I tried WaitForSingleObject SetEvent, 
							// problem is that, I don't understand why the Window Pump Message loop will delay our reflex by 
							// 0.001 second ~ sometimes up to 0.006 second, 
							// therefore I just abandoned the idea to setup a gate here. 
							// instead, whenever a frame arrived and processed, I just point it to OldpDataHolder
							// in our read thread, it will just take the bytes of what OldpDataHolder is pointing, 
							// and wala, no error, 
							// I was worrying if the OldpDataHolder pointer bytes will get deleted, due to ->Unmap in the code later, 
							// but hey no, they are not, why is that, I have no idea, but it works. 
						}
						else {
							printf("not real run so skip write_frames. framecounter: %d\n", framecounter);
							if (framecounter >= 4) realrun = true;
						}
						// Unmap the staging texture.
						context->Unmap(stagingTextures[sTi], 0);
					}
					else {
						// Handle the mapping failure (log error, etc.)
						std::cout << "Buffer " << sTi << " not ready (asynchronously mapping failed). " << std::endl;
						printf("Map failed with HRESULT: 0x%08X\n", hr);
					}
				}
				//sTi = (sTi + 1) % 2;
				//sTi = (sTi + 1) % 10;
				sTi = (sTi + 1) % sTiConstant;
				//// Release the staging texture (cleanup).
				//stagingTexture->Release();
				//// We will ->Release() all 10 stagingTexture in our Scapture::~Scapture() destructor. 
				//// Hopefully I don't forget to do that. 
			}
			frame.Close();
			//////// Post a quit message to the DispatcherQueue to exit the console app gracefully (if needed in more complex scenarios)
			//auto dispatcherQueue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
			//if (dispatcherQueue) {
			//    dispatcherQueue.TryEnqueue([]() {
			//        PostQuitMessage(0); // Post a WM_QUIT message
			//    });
			//}
			//////// In our use case, we want this frameArrived callback to exist until user explicitly shut off the program. 
			//////// which we will control it in our .dll functions. 
		});
	std::cout << "create framearrived callback success. start capturing. " << std::endl;

	//6. Start Capture
	captureSession.StartCapture();

	/* STARTING FROM HERE IS VERY UGLY CODE */
	/* THERE ARE TWO WAYS TO CREATE FRAMEPOOL */
	/* Create() */
	/* AND */
	/* CreateFreeThreaded() */
	/* IF WE USE Create(), THEN WE NEED TO PUMP MESSAGE HERE. */
	/* BUT I DECIDED TO USE CreateFreeThreaded() SO I COMMENT OUT THIS SECTION. */

	
	//std::wcout << L"Screen capture started. Pumping messages for 5 seconds..." << std::endl;
	//auto startTime = std::chrono::steady_clock::now();
	//while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(1))
	//{
	//    MSG msg;
	//    // Pump any pending messages.
	//    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	//    {
	//        TranslateMessage(&msg);
	//        DispatchMessage(&msg);
	//    }
	//    std::this_thread::sleep_for(std::chrono::milliseconds(200));
	//}

	bool peekmessagebool = false;
	DWORD waitResult;
	bool preak = false;
	int counter = 0;
	char q;
	MSG msg;
	auto startTime = std::chrono::steady_clock::now();
	//std::cout << "pump message start. " << std::endl;
	//while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(60))
	while (true)
	{
		//hstart = std::chrono::high_resolution_clock::now();
		while (framecounter < 4) {
			//framecounter++;
			//printf("framecounter < 5 (warmup phase ..)\n");
			callertrue = true;
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			if (framecounter >= 4) realrun = true;
		}

		// .dll will call SetEvent() and we free to go. bye. 
		WaitForSingleObject(resourceReadyEvent, INFINITE); // Wait indefinitely (or with timeout)
		if (stoploop) {
			std::cout << "stoploop. break." << std::endl;
			break;
		}
		else {
			//std::cout << "no stoploop. " << std::endl;
			//std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		if (heventsimilar) {
			//hstart = std::chrono::high_resolution_clock::now();
			//std::cout << "hevents signalled, opening gate to screenshot. " << std::endl;
			heventsimilar = false;
			callertrue = true;
		}
		else {
			std::this_thread::yield();
		}
		//}
		//else if (waitResult == WAIT_TIMEOUT) {
		//	//std::cout << "Timeout occurred. Event was not signaled within " << 100 << " milliseconds." << std::endl;
		//	// The timeout period elapsed, and the event was not signaled
		//}
		//else if (waitResult == WAIT_FAILED) {
		//	std::cerr << "WaitForSingleObject failed. Error: " << GetLastError() << std::endl;
		//	// An error occurred during WaitForSingleObject
		//}
		//else {
		//	std::cout << "WaitForSingleObject returned unexpected value: " << waitResult << std::endl;
		//	// Handle unexpected return values if needed. (Rare in typical event usage)
		//}
		////std::cout << "pump message loop. " << std::endl;
		//if (_kbhit())
		//{
		//	std::cout << "kbhit!" << std::endl;
		//	int ch = _getch();
		//	if (ch == 27) // Escape key code
		//	{
		//		break;
		//	}
		//	else if (ch == 102) {
		//		callertrue = true;
		//	}
		//}
		////std::cout << "q to quit, else capture a screenshot. " << std::endl;
		////q = std::cin.get();
		////if (q == 'q') {
		////	break;
		//////}
		//peekmessagebool = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
		//if (peekmessagebool) {
		//	//std::cout << "peekmessagebool: " << peekmessagebool << "msg.message: " << msg.message << std::endl;
		//	TranslateMessage(&msg);
		//	DispatchMessage(&msg);
		//}
		//while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		//{
		//	std::cout << "peekmessage msg: " << msg.message << std::endl;
		//	hstart = std::chrono::high_resolution_clock::now();
		//	TranslateMessage(&msg);
		//	DispatchMessage(&msg);
		//	hend = std::chrono::high_resolution_clock::now();
		//	auto hduration = std::chrono::duration_cast<std::chrono::nanoseconds>(hend - hstart);
		//	std::cout << std::fixed << std::setprecision(10) << "" << hduration.count() / 1000000000.0 << " seconds for trans+dispatch message. " << std::endl;
		//	counter++;
		//}
		//while (GetMessage(&msg, nullptr, 0, 0))
		//{
		//	DispatchMessage(&msg);
		//	counter++;
		//	//if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(10)) {
		//	//	std::cout << "break!!!" << std::endl;
		//	//	break;
		//	//}
		//	std::cout << "q to quit, else capture a screenshot. " << std::endl;
		//	//q = std::cin.get();
		//	//if (q == 'q') {
		//	//	preak = true;
		//	//	break;
		//	//}
		//	//discardframe = true;
		//	//GetMessage(&msg, nullptr, 0, 0);
		//	//DispatchMessage(&msg);
		//	//discardframe = false;
		//}
		//if (preak) break;
		////std::this_thread::sleep_for(std::chrono::nanoseconds(1)); // IS IT BECAUSE OF YOU???????
		//hend = std::chrono::high_resolution_clock::now();
		//auto hduration = std::chrono::duration_cast<std::chrono::nanoseconds>(hend - hstart);
		//std::cout << std::fixed << std::setprecision(10) << "" << hduration.count() / 1000000000.0 << " seconds for each message pump loop. " << std::endl;
	}
	//cv::destroyAllWindows();
	//std::cout << "fps: " << counter << std::endl;
	//for (int i = 0; i < sTiConstant; i++) {
	//	if (stagingTextures[i]) {
	//		stagingTextures[i]->Release();
	//	}
	//	else {
	//		//printf("stagingTextures[0] nullptr. \n");
	//	}
	//}
	//if (stagingTextures[0]) {
	//	stagingTextures[0]->Release();
	//}
	//else {
	//	//printf("stagingTextures[0] nullptr. \n");
	//}
	//if (stagingTextures[1]) {
	//	stagingTextures[1]->Release();
	//}
	//else {
	//	//printf("stagingTextures[1] nullptr. \n");
	//}
	//MSG msg;
	//while (GetMessage(&msg, nullptr, 0, 0))
	//{
	//	DispatchMessage(&msg);
	//}
	//// Stop Graphics Capture Session (if needed - depends on how you structured it)
	//if (captureSession) {
	//    captureSession.Close();
	//    captureSession = nullptr;
	//}

	std::cout << "sleep for 1 seconds then end program i guess. bye Scapture::that_one_important_function_that_run_in_the_background_to_write_frame_thread_thing()  " << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	
	/* HERE END OF THIS FUNCTION */
	/* WE NEED TO KEEP THIS FUNCTION ALIVE/LOOPING */
	/* SO THAT THE FRAME WILL KEEP ON WRITING TO OUR Scapture POINTER */
	/* SO THAT OUR .DLL CAN KEEP READING FROM THE NEW (OR OLD) FRAMES */
	/* IF THIS FUNCTION (OR THREAD) ENDED, ALL THE VARIABLES E.G., GRAPHICSCAPTUREDITEM, */
	/* D3DDEVICE, D3DCONTEXT, FRAMEPOOL, GRAPHICSSESSION, FRAMEARRIVED, CALLBACK, */
	/* EVERYTHING WILL GET DESTROYED. */
	
	/* this is my implementation/demonstration on winrt Windows Graphics Capture library to */
	/* to capture any desktop window, using class/object-oriented design. * /
	/* i am not good with C++ code, I wish to rewrite the whole thing, */
	/* so that it becomes more efficient, and more powerful, */
	/* thank you.  */
	
	/* Author : github.com/karwan5880 */
	/* Date   : 2025/2/18 12:43am     */
}

// this function is not the real function to get one frame, but to hot start the frameArrived callback. 
bool Scapture::get_one_frame(void** dataPtr, long& _rowpitch, long& _width, long& _height, long& _totalbytes) {
	
	bool result = get_confirmation_that_width_height_rowpitch_remain_the_same_else(dataPtr, _rowpitch, _width, _height, _totalbytes); // if return true, reallocate dataPtr, if false, directly copy
	
	*dataPtr = mappedResource.pData;	
	
	if (mappedResource.pData == nullptr) {
		std::cout << "mappedResource.pData == nullptr. " << std::endl;
		return false;
	}
	if (*dataPtr == nullptr) {
		std::cout << "*dataPtr == nullptr. " << std::endl;
		return false;
	}

	return true;
}

bool Scapture::get_one_frame_fast_version_without_checking_for_width_height_rowpitch_changed(void** dataPtr) {
	
	*dataPtr = OldpDataHolder;
	if (*dataPtr == nullptr) {
		std::cout << "*dataPtr == nullptr. " << std::endl;
		return false;
	}
	return true;
}

bool Scapture::get_confirmation_that_width_height_rowpitch_remain_the_same_else(void** dataPtr, long& _rowpitch, long& _width, long& _height, long& _totalbytes) {

	// due to the nature of this "write thread" and "read thread" thing
	// but then both thread share the same class object instance Scapture declared in global scope of .dll, 
	// I just use this rowpitch compare newrowpitch thing. rowpitch is old rowpitch. newrowpitch is new rowpitch. 
	if (newrowpitch != rowpitch || newwidth != width || newheight != height) {
		printf("rowpitch OR width OR height HAD changed!!!\n");
		//printf("rowpitch: %d, newrowpitch: %d, width: %d, newwidth: %d, height: %d, newheight: %d, totalbytes: %d \n", rowpitch, newrowpitch, width, newwidth, height, newheight, totalbytes);
		totalbytes = newheight * newrowpitch; // this is the correct calculation (might be wrong, but )
		*dataPtr = new BYTE[totalbytes]; // we need to resize the *dataPtr // not sure if we need to delete the previous one tho // im bad at C++
		heightrowpitch = totalbytes; // this is redundant variable ignore it. 
		_rowpitch = newrowpitch; // _rowpitch is pass by ref, we need these values in our python caller. 
		_width = newwidth; // _width is pass by ref, we need these values in our python caller. 
		_height = newheight; // _height is pass by ref, we need these values in our python caller. 
		_totalbytes = totalbytes; // _totalbytes is pass by ref, we need these values in our python caller. 
		rowpitch = newrowpitch; // important
		width = newwidth; // important
		height = newheight; // important
		//printf("_rowpitch: %d, _width: %d, _height: %d, _totalbytes: %d \n", _rowpitch, _width, _height, _totalbytes);
	}
	return true;
}

void Scapture::donothing(void** dataPtr) {
	std::cout << "literally doing nothing. Scapture::donothing" << std::endl;
}

void Scapture::setevent() { // ok this function is important to stop the foreverloop in our framearrived main function do not remove
	SetEvent(resourceReadyEvent); // Signal the event
}
