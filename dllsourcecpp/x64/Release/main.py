import ctypes.wintypes
import os
import numpy as np
import cv2
from time import perf_counter
import time
import ctypes as ct

dll_path = os.path.join(os.path.dirname(__file__), "dllsourcecpp.dll")
my_dll = ctypes.WinDLL(dll_path)

#wincom
class Rect(ct.Structure):
    _fields_ = [("x", ct.c_int), ("y", ct.c_int), ("width", ct.c_int), ("height", ct.c_int)]

class SimpleCapture:
    def __init__(self):        
        self.data_ptr = ctypes.POINTER(ctypes.c_ubyte)()
        self.rowpitch = ctypes.c_long()
        self.x = ctypes.c_int()
        self.y = ctypes.c_int()
        self.width = ctypes.c_long()
        self.height = ctypes.c_long()
        self.totalbytes = ctypes.c_long()
        self.bytes_per_pixel = 4
        
        # wincom        
        self.lib = my_dll # wincom
        self._started = False
        self._buffer = None
        self._size = 0
        self._capture_bounds = Rect() # wincom
        
    def StartCapture(self):
        vectorindex = my_dll.StartCapture()
    
    def InitiateReadPixelsThread(self):
        my_dll.InitiateReadPixelsThread()
            
    def StopReadPixelsThread(self):
        vectorindex = my_dll.StopReadPixelsThread()

    def GetXYWidthHeightRowpitchTotalBytes(self):
        my_dll.GetXYWidthHeightRowpitchTotalBytes(
            ctypes.byref(self.data_ptr), 
            ctypes.byref(self.x), 
            ctypes.byref(self.y), 
            ctypes.byref(self.width), 
            ctypes.byref(self.height), 
            ctypes.byref(self.rowpitch), 
            ctypes.byref(self.totalbytes))
        print(f'{self.x.value=} {self.y.value=} {self.width.value=} {self.height.value=} {self.rowpitch.value=} {self.totalbytes.value=}')
    
    def ReadBuffer(self):
        if my_dll.ReadBuffer(ctypes.byref(self.data_ptr)):
            buffer_shape_1D = (self.totalbytes.value,)
            np_array_1D = np.ctypeslib.as_array(self.data_ptr, shape=buffer_shape_1D)
            image_shape_3D = (self.height.value, self.rowpitch.value // 4, 4) # (height, width, channels)
            image = np_array_1D.reshape(image_shape_3D)
            image = image[:self.height.value-7, :self.width.value-14, :3]
            return image        
        return None

# 1. Initialize SimpleCapture
simplecapture = SimpleCapture()

# 2. call the .dll StartCapture() function, this will create all the FramePool, Session, d3dDevice, d3dContext, everything. 
simplecapture.StartCapture()
time.sleep(1)

# 3. We somehow need these variables in our Python to construct our numpy image. 
simplecapture.GetXYWidthHeightRowpitchTotalBytes()

# 4. We start the Read Thread so it will keep looping and copying frames from our GPU -> CPU. 
simplecapture.InitiateReadPixelsThread()
time.sleep(1)

# 5. We demonstrate for 15 seconds, you can check your task manager for CPU and GPU usage. 
start=perf_counter()
while perf_counter() - start < 20:
    now=perf_counter()
    a = simplecapture.ReadBuffer() # You can time this function call here, it should be about 0.000070 seconds per call
    print(f'{perf_counter()-now:.10f}')
    # if a is not None:
    #     cv2.imshow('a', a)
    #     cv2.waitKey(1)
    #     time.sleep(.0500) # We throttle our function call to prevent CPU/GPU go crazy. 
    # else:
    #     print(f'a is None!')
    #     time.sleep(.0100)
cv2.destroyAllWindows()

# 6. And finally, we stop the Read Thread loop. 
simplecapture.StopReadPixelsThread()