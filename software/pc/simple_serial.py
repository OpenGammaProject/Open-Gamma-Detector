#!/usr/bin/env python
##
##  Prints the average cps over the time event_refresh.
##  OpenSC, 2021, phnx.
##

import serial
import time

refresh_rate = 1 #Hz
event_refresh = 2 #s

events = 0
last_reset = time.time()

ser = serial.Serial("COM3", 2000000)
ser.flushInput()

while True:
    try:
        ser_bytes = ser.readline()

        events += 1

        if time.time() - last_reset >= event_refresh:
            print(str(round(events / (time.time() - last_reset),2)) + " cps")
            events = 0
            last_reset = time.time()
        
    except KeyboardInterrupt:
        print("Exit.")
        ser.flushInput()
        ser.close()
        break
    except:
        continue
