from wincam import DXCamera
from time import perf_counter
import time

x=10
y=10
w=800
h=600

with DXCamera(x, y, w, h, fps=1000) as camera:
    while True:
        now = perf_counter()
        frame, timestamp = camera.get_bgr_frame()
        end = perf_counter() - now
        print(f'{end:.10f} {timestamp:.10f}')




