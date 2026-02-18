import serial
import time

s = serial.Serial('COM3', 115200, timeout=1)

# Reset device via DTR/RTS toggle
s.dtr = False
s.rts = True
time.sleep(0.1)
s.rts = False
time.sleep(0.1)
s.dtr = False

start = time.time()
try:
    while time.time() - start < 90:
        line = s.readline().decode('utf-8', 'replace').rstrip()
        if line:
            print(line, flush=True)
finally:
    s.close()
    print("--- Monitor ended ---")
