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
filename = "out.csv"
plot = True
refresh_rate = 2 # Live Plot Hz
avg_reset = 20 # s
## END USER CONFIG

if plot:
    plt.ion()
    plt.figure("Scintillation Counter")

ser = serial.Serial("COM3", 2000000)
ser.flushInput()

avg_count = 0
avg_t = 0
nano = time.time_ns() # Get ns start
start = time.time() # Get s start
rt = 1/refresh_rate
last_ref = start
last_avg_refr = time.time()

values = {}
i = 0
imax = 3.3
decimal = 3

while i < imax:
    i += 1/pow(10,decimal)
    values[round(i,3)] = 0

with open(filename,"a") as f:
            writer = csv.writer(f, delimiter=";")
            writer.writerow(["### Begin Measurement Data ###"])
            writer.writerow([time.time(),nano])

while True:
    try:
        ser_bytes = ser.readline()

        now = time.time_ns()
        dt = now - nano
        nano = now
        
        t = avg_count * avg_t + dt
        avg_count += 1
        avg_t = t / avg_count

        data = float(ser_bytes.decode("utf-8"))

        delta_time = time.time() - start

        print(f"{round(data,3)} V: {round((1 / avg_t) * 1000000000,2)} cps --- {round(delta_time,1)}s")

        if time.time() - last_avg_refr >= avg_reset:
            last_avg_refr = time.time()
            avg_count = 0

        values[round(data,3)] += 1
        
        with open(filename,"a",newline="") as f:
            writer = csv.writer(f, delimiter=";")
            writer.writerow([time.time_ns(),dt,data])
    
        if plot and time.time() - last_ref >= rt:
            last_ref = time.time()

            print_data = []

            for key in values:
                print_data += [ round(values[key]/delta_time,5) ]
            
            plt.plot(print_data, linewidth=0.3, marker=",")
            plt.title("Live Pulse Spectrum")
            #plt.xscale('log')
            #plt.yscale('log')
            #plt.ylim([0,.2])
            plt.draw()
            plt.pause(0.0001)
            plt.clf()
        
    except KeyboardInterrupt:
        print("Exit.")
        ser.flushInput()
        ser.close()
        break
    except:
        continue
