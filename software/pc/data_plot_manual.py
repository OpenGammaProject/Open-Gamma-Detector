#!/usr/bin/env python
##
##  This will use the output data from serial_data.py and
##  plot an histogram.
##  OpenSC, 2020, phnx.

import csv
import matplotlib.pyplot as plt

filename = "out.csv"

plt.figure("OpenSC Histogram")

values = {}

imin = 0.0
imax = 3.3
decimal = 3

i = imin

while i < imax:
    i += 1/pow(10,decimal)
    values[round(i,3)] = 0

with open(filename,"r") as file:
    reader = csv.reader(file, delimiter=";")
    for row in reader:
        if len(row) > 2:
            data = float(row[2])
            if data >= imin and data <= imax:
                values[round(data,3)] += 1
            
print_data = []

for key in values:
    print_data += [ values[key] ]

plt.plot(print_data, label="", linewidth=0.05, marker=",")
plt.title("OpenSC Histogram")
#plt.xscale('log')
#plt.yscale('log')
#plt.ylim([0,20])
#plt.xlim([0,3300])
plt.show()
