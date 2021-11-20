#!/usr/bin/env python
##
##  This will use the serial input and output the data to csv.
##  Optional live plot of the spectrum included.
##  2021

import serial
import time
import csv
import matplotlib.pyplot as plt

## BEGIN USER CONFIG
filename = "bg-pico.csv"
plot = True
refresh_rate = 1 # Live Plot Hz
avg_reset = 20 # s
## END USER CONFIG

if plot:
    plt.ion()
    fig = plt.figure("Scintillation Counter")
    plt.title("Live Pulse Spectrum")

ser = serial.Serial("COM4", 2000000)
ser.flushInput()

avg_count = 0
avg_t = 0
nano = time.time_ns() # Get ns start
start = time.time() # Get s start
rt = 1/refresh_rate
last_ref = start
last_avg_refr = time.time()

values = [0] * 4096

"""
with open(filename,"a") as f:
            writer = csv.writer(f, delimiter=";")
            writer.writerow(["### Begin Measurement Data ###"])
            writer.writerow([time.time(),nano])
"""

while True:
    try:
        # 30 Min Measurement
        if time.time() - start >= 1800:
            break
        
        ser_bytes = ser.readline()

        now = time.time_ns()
        dt = now - nano
        nano = now
        
        t = avg_count * avg_t + dt
        avg_count += 1
        avg_t = t / avg_count

        data = int(ser_bytes.decode("utf-8"))

        delta_time = time.time() - start

        print(f"{round(data/4095*3.3,3)} V: {round((1 / avg_t) * 1000000000,2)} cps --- {round(delta_time,1)}s")
        
        if time.time() - last_avg_refr >= avg_reset:
            last_avg_refr = time.time()
            avg_count = 0

        values[data] += 1
        
        with open(filename,"a",newline="") as f:
            writer = csv.writer(f, delimiter=";")
            writer.writerow([time.time_ns(),dt,data])
    
        if plot and time.time() - last_ref >= rt:
            last_ref = time.time()

            print_data = []

            for val in values:
                print_data += [ round(val/delta_time,5) ]
            
            plt.plot(print_data, linewidth=0.3, marker=",")
            #plt.hist(print_data, bins=100)
            #plt.xscale('log')
            #plt.yscale('log')
            #plt.ylim([0,.2])
            plt.draw()
            plt.pause(1e-17)
            plt.clf()
        
    except KeyboardInterrupt:
        print("Exit.")
        ser.flushInput()
        ser.close()
        break
    except:
        continue
