#!/usr/bin/env python
##
##  First test in evaluation software.
##  This will connect to serial and grab all the data.
##

import csv
import matplotlib.pyplot as plt

filename = "am241-1.csv"

#dataarr = []
#plt.ion()
plt.figure("Test Plot")

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

plt.plot(print_data, label="Some Test", linewidth=0.05, marker=",")
plt.title("Some Title")
#plt.xscale('log')
#plt.yscale('log')
#plt.ylim([0,20])
#plt.xlim([0,3300])
plt.show()
