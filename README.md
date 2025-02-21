# winrt_demo
Windows Graphics Capture

Demo: A very fast window capturing method using WinRT Windows Graphics Capture (C++ .dll + Python ctypes)

Windows provides a very fast screen capturing method using WGC (Windows Graphics Capture). Theoretically, you can only grab frames as fast as your system's refresh rate supports, i.e., 60 fps to 240 fps.

Frame rate examples: 

60 fps: 0.016666 ms per frame 

240 fps: 0.004166 ms per frame

Initially, the frames that arrive are stored in the GPU. You need to copy them from GPU to CPU. This operation takes around 0.0008 to 0.0018 seconds, depending on how you write the GPU to CPU code.

I managed to do it in 0.0008 to 0.0009 seconds by omitting the memcpy() step and only calling m_d3dContext->Map() once.

Not only that, imagine frames arriving in the background. When the user requests a frame, and the system copies it from GPU to CPU, then 0.0008 seconds is wasted each time while waiting for the operation to complete.

What if we can create a background thread to do the copying for us in the background? That way, when the user requests a frame, the GPU to CPU step is already done beforehand. It's like the meal is already cooked in the kitchen. Now, the waiter just needs to simply deliver the meal to the customer. Now you don't need to wait 0.0008 seconds for the frame; you can just grab it from the CPU memory instantly.

However, the drawbacks for this approach is that it consumes both CPU and GPU resources heavily all the time. So, this probably is not a good design for most applications. But, if you have a special use case where you need to grab the newest frame as fast as you possibly can, then this approach may be good for you.

References: 

Special thanks to https://github.com/lovettchris/wincam, an amazing open-sourced Python module that you can just install and use with pip install wincam.


Technical Details: 

C/C++ -> Additional Include Directories:
- C:\boost

Linker --> Input:
- d3d11.lib
- dxgi.lib

NuGet Package:
- Microsoft.Windows.CppWinRT 2.0.240405.15
- Microsoft.Windows.ImplementationLibrary 1.0.240803.1

Configuration Properties:
- Windows SDK Version --> 10.0 (latest installed version)
- Platform Toolset --> Visual Studio 2022 (v143)
- C++ Language Standard --> ISO C++20 Standard (/std:c++20)
- C Language Standard --> ISO C17 (2018) Standard (/std:c17)

Others:
- Scapture.h/Scapture.cpp was my implementation before I found wincom. 
- wincom has very nice code, it is very clean, very easily understandable, so I switched to their way of doing things. (SimpleCapture.h/SimpleCapture.cpp)

How to run:
- just run python main.py
- change anything you want to change in the C++ code
- change anything you want in the main.py
- https://www.youtube.com/watch?v=YWxk4RHomMg
- Conclusion: 
    - FrameArrived running in the background on the GPU.
    - ReadPixelsThread running in the background to copy frame from GPU->CPU every x second. 
    - Python just grab the newest frame directly from the memory pointer, no time wasted on waiting. 