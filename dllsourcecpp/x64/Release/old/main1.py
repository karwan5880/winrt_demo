import ctypes.wintypes
import os
import numpy as np
import cv2
from time import perf_counter
import time

dll_path = os.path.join(os.path.dirname(__file__), "dllsourcecpp.dll")
my_dll = ctypes.WinDLL(dll_path)

my_dll.IWantOneFrame.argtypes = [ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte)), ctypes.POINTER(ctypes.c_long), ctypes.POINTER(ctypes.c_long), ctypes.POINTER(ctypes.c_long), ctypes.POINTER(ctypes.c_long)]
my_dll.IWantOneFrame.restype = ctypes.c_bool
my_dll.IWantOneFrameStraight.argtypes = [ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte))]
my_dll.IWantOneFrameStraight.restype = ctypes.c_bool
my_dll.DoNothing.argtypes = [ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte))]
my_dll.DoNothing.restype = None

class Demo:
    def __init__(self):
        self.data_ptr = ctypes.POINTER(ctypes.c_ubyte)()
        self.rowpitch = ctypes.c_long()
        self.width = ctypes.c_long()
        self.height = ctypes.c_long()
        self.totalbytes = ctypes.c_long()
        self.bytes_per_pixel = 4
    
    def IWantOneFrame(self):
        res = my_dll.IWantOneFrame(ctypes.byref(self.data_ptr), ctypes.byref(self.rowpitch), ctypes.byref(self.width), ctypes.byref(self.height), ctypes.byref(self.totalbytes))
        if res:
            buffer_shape_1D = (self.height.value * self.rowpitch.value,)
            np_array_1D = np.ctypeslib.as_array(self.data_ptr, shape=buffer_shape_1D)            
            image_shape_3D = (self.height.value, self.rowpitch.value // 4, 4) 
            np_image_bgra = np_array_1D.reshape(image_shape_3D)
            img = np_image_bgra
            return img
        else:
            return None

    def IWantOneFrameStraight(self):
        res = my_dll.IWantOneFrameStraight(ctypes.byref(self.data_ptr))
        if res:
            buffer_shape_1D = (self.totalbytes.value,)
            np_array_1D = np.ctypeslib.as_array(self.data_ptr, shape=buffer_shape_1D)
            cv2.imshow('np_array_1D', np_array_1D)
            cv2.waitKey(0)
            cv2.destroyAllWindows()
            print(f'{self.rowpitch.value//4=} {self.rowpitch.value/4=}')
            image_shape_3D = (self.height.value, self.rowpitch.value // 4, 4)
            img = np_array_1D.reshape(image_shape_3D)
            cv2.imshow('img', img)
            cv2.waitKey(0)
            cv2.destroyAllWindows()
            #
            print(f'befor: {img.shape=}')
            # img = img[:self.height.value,:self.width.value]
            print(f'after: {img.shape=}')
            #
            return img
        else:
            return None
    
    def DoNothing(self):
        my_dll.DoNothing(ctypes.byref(self.data_ptr))
    
    def StartLoop(self):
        my_dll.StartLoop()
    
    def StopCapturing(self):
        my_dll.StopCapturing()

demo = Demo()

demo.StartLoop() # Here We Initialize And Start The Write Thread. 

time.sleep(1) # In Case The Computer Is Slow. 

for i in range(20): # Hot Start 20 Loops For the StagingTexture[10] And Correctness Of Width Height Rowpitch. 
    img = demo.IWantOneFrame()
    if img is not None:
        print(f'Valid Image Constructed!')
        # cv2.imshow('img', img)
        # cv2.waitKey(1)
        # time.sleep(.050)
    else:
        print(f'Image Is Null!')
cv2.destroyAllWindows()

time.sleep(1)

print(f'\n\n')

loop=0
failed_counter=0
total=0
start=perf_counter()

while perf_counter()-start < 1:
    
    now=perf_counter()
    
    img = demo.IWantOneFrameStraight()
    
    end=perf_counter()-now
    
    total+=end
    
    print(f'time={end:.10f} {loop=}')
    
    if img is not None:
        print(f'Valid Image Constructed!')
        cv2.imshow('img', img)
        cv2.waitKey(0)
        cv2.destroyAllWindows()
        loop+=1
    else:
        print(f'Image Is Null!')
        failed_counter+=1
        
cv2.destroyAllWindows()

average=total/loop

fps = 1 / average

print(f'loops={loop} average={average:.10f}; fps={fps} failed_counter={failed_counter}')

print(f'{demo.width=} {demo.height=} {demo.totalbytes=} {demo.rowpitch=}')

for i in range(1):
    demo.DoNothing()
    time.sleep(1)
    
demo.StopCapturing()




import win32gui
import win32process
import psutil


width = 0
height = 0

notepad_pid = None
for proc in psutil.process_iter(['pid', 'name']):
    if proc.info['name'].lower() == 'notepad.exe':
        notepad_pid = proc.info['pid']
        break

print(f'{notepad_pid=}')

if notepad_pid:
    hwnd = None
    try:
        hwnd = win32gui.FindWindow(None, "Untitled - Notepad") # Default title, might need adjustment
        print(f'no windows name as "Untitled - Notepad"')
        try: # Try finding by process ID if window title is not standard
            def callback(current_hwnd, hwnds):
                _, process_id = win32process.GetWindowThreadProcessId(current_hwnd)
                # print(f'{current_hwnd=} {process_id=} {notepad_pid=}')
                if process_id == notepad_pid:
                    hwnds.append(current_hwnd)
                    print(f'found {current_hwnd=}')
                return True

            hwnds = []
            win32gui.EnumWindows(callback, hwnds)
            if hwnds:
                hwnd = hwnds[0] # Take the first found window                
        except win32gui.error:
            print(f'still no window found')
            hwnd = None  # Still no window found
    except win32gui.error:
        print(f'win32gui.error. ')

    if hwnd:
        rect = win32gui.GetWindowRect(hwnd)
        width = rect[2] - rect[0]
        height = rect[3] - rect[1]
        # return width, height
        print(f'hwnd {width=} {height=}')
    else:
        # return None, None  # Window handle not found
        pass
else:
    # return None, None  # Notepad process not found
    pass

print(f'{width=} {height=}')